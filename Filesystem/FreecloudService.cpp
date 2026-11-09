#include "FreecloudService.h"

#include "Network/ICloudStorage.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <algorithm>
#include <exception>
#include <utility>

FreecloudService::FreecloudService(ICloudStorage* storage, EncryptionAlgorithmFactory factory, QObject* parent)
	: QObject(parent)
	, cloudStorage(storage)
	, encryptionFactory(std::move(factory))
	, storageFolder(QDir(QCoreApplication::applicationDirPath()).filePath("Freecloud"))
{
	cloudStorage->setParent(this);
	connect(cloudStorage, &ICloudStorage::ConfigurationChanged, this, &FreecloudService::StartPendingUpload);
	connect(cloudStorage, &ICloudStorage::UploadFinished, this, &FreecloudService::OnUploadFinished);
	connect(cloudStorage, &ICloudStorage::UploadProgress, this, &FreecloudService::OnUploadProgress);
	connect(cloudStorage, &ICloudStorage::DownloadFinished, this, &FreecloudService::OnDownloadFinished);
	connect(cloudStorage, &ICloudStorage::DownloadProgress, this, &FreecloudService::OnDownloadProgress);
	connect(cloudStorage, &ICloudStorage::DeleteProgress, this, &FreecloudService::OnDeleteProgress);
	connect(cloudStorage, &ICloudStorage::DeleteFinished, this, &FreecloudService::OnDeleteFinished);
}

FreecloudService::~FreecloudService()
{
	delete downloadFile;
}

const QList<FreecloudService::FileMetadata>& FreecloudService::GetFiles() const
{
	return files;
}

const QList<FreecloudService::DirectoryMetadata>& FreecloudService::GetDirectories() const
{
	return directories;
}

FreecloudService::Operation FreecloudService::GetOperation() const
{
	return operation;
}

bool FreecloudService::IsPaused() const
{
	return paused;
}

QString FreecloudService::GetActiveFileId() const
{
	return activeFileIdx >= 0 ? files[activeFileIdx].id : QString{};
}

int FreecloudService::GetTransferProgress() const
{
	if (transferTotalBytes <= 0) {
		return operation == Operation::None ? 0 : 100;
	}

	return std::clamp<int>(transferredBytes * 100 / transferTotalBytes, 0, 100);
}

QString FreecloudService::GetStorageFolder() const
{
	return storageFolder;
}

void FreecloudService::SetStorageFolder(const QString& value)
{
	storageFolder = value;
}

void FreecloudService::Pause()
{
	if (paused || (operation != Operation::Upload && operation != Operation::Download)) {
		return;
	}

	cloudStorage->Pause();
	paused = true;
	emit ViewChanged();
	emit StateChanged(false);
}

void FreecloudService::Resume()
{
	if (!paused) {
		return;
	}

	if (restoredTransfer) {
		const auto restoredOperation = operation;
		const auto id = GetActiveFileId();
		operation = Operation::None;
		paused = false;
		restoredTransfer = false;
		if (restoredOperation == Operation::Upload) {
			StartUpload(GetFileIdx(id));
		} else {
			DownloadFile(id);
		}

		return;
	}

	cloudStorage->Resume();
	paused = false;
	emit ViewChanged();
	emit StateChanged(false);
}

void FreecloudService::StopEntries(const QStringList& fileIds, const QStringList& directoryIds)
{
	bool stopActive = false;
	if ((operation == Operation::Upload || operation == Operation::Download) && activeFileIdx >= 0) {
		const auto& activeFile = files[activeFileIdx];
		stopActive = fileIds.contains(activeFile.id)
			|| IsDirectorySelectedOrDescendant(activeFile.parentId, directoryIds);
	}

	for (const auto& file : files) {
		if (!fileIds.contains(file.id)
			&& !IsDirectorySelectedOrDescendant(file.parentId, directoryIds)) {
			continue;
		}

		pendingDownloadIds.removeAll(file.id);
		if (!file.localPath.isEmpty()) {
			QFile::remove(file.localPath + ".freecloud.part");
		}
		if (HasMissingChunks(file.id) && !stoppedUploadIds.contains(file.id)) {
			stoppedUploadIds.append(file.id);
		}
	}

	if (stopActive) {
		cloudStorage->Stop();
		if (downloadFile) {
			downloadFile->close();
			delete downloadFile;
			downloadFile = nullptr;
		}

		if (operation == Operation::Download && !files[activeFileIdx].localPath.isEmpty()) {
			QFile::remove(files[activeFileIdx].localPath + ".freecloud.part");
		}

		operation = Operation::None;
		paused = false;
		restoredTransfer = false;
		activeFileIdx = -1;
		activeChunkIdx = -1;
		transferredBytes = 0;
		transferTotalBytes = 0;
		openAfterDownload = false;
	}

	emit StateChanged(false);
	emit ViewChanged();
	if (stopActive && !pendingDownloadIds.isEmpty()) {
		DownloadFile(pendingDownloadIds.takeFirst());
	} else if (stopActive) {
		StartPendingUpload();
	}
}

