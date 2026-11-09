#pragma once

#include "ICloudStorage.h"

#include <QPointer>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

class TelegramStorage : public ICloudStorage
{
	Q_OBJECT

public:
	explicit TelegramStorage(QObject* parent = nullptr);

	bool CanUpload() const override;
	bool CanDownload() const override;
	bool CanDelete() const override;
	void UploadFile(const QByteArray& data, const QString& fileName) override;
	void DownloadFile(const QString& fileId) override;
	void DeleteFiles(const QStringList& fileIds) override;
	void Pause() override;
	void Resume() override;
	void Stop() override;
	QString GetBotApiKey() const;
	QString GetChatId() const;
	void SetCredentials(const QString& botApiKey, const QString& chatId);

private slots:
	void OnReplyFinished(QNetworkReply* reply);

private:
	void DownloadFilePath(const QString& path);
	void DeleteNextFile();
	QString GetApiUrl(const QString& method) const;
	static QString GetError(QNetworkReply* reply, const QJsonObject& response);

	QNetworkAccessManager* networkManager;
	QString botApiKey;
	QString chatId;
	QStringList pendingDeletionIds;
	QPointer<QNetworkReply> activeReply;
	QByteArray activeUploadData;
	QString activeUploadFileName;
	QString activeDownloadFileId;
	QString activeOperation;
	int deletionCount{};
	bool paused{};
};
