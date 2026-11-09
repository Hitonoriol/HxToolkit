#pragma once

#include <QObject>
#include <QStringList>

class ICloudStorage : public QObject
{
	Q_OBJECT

public:
	using QObject::QObject;

	virtual bool CanUpload() const = 0;
	virtual bool CanDownload() const = 0;
	virtual bool CanDelete() const = 0;
	virtual void UploadFile(const QByteArray& data, const QString& fileName) = 0;
	virtual void DownloadFile(const QString& fileId) = 0;
	virtual void DeleteFiles(const QStringList& fileIds) = 0;
	virtual void Pause() = 0;
	virtual void Resume() = 0;
	virtual void Stop() = 0;

signals:
	void ConfigurationChanged();
	void UploadFinished(const QString& fileId, const QString& deletionId, const QString& error);
	void UploadProgress(qint64 bytesSent, qint64 bytesTotal);
	void DownloadFinished(const QByteArray& data, const QString& error);
	void DownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void DeleteProgress(qint64 filesDeleted, qint64 filesTotal);
	void DeleteFinished(const QString& error);
};