QJsonObject FreecloudService::SaveState() const
{
	QJsonObject state;
	state["Encryption"] = encryptionFactory.GetAlgorithm() == EncryptionAlgorithmFactory::Algorithm::Aes
		? "AES-256-GCM" : QString{};
	state["StorageFolder"] = storageFolder;
	QString operationValue;
	if (operation == Operation::Upload) {
		operationValue = "Upload";
	} else if (operation == Operation::Download) {
		operationValue = "Download";
	}

	state["Operation"] = operationValue;
	state["ActiveFileId"] = GetActiveFileId();
	state["Paused"] = paused;
	QJsonArray pendingDownloadValues;
	for (const auto& id : pendingDownloadIds) {
		pendingDownloadValues.append(id);
	}

	state["PendingDownloadIds"] = pendingDownloadValues;
	QJsonArray stoppedUploadValues;
	for (const auto& id : stoppedUploadIds) {
		stoppedUploadValues.append(id);
	}

	state["StoppedUploadIds"] = stoppedUploadValues;
	QJsonArray fileValues;
	for (const auto& file : files) {
		QJsonObject fileValue{
			{"Id", file.id}, {"Name", file.name}, {"LocalPath", file.localPath}, {"ParentId", file.parentId},
			{"Size", file.size}
		};
		QJsonArray chunkValues;
		for (const auto& chunk : file.chunks) {
			chunkValues.append(QJsonObject{
				{"Index", chunk.idx}, {"PlainSize", chunk.plainSize},
				{"Key", QString::fromLatin1(chunk.key.toBase64())}, {"RemoteId", chunk.remoteId},
				{"DeletionId", chunk.deletionId}
			});
		}

		fileValue["Chunks"] = chunkValues;
		fileValues.append(fileValue);
	}

	state["Files"] = fileValues;
	QJsonArray directoryValues;
	for (const auto& directory : directories) {
		directoryValues.append(QJsonObject{
			{"Id", directory.id}, {"Name", directory.name}, {"LocalPath", directory.localPath},
			{"ParentId", directory.parentId}
		});
	}

	state["Directories"] = directoryValues;
	return state;
}

void FreecloudService::LoadState(const QJsonObject& state)
{
	storageFolder = state["StorageFolder"].toString(storageFolder);
	files.clear();
	for (const auto& fileValue : state["Files"].toArray()) {
		const auto fileObject = fileValue.toObject();
		FileMetadata file;
		file.id = fileObject["Id"].toString();
		file.name = fileObject["Name"].toString();
		file.localPath = fileObject["LocalPath"].toString();
		file.parentId = fileObject["ParentId"].toString();
		file.size = fileObject["Size"].toInteger();
		for (const auto& chunkValue : fileObject["Chunks"].toArray()) {
			const auto chunkObject = chunkValue.toObject();
			file.chunks.append({chunkObject["Index"].toInt(), chunkObject["PlainSize"].toInteger(),
				QByteArray::fromBase64(chunkObject["Key"].toString().toLatin1()),
				chunkObject["RemoteId"].toString(), chunkObject["DeletionId"].toString()});
		}

		if (!file.id.isEmpty() && !file.name.isEmpty() && !file.chunks.isEmpty()) {
			files.append(file);
		}
	}

	directories.clear();
	for (const auto& directoryValue : state["Directories"].toArray()) {
		const auto directoryObject = directoryValue.toObject();
		DirectoryMetadata directory{directoryObject["Id"].toString(), directoryObject["Name"].toString(),
			directoryObject["LocalPath"].toString(), directoryObject["ParentId"].toString()};
		if (!directory.id.isEmpty() && !directory.name.isEmpty()) {
			directories.append(directory);
		}
	}

	pendingDownloadIds.clear();
	for (const auto& value : state["PendingDownloadIds"].toArray()) {
		const auto id = value.toString();
		if (GetFileIdx(id) >= 0) {
			pendingDownloadIds.append(id);
		}
	}

	stoppedUploadIds.clear();
	for (const auto& value : state["StoppedUploadIds"].toArray()) {
		const auto id = value.toString();
		if (GetFileIdx(id) >= 0) {
			stoppedUploadIds.append(id);
		}
	}

	const auto operationValue = state["Operation"].toString();
	const int restoredFileIdx = GetFileIdx(state["ActiveFileId"].toString());
	const bool restoreUpload = operationValue == "Upload";
	const bool restoreDownload = operationValue == "Download";
	if (restoredFileIdx >= 0 && (restoreUpload || restoreDownload)) {
		if (state["Paused"].toBool()) {
			activeFileIdx = restoredFileIdx;
			activeChunkIdx = -1;
			operation = restoreUpload ? Operation::Upload : Operation::Download;
			paused = true;
			restoredTransfer = true;
			qint64 completed = 0;
			if (restoreUpload) {
				for (const auto& chunk : files[activeFileIdx].chunks) {
					if (!chunk.remoteId.isEmpty()) {
						completed += chunk.plainSize;
					}
				}
			} else {
				completed = QFileInfo(files[activeFileIdx].localPath + ".freecloud.part").size();
			}

			transferredBytes = completed;
			transferTotalBytes = files[activeFileIdx].size;
			emit ViewChanged();
		} else if (restoreUpload) {
			StartUpload(restoredFileIdx);
		} else {
			DownloadFile(files[restoredFileIdx].id);
		}
	} else {
		emit ViewChanged();
		StartPendingUpload();
	}
}

