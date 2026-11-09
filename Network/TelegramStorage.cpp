#include "TelegramStorage.h"

#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

TelegramStorage::TelegramStorage(QObject* parent)
	: ICloudStorage(parent)
	, networkManager(new QNetworkAccessManager(this))
{
	connect(networkManager, &QNetworkAccessManager::finished, this, &TelegramStorage::OnReplyFinished);
}

bool TelegramStorage::CanUpload() const
{
	return !botApiKey.isEmpty() && !chatId.isEmpty();
}

bool TelegramStorage::CanDownload() const
{
	return !botApiKey.isEmpty();
}

bool TelegramStorage::CanDelete() const
{
	return !botApiKey.isEmpty() && !chatId.isEmpty();
}

void TelegramStorage::UploadFile(const QByteArray& data, const QString& fileName)
{
	paused = false;
	activeOperation = "upload";
	activeUploadData = data;
	activeUploadFileName = fileName;
	auto multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	QHttpPart chatPart;
	chatPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"chat_id\"");
	chatPart.setBody(chatId.toUtf8());
	multipart->append(chatPart);

	QHttpPart filePart;
	filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
		QString("form-data; name=\"document\"; filename=\"%1\"").arg(fileName));
	filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	filePart.setBody(data);
	multipart->append(filePart);

	auto reply = networkManager->post(QNetworkRequest(GetApiUrl("sendDocument")), multipart);
	activeReply = reply;
	reply->setProperty("FreecloudOperation", "upload");
	multipart->setParent(reply);
	connect(reply, &QNetworkReply::uploadProgress, this, &TelegramStorage::UploadProgress);
}

void TelegramStorage::DownloadFile(const QString& fileId)
{
	paused = false;
	activeOperation = "download";
	activeDownloadFileId = fileId;
	QUrl url(GetApiUrl("getFile"));
	QUrlQuery query;
	query.addQueryItem("file_id", fileId);
	url.setQuery(query);
	auto reply = networkManager->get(QNetworkRequest(url));
	activeReply = reply;
	reply->setProperty("FreecloudOperation", "getFile");
}

void TelegramStorage::DeleteFiles(const QStringList& fileIds)
{
	pendingDeletionIds = fileIds;
	deletionCount = fileIds.size();
	emit DeleteProgress(0, deletionCount);
	DeleteNextFile();
}

void TelegramStorage::Pause()
{
	if (paused || (activeOperation != "upload" && activeOperation != "download")) {
		return;
	}

	paused = true;
	if (activeReply) {
		activeReply->setProperty("FreecloudPaused", true);
		activeReply->abort();
	}
}

void TelegramStorage::Resume()
{
	if (!paused) {
		return;
	}

	const auto operation = activeOperation;
	paused = false;
	if (operation == "upload") {
		UploadFile(activeUploadData, activeUploadFileName);
	} else if (operation == "download") {
		DownloadFile(activeDownloadFileId);
	}
}

void TelegramStorage::Stop()
{
	paused = false;
	activeOperation.clear();
	activeUploadData.clear();
	activeUploadFileName.clear();
	activeDownloadFileId.clear();
	if (activeReply) {
		activeReply->setProperty("FreecloudStopped", true);
		activeReply->abort();
	}
}

QString TelegramStorage::GetBotApiKey() const
{
	return botApiKey;
}

QString TelegramStorage::GetChatId() const
{
	return chatId;
}

void TelegramStorage::SetCredentials(const QString& newBotApiKey, const QString& newChatId)
{
	if (botApiKey == newBotApiKey && chatId == newChatId) {
		return;
	}

	botApiKey = newBotApiKey;
	chatId = newChatId;
	emit ConfigurationChanged();
}

void TelegramStorage::OnReplyFinished(QNetworkReply* reply)
{
	if (reply == activeReply) {
		activeReply = nullptr;
	}

	if (reply->property("FreecloudPaused").toBool()) {
		reply->deleteLater();
		return;
	}

	if (reply->property("FreecloudStopped").toBool()) {
		reply->deleteLater();
		return;
	}

	const auto operation = reply->property("FreecloudOperation").toString();
	const auto data = reply->readAll();
	if (operation == "download") {
		const auto error = reply->error() == QNetworkReply::NoError ? QString{} : reply->errorString();
		activeOperation.clear();
		activeDownloadFileId.clear();
		emit DownloadFinished(data, error);
		reply->deleteLater();
		return;
	}

	const auto response = QJsonDocument::fromJson(data).object();
	const auto error = GetError(reply, response);
	if (operation == "upload") {
		const auto result = response["result"].toObject();
		activeOperation.clear();
		activeUploadData.clear();
		activeUploadFileName.clear();
		emit UploadFinished(result["document"].toObject()["file_id"].toString(),
			QString::number(result["message_id"].toInteger()), error);
	} else if (operation == "delete") {
		if (!error.isEmpty()) {
			pendingDeletionIds.clear();
			deletionCount = 0;
			emit DeleteFinished(error);
		} else {
			pendingDeletionIds.removeFirst();
			emit DeleteProgress(deletionCount - pendingDeletionIds.size(), deletionCount);
			DeleteNextFile();
		}
	} else if (!error.isEmpty()) {
		activeOperation.clear();
		activeDownloadFileId.clear();
		emit DownloadFinished({}, error);
	} else {
		DownloadFilePath(response["result"].toObject()["file_path"].toString());
	}

	reply->deleteLater();
}

void TelegramStorage::DownloadFilePath(const QString& path)
{
	const auto url = QString("https://api.telegram.org/file/bot%1/%2").arg(botApiKey, path);
	auto reply = networkManager->get(QNetworkRequest(url));
	activeReply = reply;
	reply->setProperty("FreecloudOperation", "download");
	connect(reply, &QNetworkReply::downloadProgress, this, &TelegramStorage::DownloadProgress);
}

void TelegramStorage::DeleteNextFile()
{
	if (pendingDeletionIds.isEmpty()) {
		deletionCount = 0;
		emit DeleteFinished({});
		return;
	}

	QUrl url(GetApiUrl("deleteMessage"));
	QUrlQuery query;
	query.addQueryItem("chat_id", chatId);
	query.addQueryItem("message_id", pendingDeletionIds.front());
	url.setQuery(query);
	auto reply = networkManager->post(QNetworkRequest(url), QByteArray{});
	reply->setProperty("FreecloudOperation", "delete");
}

QString TelegramStorage::GetApiUrl(const QString& method) const
{
	return QString("https://api.telegram.org/bot%1/%2").arg(botApiKey, method);
}

QString TelegramStorage::GetError(QNetworkReply* reply, const QJsonObject& response)
{
	if (reply->error() != QNetworkReply::NoError) {
		return response["description"].toString(reply->errorString());
	}

	if (!response["ok"].toBool()) {
		return response["description"].toString("Telegram request failed.");
	}

	return {};
}
