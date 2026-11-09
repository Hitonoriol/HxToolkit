#pragma once

#include "EncryptionAlgorithmFactory.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>

class QFile;
class ICloudStorage;

class FreecloudService : public QObject
{
	Q_OBJECT

public:
	enum class Operation { None, Upload, Download, Delete };
	enum class DeleteMode { CloudAndLocal, Cloud, Local };
	enum class TransferState { None, PendingUpload, Uploading, PendingDownload, Downloading, PendingDelete, Deleting };

	struct ChunkMetadata
	{
		int idx{};
		qint64 plainSize{};
		QByteArray key;
		QString remoteId;
		QString deletionId;
	};

	struct FileMetadata
	{
		QString id;
		QString name;
		QString localPath;
		QString parentId;
		qint64 size{};
		QList<ChunkMetadata> chunks;
	};

	struct DirectoryMetadata
	{
		QString id;
		QString name;
		QString localPath;
		QString parentId;
	};

	explicit FreecloudService(ICloudStorage* cloudStorage, EncryptionAlgorithmFactory encryptionFactory,
		QObject* parent = nullptr);
	~FreecloudService() override;

	const QList<FileMetadata>& GetFiles() const;
	const QList<DirectoryMetadata>& GetDirectories() const;
	Operation GetOperation() const;
	bool IsPaused() const;
	QString GetActiveFileId() const;
	int GetTransferProgress() const;
	QString GetStorageFolder() const;
	void SetStorageFolder(const QString& value);
	void Pause();
	void Resume();
	void StopEntries(const QStringList& fileIds, const QStringList& directoryIds);
	QJsonObject SaveState() const;
	void LoadState(const QJsonObject& state);
	QString AddFile(const QString& path, const QString& parentId = {});
	QString AddFolder(const QString& path, const QString& parentId = {});
	QString AddDirectory(const QString& name, const QString& parentId = {});
	void UpdateParents(const QHash<QString, QString>& fileParents,
		const QHash<QString, QString>& directoryParents);
	bool RenameFile(const QString& id, const QString& name, bool renameLocal, QString& error);
	bool RenameDirectory(const QString& id, const QString& name, bool renameLocal, QString& error);
	void DeleteEntries(const QStringList& fileIds, const QStringList& directoryIds, DeleteMode mode);
	bool IsFileLocal(const QString& id) const;
	bool IsFileCloud(const QString& id) const;
	bool IsDirectoryLocal(const QString& id) const;
	bool IsDirectoryCloud(const QString& id) const;
	TransferState GetFileTransferState(const QString& id) const;
	TransferState GetDirectoryTransferState(const QString& id) const;
	QString GetFileLocalPath(const QString& id) const;
	QString GetDirectoryLocalPath(const QString& id) const;
	qint64 GetDirectorySize(const QString& id) const;
	void DownloadFile(const QString& id, bool openAfterDownload = false);
	void DownloadDirectory(const QString& id);

signals:
	void ViewChanged();
	void StateChanged(bool undoable = true);
	void Error(const QString& error);
	void FileReady(const QString& path);

private slots:
	void OnUploadFinished(const QString& fileId, const QString& deletionId, const QString& error);
	void OnUploadProgress(qint64 bytesSent, qint64 bytesTotal);
	void OnDownloadFinished(const QByteArray& data, const QString& error);
	void OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void OnDeleteProgress(qint64 filesDeleted, qint64 filesTotal);
	void OnDeleteFinished(const QString& error);

private:
	void AddFileMetadata(const QString& path, const QString& parentId);
	QString AddFolderMetadata(const QString& path, const QString& parentId);
	QString AddDirectoryMetadata(const QString& name, const QString& parentId, const QString& localPath = {});
	bool DeleteLocalFile(const QString& id);
	bool DeleteLocalDirectory(const QString& id);
	void FinishDeletion();
	bool IsDirectoryDescendant(const QString& id, const QString& ancestorId) const;
	bool IsDirectorySelectedOrDescendant(const QString& id, const QStringList& selectedIds) const;
	void UpdateTransferProgress(qint64 transferred, qint64 total);
	void StartPendingUpload();
	void StartUpload(int fileIdx);
	void UploadNextChunk();
	void DownloadNextChunk();
	void FinishOperation(const QString& error = {});
	int GetFileIdx(const QString& id) const;
	bool HasMissingChunks(const QString& id) const;
	QString GetRelativeDirectoryPath(const QString& id) const;
	qint64 GetLocalBytes(const FileMetadata& file) const;
	static bool ParseChunk(const QByteArray& data, const FileMetadata& file, const ChunkMetadata& chunk,
		QByteArray& payload, QString& error);

	static constexpr qint64 ChunkSize = 10 * 1024 * 1024;

	ICloudStorage* cloudStorage;
	QList<FileMetadata> files;
	QList<DirectoryMetadata> directories;
	QFile* downloadFile{};
	EncryptionAlgorithmFactory encryptionFactory;
	Operation operation{Operation::None};
	DeleteMode pendingDeleteMode{DeleteMode::CloudAndLocal};
	QString storageFolder;
	int activeFileIdx{-1};
	int activeChunkIdx{-1};
	qint64 transferredBytes{};
	qint64 transferTotalBytes{};
	bool openAfterDownload{};
	bool paused{};
	bool restoredTransfer{};
	QStringList pendingDeleteFileIds;
	QStringList pendingDeleteDirectoryIds;
	QStringList pendingDownloadIds;
	QStringList stoppedUploadIds;
};