QString FreecloudService::AddFile(const QString& path, const QString& parentId)
{
	AddFileMetadata(path, parentId);
	const auto id = files.back().id;
	emit StateChanged();
	StartPendingUpload();
	return id;
}

QString FreecloudService::AddFolder(const QString& path, const QString& parentId)
{
	const auto id = AddFolderMetadata(path, parentId);
	emit StateChanged();
	StartPendingUpload();
	return id;
}

QString FreecloudService::AddDirectory(const QString& name, const QString& parentId)
{
	const auto id = AddDirectoryMetadata(name, parentId);
	emit StateChanged();
	return id;
}

void FreecloudService::UpdateParents(const QHash<QString, QString>& fileParents,
	const QHash<QString, QString>& directoryParents)
{
	for (auto& file : files) {
		if (fileParents.contains(file.id)) {
			file.parentId = fileParents.value(file.id);
		}
	}

	for (auto& directory : directories) {
		if (directoryParents.contains(directory.id)) {
			directory.parentId = directoryParents.value(directory.id);
		}
	}

	emit StateChanged();
}

bool FreecloudService::RenameFile(const QString& id, const QString& name, bool renameLocal, QString& error)
{
	const int fileIdx = GetFileIdx(id);
	if (fileIdx < 0 || name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\')) {
		error = "The file name is invalid.";
		return false;
	}

	auto& file = files[fileIdx];
	if (renameLocal && !GetFileLocalPath(id).isEmpty()) {
		const auto newPath = QFileInfo(file.localPath).dir().filePath(name);
		if (QFileInfo::exists(newPath) || !QFile::rename(file.localPath, newPath)) {
			error = "Could not rename the local file.";
			return false;
		}

		file.localPath = newPath;
	}

	file.name = name;
	emit StateChanged();
	return true;
}

bool FreecloudService::RenameDirectory(const QString& id, const QString& name, bool renameLocal, QString& error)
{
	if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\')) {
		error = "The directory name is invalid.";
		return false;
	}

	for (auto& directory : directories) {
		if (directory.id != id) {
			continue;
		}

		const auto oldPath = GetDirectoryLocalPath(id);
		if (renameLocal && !oldPath.isEmpty()) {
			const auto newPath = QFileInfo(oldPath).dir().filePath(name);
			if (QFileInfo::exists(newPath) || !QDir().rename(oldPath, newPath)) {
				error = "Could not rename the local directory.";
				return false;
			}

			const auto oldPrefix = QDir::cleanPath(oldPath) + '/';
			for (auto& file : files) {
				const auto cleanPath = QDir::cleanPath(file.localPath);
				if (cleanPath.startsWith(oldPrefix, Qt::CaseInsensitive)) {
					file.localPath = QDir(newPath).filePath(cleanPath.sliced(oldPrefix.size()));
				}
			}

			for (auto& child : directories) {
				const auto cleanPath = QDir::cleanPath(child.localPath);
				if (cleanPath.startsWith(oldPrefix, Qt::CaseInsensitive)) {
					child.localPath = QDir(newPath).filePath(cleanPath.sliced(oldPrefix.size()));
				}
			}

			directory.localPath = newPath;
		} else if (!oldPath.isEmpty()) {
			directory.localPath = oldPath;
		}

		directory.name = name;
		emit StateChanged();
		return true;
	}

	error = "The directory no longer exists in Freecloud.";
	return false;
}

void FreecloudService::DeleteEntries(const QStringList& fileIds, const QStringList& directoryIds,
	DeleteMode mode)
{
	if (operation != Operation::None || (fileIds.isEmpty() && directoryIds.isEmpty())) {
		return;
	}

	if (mode == DeleteMode::Local) {
		bool changed = false;
		for (const auto& id : fileIds) {
			const int fileIdx = GetFileIdx(id);
			if (fileIdx >= 0 && !IsDirectorySelectedOrDescendant(files[fileIdx].parentId, directoryIds)) {
				changed = DeleteLocalFile(id) || changed;
			}
		}

		for (const auto& id : directoryIds) {
			changed = DeleteLocalDirectory(id) || changed;
		}

		if (changed) {
			emit StateChanged(false);
		}

		return;
	}

	pendingDeleteFileIds = fileIds;
	pendingDeleteDirectoryIds = directoryIds;
	pendingDeleteMode = mode;
	operation = Operation::Delete;
	emit ViewChanged();
	if (!cloudStorage->CanDelete()) {
		FinishDeletion();
		emit Error("Configure cloud storage before deleting remote files.");
		return;
	}

	QStringList deletionIds;
	for (const auto& file : files) {
		if (!fileIds.contains(file.id) && !IsDirectorySelectedOrDescendant(file.parentId, directoryIds)) {
			continue;
		}

		for (const auto& chunk : file.chunks) {
			if (!chunk.remoteId.isEmpty() && chunk.deletionId.isEmpty()) {
				FinishDeletion();
				emit Error("The selection contains files without remote deletion metadata.");
				return;
			}

			if (!chunk.deletionId.isEmpty()) {
				deletionIds.append(chunk.deletionId);
			}
		}
	}

	cloudStorage->DeleteFiles(deletionIds);
}

bool FreecloudService::IsFileLocal(const QString& id) const
{
	const int fileIdx = GetFileIdx(id);
	if (fileIdx < 0) {
		return false;
	}

	const QFileInfo localFile(files[fileIdx].localPath);
	return localFile.isFile() && localFile.size() == files[fileIdx].size;
}

bool FreecloudService::IsFileCloud(const QString& id) const
{
	return !HasMissingChunks(id);
}

bool FreecloudService::IsDirectoryLocal(const QString& id) const
{
	bool hasFiles = false;
	for (const auto& file : files) {
		if (!IsDirectoryDescendant(file.parentId, id)) {
			continue;
		}

		hasFiles = true;
		if (!IsFileLocal(file.id)) {
			return false;
		}
	}

	return hasFiles || !GetDirectoryLocalPath(id).isEmpty();
}

bool FreecloudService::IsDirectoryCloud(const QString& id) const
{
	for (const auto& file : files) {
		if (IsDirectoryDescendant(file.parentId, id) && HasMissingChunks(file.id)) {
			return false;
		}
	}

	return true;
}

FreecloudService::TransferState FreecloudService::GetFileTransferState(const QString& id) const
{
	if (operation == Operation::Delete) {
		const int fileIdx = GetFileIdx(id);
		if (pendingDeleteFileIds.contains(id) || (fileIdx >= 0
			&& IsDirectorySelectedOrDescendant(files[fileIdx].parentId, pendingDeleteDirectoryIds))) {
			return transferredBytes == 0 ? TransferState::PendingDelete : TransferState::Deleting;
		}
	}

	if (id == GetActiveFileId()) {
		if (operation == Operation::Upload) {
			return TransferState::Uploading;
		}

		if (operation == Operation::Download) {
			return TransferState::Downloading;
		}
	}

	if (pendingDownloadIds.contains(id)) {
		return TransferState::PendingDownload;
	}

	return HasMissingChunks(id) && IsFileLocal(id) && !stoppedUploadIds.contains(id)
		? TransferState::PendingUpload : TransferState::None;
}

FreecloudService::TransferState FreecloudService::GetDirectoryTransferState(const QString& id) const
{
	if (operation == Operation::Delete
		&& IsDirectorySelectedOrDescendant(id, pendingDeleteDirectoryIds)) {
		return transferredBytes == 0 ? TransferState::PendingDelete : TransferState::Deleting;
	}

	const auto activeFileId = GetActiveFileId();
	const int activeIdx = GetFileIdx(activeFileId);
	if (activeIdx >= 0 && IsDirectoryDescendant(files[activeIdx].parentId, id)) {
		if (operation == Operation::Upload) {
			return TransferState::Uploading;
		}

		if (operation == Operation::Download) {
			return TransferState::Downloading;
		}
	}

	for (const auto& pendingId : pendingDownloadIds) {
		const int fileIdx = GetFileIdx(pendingId);
		if (fileIdx >= 0 && IsDirectoryDescendant(files[fileIdx].parentId, id)) {
			return TransferState::PendingDownload;
		}
	}

	for (const auto& file : files) {
		if (IsDirectoryDescendant(file.parentId, id) && HasMissingChunks(file.id) && IsFileLocal(file.id)
			&& !stoppedUploadIds.contains(file.id)) {
			return TransferState::PendingUpload;
		}
	}

	return TransferState::None;
}

QString FreecloudService::GetFileLocalPath(const QString& id) const
{
	const int fileIdx = GetFileIdx(id);
	return fileIdx >= 0 && QFileInfo(files[fileIdx].localPath).isFile() ? files[fileIdx].localPath : QString{};
}

QString FreecloudService::GetDirectoryLocalPath(const QString& id) const
{
	for (const auto& directory : directories) {
		if (directory.id != id) {
			continue;
		}

		if (QFileInfo(directory.localPath).isDir()) {
			return directory.localPath;
		}

		const auto mirroredPath = QDir(storageFolder).filePath(GetRelativeDirectoryPath(id));
		if (QFileInfo(mirroredPath).isDir()) {
			return mirroredPath;
		}

		for (const auto& file : files) {
			if (!QFileInfo(file.localPath).isFile() || !IsDirectoryDescendant(file.parentId, id)) {
				continue;
			}

			QString candidatePath = QFileInfo(file.localPath).absolutePath();
			QString directoryId = file.parentId;
			QSet<QString> visitedIds;
			while (!directoryId.isEmpty() && !visitedIds.contains(directoryId)) {
				if (directoryId == id) {
					return QFileInfo(candidatePath).isDir() ? candidatePath : QString{};
				}

				visitedIds.insert(directoryId);
				for (const auto& parent : directories) {
					if (parent.id == directoryId) {
						directoryId = parent.parentId;
						candidatePath = QFileInfo(candidatePath).absolutePath();
						break;
					}
				}
			}
		}
	}

	return {};
}

qint64 FreecloudService::GetDirectorySize(const QString& id) const
{
	qint64 size = 0;
	for (const auto& file : files) {
		QString parentId = file.parentId;
		QSet<QString> visitedIds;
		while (!parentId.isEmpty() && !visitedIds.contains(parentId)) {
			if (parentId == id) {
				size += file.size;
				break;
			}

			visitedIds.insert(parentId);
			QString nextParentId;
			for (const auto& directory : directories) {
				if (directory.id == parentId) {
					nextParentId = directory.parentId;
					break;
				}
			}

			parentId = nextParentId;
		}
	}

	return size;
}

bool FreecloudService::HasMissingChunks(const QString& id) const
{
	const int fileIdx = GetFileIdx(id);
	return fileIdx < 0 || std::any_of(files[fileIdx].chunks.begin(), files[fileIdx].chunks.end(),
		[](const auto& chunk) { return chunk.remoteId.isEmpty(); });
}

void FreecloudService::DownloadFile(const QString& id, bool shouldOpen)
{
	const int fileIdx = GetFileIdx(id);
	if (fileIdx < 0) {
		return;
	}

	if (!cloudStorage->CanDownload()) {
		emit Error("Configure cloud storage first.");
		return;
	}

	if (HasMissingChunks(id)) {
		emit Error("The file has chunks that have not been uploaded yet.");
		return;
	}

	if (operation != Operation::None) {
		if ((operation == Operation::Upload || operation == Operation::Download)
			&& id != GetActiveFileId() && !pendingDownloadIds.contains(id)) {
			pendingDownloadIds.append(id);
			emit StateChanged(false);
			emit ViewChanged();
		}

		return;
	}

	auto& file = files[fileIdx];
	const auto directoryPath = QDir(storageFolder).filePath(GetRelativeDirectoryPath(file.parentId));
	file.localPath = QDir(directoryPath).filePath(file.name);
	if (!QDir().mkpath(QFileInfo(file.localPath).absolutePath())) {
		pendingDownloadIds.clear();
		emit Error("Could not create the storage folder.");
		return;
	}

	const auto partPath = file.localPath + ".freecloud.part";
	downloadFile = new QFile(partPath);
	if (!downloadFile->open(QIODevice::ReadWrite | QIODevice::Append)) {
		const auto error = downloadFile->errorString();
		delete downloadFile;
		downloadFile = nullptr;
		pendingDownloadIds.clear();
		emit Error(error);
		return;
	}

	qint64 expectedBytes = 0;
	activeChunkIdx = 0;
	while (activeChunkIdx < file.chunks.size()
		&& expectedBytes + file.chunks[activeChunkIdx].plainSize <= downloadFile->size()) {
		expectedBytes += file.chunks[activeChunkIdx].plainSize;
		++activeChunkIdx;
	}

	if (downloadFile->size() != expectedBytes) {
		downloadFile->resize(expectedBytes);
	}

	activeFileIdx = fileIdx;
	transferredBytes = expectedBytes;
	transferTotalBytes = file.size;
	operation = Operation::Download;
	paused = false;
	openAfterDownload = shouldOpen;
	emit StateChanged(false);
	emit ViewChanged();
	DownloadNextChunk();
}

void FreecloudService::DownloadDirectory(const QString& id)
{
	if (operation == Operation::Delete) {
		return;
	}

	if (!cloudStorage->CanDownload()) {
		emit Error("Configure cloud storage first.");
		return;
	}

	QStringList downloadIds;
	for (const auto& file : files) {
		if (!IsDirectoryDescendant(file.parentId, id)) {
			continue;
		}

		if (HasMissingChunks(file.id)) {
			emit Error("The directory contains files that have not been uploaded yet.");
			return;
		}

		downloadIds.append(file.id);
	}

	for (const auto& directory : directories) {
		if (!IsDirectoryDescendant(directory.id, id)) {
			continue;
		}

		const auto path = QDir(storageFolder).filePath(GetRelativeDirectoryPath(directory.id));
		if (!QDir().mkpath(path)) {
			emit Error("Could not create the local directory structure.");
			return;
		}
	}

	for (const auto& fileId : downloadIds) {
		if (fileId != GetActiveFileId() && !pendingDownloadIds.contains(fileId)) {
			pendingDownloadIds.append(fileId);
		}
	}

	emit StateChanged(false);
	emit ViewChanged();
	if (operation == Operation::None && !pendingDownloadIds.isEmpty()) {
		DownloadFile(pendingDownloadIds.takeFirst());
	}
}

void FreecloudService::OnUploadFinished(const QString& fileId, const QString& deletionId, const QString& error)
{
	if (operation != Operation::Upload) {
		return;
	}

	if (!error.isEmpty() || fileId.isEmpty()) {
		FinishOperation(error.isEmpty() ? "Cloud storage did not return a file ID." : error);
		return;
	}

	files[activeFileIdx].chunks[activeChunkIdx].remoteId = fileId;
	files[activeFileIdx].chunks[activeChunkIdx].deletionId = deletionId;
	emit StateChanged(false);
	emit ViewChanged();
	UploadNextChunk();
}

void FreecloudService::OnUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
	if (operation == Operation::Upload) {
		UpdateTransferProgress(bytesSent, bytesTotal);
	}
}

void FreecloudService::OnDownloadFinished(const QByteArray& data, const QString& error)
{
	if (operation != Operation::Download) {
		return;
	}

	if (!error.isEmpty()) {
		FinishOperation(error);
		return;
	}

	QByteArray plain;
	QString parseError;
	const auto& file = files[activeFileIdx];
	const auto& chunk = file.chunks[activeChunkIdx];
	try {
		auto encryption = encryptionFactory.Create(EncryptionAlgorithm::Mode::Decrypt, chunk.key);
		plain = encryption->Update(data);
		plain.append(encryption->Finish());
	} catch (const std::exception& exception) {
		FinishOperation(QString::fromUtf8(exception.what()));
		return;
	}

	if (!ParseChunk(plain, file, chunk, plain, parseError)) {
		FinishOperation(parseError);
		return;
	}

	if (downloadFile->write(plain) != plain.size()) {
		FinishOperation(downloadFile->errorString());
		return;
	}

	downloadFile->flush();
	++activeChunkIdx;
	emit ViewChanged();
	DownloadNextChunk();
}

void FreecloudService::OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (operation == Operation::Download) {
		UpdateTransferProgress(bytesReceived, bytesTotal);
	}
}

void FreecloudService::OnDeleteProgress(qint64 filesDeleted, qint64 filesTotal)
{
	if (operation != Operation::Delete || filesTotal <= 0) {
		return;
	}

	transferredBytes = filesDeleted;
	transferTotalBytes = filesTotal;
	emit ViewChanged();
}

void FreecloudService::OnDeleteFinished(const QString& error)
{
	if (operation != Operation::Delete) {
		return;
	}

	FinishDeletion();
	if (!error.isEmpty()) {
		emit Error(error);
	}
}

void FreecloudService::AddFileMetadata(const QString& path, const QString& parentId)
{
	const QFileInfo info(path);
	FileMetadata file;
	file.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	file.name = info.fileName();
	file.localPath = path;
	file.parentId = parentId;
	file.size = info.size();
	const auto chunkCount = std::max<qint64>(1, (file.size + ChunkSize - 1) / ChunkSize);
	for (int idx = 0; idx < chunkCount; ++idx) {
		file.chunks.append({idx, std::min(ChunkSize, std::max<qint64>(0, file.size - idx * ChunkSize)), {}, {}, {}});
	}

	files.append(file);
}

QString FreecloudService::AddFolderMetadata(const QString& path, const QString& parentId)
{
	const QFileInfo folderInfo(path);
	const auto directoryId = AddDirectoryMetadata(folderInfo.fileName(), parentId, path);
	const QDir directory(path);
	const auto entries = directory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
		QDir::DirsFirst | QDir::Name);
	for (const auto& entry : entries) {
		if (entry.isDir()) {
			AddFolderMetadata(entry.absoluteFilePath(), directoryId);
		} else if (entry.isFile()) {
			AddFileMetadata(entry.absoluteFilePath(), directoryId);
		}
	}

	return directoryId;
}

QString FreecloudService::AddDirectoryMetadata(const QString& name, const QString& parentId,
	const QString& localPath)
{
	const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	directories.append({id, name, localPath, parentId});
	return id;
}

bool FreecloudService::DeleteLocalFile(const QString& id)
{
	const int fileIdx = GetFileIdx(id);
	if (fileIdx < 0) {
		return false;
	}

	auto& file = files[fileIdx];
	if (QFileInfo::exists(file.localPath) && !QFile::moveToTrash(file.localPath)) {
		emit Error("Could not delete the local file.");
		return false;
	}

	QFile::remove(file.localPath + ".freecloud.part");
	file.localPath.clear();
	return true;
}

bool FreecloudService::DeleteLocalDirectory(const QString& id)
{
	QStringList paths;
	for (const auto& directory : directories) {
		if (directory.id != id && !IsDirectoryDescendant(directory.id, id)) {
			continue;
		}

		if (QFileInfo(directory.localPath).isDir()) {
			paths.append(QDir::cleanPath(directory.localPath));
		}

		const auto mirroredPath = QDir(storageFolder).filePath(GetRelativeDirectoryPath(directory.id));
		if (QFileInfo(mirroredPath).isDir()) {
			paths.append(QDir::cleanPath(mirroredPath));
		}
	}

	paths.removeDuplicates();
	std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
		return left.size() < right.size();
	});
	for (const auto& path : paths) {
		if (QFileInfo::exists(path) && !QFile::moveToTrash(path)) {
			emit Error("Could not delete a local directory.");
			return false;
		}
	}

	for (auto& file : files) {
		if (file.parentId != id && !IsDirectoryDescendant(file.parentId, id)) {
			continue;
		}

		if (QFileInfo::exists(file.localPath) && !QFile::moveToTrash(file.localPath)) {
			emit Error("Could not delete a local file.");
			return false;
		}

		QFile::remove(file.localPath + ".freecloud.part");
		file.localPath.clear();
	}

	for (auto& directory : directories) {
		if (directory.id == id || IsDirectoryDescendant(directory.id, id)) {
			directory.localPath.clear();
		}
	}

	return true;
}

void FreecloudService::FinishDeletion()
{
	if (pendingDeleteMode == DeleteMode::CloudAndLocal) {
		for (const auto& id : pendingDeleteDirectoryIds) {
			DeleteLocalDirectory(id);
		}

		for (const auto& id : pendingDeleteFileIds) {
			const int fileIdx = GetFileIdx(id);
			if (fileIdx >= 0 && !IsDirectorySelectedOrDescendant(files[fileIdx].parentId,
				pendingDeleteDirectoryIds)) {
				DeleteLocalFile(id);
			}
		}
	}

	for (int idx = files.size() - 1; idx >= 0; --idx) {
		if (pendingDeleteFileIds.contains(files[idx].id)
			|| IsDirectorySelectedOrDescendant(files[idx].parentId, pendingDeleteDirectoryIds)) {
			stoppedUploadIds.removeAll(files[idx].id);
			files.removeAt(idx);
		}
	}

	for (int idx = directories.size() - 1; idx >= 0; --idx) {
		if (IsDirectorySelectedOrDescendant(directories[idx].id, pendingDeleteDirectoryIds)) {
			directories.removeAt(idx);
		}
	}

	operation = Operation::None;
	transferredBytes = 0;
	transferTotalBytes = 0;
	pendingDeleteFileIds.clear();
	pendingDeleteDirectoryIds.clear();
	emit StateChanged();
	emit ViewChanged();
	StartPendingUpload();
}

bool FreecloudService::IsDirectoryDescendant(const QString& id, const QString& ancestorId) const
{
	QString directoryId = id;
	QSet<QString> visitedIds;
	while (!directoryId.isEmpty() && !visitedIds.contains(directoryId)) {
		if (directoryId == ancestorId) {
			return true;
		}

		visitedIds.insert(directoryId);
		QString parentId;
		for (const auto& directory : directories) {
			if (directory.id == directoryId) {
				parentId = directory.parentId;
				break;
			}
		}

		directoryId = parentId;
	}

	return false;
}

bool FreecloudService::IsDirectorySelectedOrDescendant(const QString& id,
	const QStringList& selectedIds) const
{
	for (const auto& selectedId : selectedIds) {
		if (IsDirectoryDescendant(id, selectedId)) {
			return true;
		}
	}

	return false;
}

void FreecloudService::UpdateTransferProgress(qint64 transferred, qint64 total)
{
	if (activeFileIdx < 0 || activeChunkIdx < 0 || total <= 0) {
		return;
	}

	const auto& file = files[activeFileIdx];
	qint64 completed = 0;
	if (operation == Operation::Upload) {
		for (const auto& chunk : file.chunks) {
			if (!chunk.remoteId.isEmpty()) {
				completed += chunk.plainSize;
			}
		}
	} else {
		for (int idx = 0; idx < activeChunkIdx; ++idx) {
			completed += file.chunks[idx].plainSize;
		}
	}

	const auto currentSize = file.chunks[activeChunkIdx].plainSize;
	completed += currentSize * std::clamp(transferred, qint64{0}, total) / total;
	transferredBytes = completed;
	transferTotalBytes = file.size;
	emit ViewChanged();
}

void FreecloudService::StartPendingUpload()
{
	if (operation != Operation::None || !cloudStorage->CanUpload()) {
		return;
	}

	for (int idx = 0; idx < files.size(); ++idx) {
		const auto& file = files[idx];
		if (stoppedUploadIds.contains(file.id)) {
			continue;
		}

		const QFileInfo localFile(file.localPath);
		const bool missingChunks = std::any_of(file.chunks.begin(), file.chunks.end(),
			[](const auto& chunk) { return chunk.remoteId.isEmpty(); });
		if (missingChunks && localFile.isFile() && localFile.size() == file.size) {
			StartUpload(idx);
			return;
		}
	}
}

void FreecloudService::StartUpload(int fileIdx)
{
	if (fileIdx < 0 || operation != Operation::None) {
		return;
	}

	if (GetLocalBytes(files[fileIdx]) < files[fileIdx].size) {
		emit Error("The complete local file is required to upload missing chunks.");
		return;
	}

	activeFileIdx = fileIdx;
	activeChunkIdx = -1;
	transferredBytes = 0;
	transferTotalBytes = files[fileIdx].size;
	operation = Operation::Upload;
	paused = false;
	emit StateChanged(false);
	emit ViewChanged();
	UploadNextChunk();
}

void FreecloudService::UploadNextChunk()
{
	auto& file = files[activeFileIdx];
	for (++activeChunkIdx; activeChunkIdx < file.chunks.size(); ++activeChunkIdx) {
		if (!file.chunks[activeChunkIdx].remoteId.isEmpty()) {
			continue;
		}

		QFile source(file.localPath);
		if (!source.open(QIODevice::ReadOnly) || !source.seek(activeChunkIdx * ChunkSize)) {
			FinishOperation(source.errorString());
			return;
		}

		auto& chunk = file.chunks[activeChunkIdx];
		QByteArray encrypted;
		try {
			if (chunk.key.isEmpty()) {
				chunk.key = encryptionFactory.GenerateKey();
			}

			auto encryption = encryptionFactory.Create(EncryptionAlgorithm::Mode::Encrypt, chunk.key);
			QByteArray metadata;
			QDataStream stream(&metadata, QIODevice::WriteOnly);
			stream << quint32{0x46434348} << quint16{1} << file.id << file.name << static_cast<quint64>(file.size)
				<< static_cast<quint32>(chunk.idx) << static_cast<quint32>(file.chunks.size())
				<< static_cast<quint32>(chunk.plainSize);
			encrypted = encryption->Update(metadata);
			qint64 remaining = chunk.plainSize;
			while (remaining > 0) {
				const auto data = source.read(std::min<qint64>(64 * 1024, remaining));
				if (data.isEmpty()) {
					FinishOperation("The local file changed after it was added.");
					return;
				}

				encrypted.append(encryption->Update(data));
				remaining -= data.size();
			}

			encrypted.append(encryption->Finish());
		} catch (const std::exception& exception) {
			FinishOperation(QString::fromUtf8(exception.what()));
			return;
		}

		emit StateChanged(false);

		UpdateTransferProgress(0, 1);
		cloudStorage->UploadFile(encrypted, QString("%1.%2.fc").arg(file.id).arg(activeChunkIdx));
		return;
	}

	FinishOperation();
}

void FreecloudService::DownloadNextChunk()
{
	const auto& file = files[activeFileIdx];
	if (activeChunkIdx >= file.chunks.size()) {
		const auto partPath = downloadFile->fileName();
		downloadFile->close();
		delete downloadFile;
		downloadFile = nullptr;
		QFile::remove(file.localPath);
		if (!QFile::rename(partPath, file.localPath)) {
			FinishOperation("Could not move the completed file to its destination.");
			return;
		}

		const auto completedPath = file.localPath;
		const bool shouldOpen = openAfterDownload;
		openAfterDownload = false;
		FinishOperation();
		if (shouldOpen) {
			emit FileReady(completedPath);
		}

		return;
	}

	cloudStorage->DownloadFile(file.chunks[activeChunkIdx].remoteId);
}

void FreecloudService::FinishOperation(const QString& error)
{
	const auto completedOperation = operation;
	if (downloadFile) {
		downloadFile->close();
		delete downloadFile;
		downloadFile = nullptr;
	}

	operation = Operation::None;
	paused = false;
	restoredTransfer = false;
	activeFileIdx = -1;
	activeChunkIdx = -1;
	transferredBytes = 0;
	transferTotalBytes = 0;
	if (!error.isEmpty()) {
		pendingDownloadIds.clear();
		openAfterDownload = false;
	}

	emit ViewChanged();
	emit StateChanged(false);
	if (!error.isEmpty()) {
		emit Error(error);
	} else if (!pendingDownloadIds.isEmpty()) {
		DownloadFile(pendingDownloadIds.takeFirst());
	} else if (completedOperation != Operation::None) {
		StartPendingUpload();
	}
}

int FreecloudService::GetFileIdx(const QString& id) const
{
	for (int idx = 0; idx < files.size(); ++idx) {
		if (files[idx].id == id) {
			return idx;
		}
	}

	return -1;
}

QString FreecloudService::GetRelativeDirectoryPath(const QString& id) const
{
	QStringList names;
	QString directoryId = id;
	QSet<QString> visitedIds;
	while (!directoryId.isEmpty() && !visitedIds.contains(directoryId)) {
		visitedIds.insert(directoryId);
		QString parentId;
		for (const auto& directory : directories) {
			if (directory.id == directoryId) {
				names.prepend(directory.name);
				parentId = directory.parentId;
				break;
			}
		}

		directoryId = parentId;
	}

	return names.join('/');
}

qint64 FreecloudService::GetLocalBytes(const FileMetadata& file) const
{
	const QFileInfo complete(file.localPath);
	if (complete.isFile() && complete.size() == file.size) {
		return file.size;
	}

	return QFileInfo(file.localPath + ".freecloud.part").size();
}

bool FreecloudService::ParseChunk(const QByteArray& data, const FileMetadata& file, const ChunkMetadata& chunk,
	QByteArray& payload, QString& error)
{
	QDataStream stream(data);
	quint32 magic = 0;
	quint16 version = 0;
	QString fileId;
	QString fileName;
	quint64 fileSize = 0;
	quint32 chunkIdx = 0;
	quint32 chunkCount = 0;
	QByteArray chunkData;
	stream >> magic >> version >> fileId >> fileName >> fileSize >> chunkIdx >> chunkCount >> chunkData;
	if (stream.status() != QDataStream::Ok || magic != 0x46434348 || version != 1 || fileId != file.id
		|| fileName != file.name || fileSize != file.size || chunkIdx != chunk.idx
		|| chunkCount != file.chunks.size() || chunkData.size() != chunk.plainSize) {
		error = "The downloaded chunk metadata is invalid.";
		return false;
	}

	payload = chunkData;
	return true;
}
