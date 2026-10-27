#pragma once

#include "UI/Component.h"

#include <QList>
#include <QPointer>
#include <QSet>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QVBoxLayout;

class RSSFeed : public Component
{
	Q_OBJECT

public:
	explicit RSSFeed(QWidget* parent = nullptr);

	QList<ToolConfigurationParameter*> GetConfigurationParameters() const override;
	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

signals:
	void UpdatesAvailable(const QString& title, const QString& message);

private slots:
	void FetchFeed();
	void OnReplyFinished(QNetworkReply* reply);

private:
	struct Entry
	{
		QString key;
		QString title;
		QString description;
		QString date;
		QString link;
	};

	bool EditUrl(QWidget* parent);
	bool EditUpdateInterval(QWidget* parent);
	QList<Entry> ParseFeed(const QByteArray& data, QString& parsedFeedTitle, QString& error) const;
	void RefreshEntries(const QList<Entry>& entries);
	void UpdateSchedule();
	bool IsToolVisible() const;

	QNetworkAccessManager* networkManager;
	QPointer<QNetworkReply> activeReply;
	QLabel* statusLabel;
	QVBoxLayout* entriesLayout;
	QTimer updateTimer;
	QString url;
	QString feedTitle;
	QSet<QString> knownEntryKeys;
	QList<ToolConfigurationParameter*> configurationParameters;
	int updateIntervalSeconds{300};
	bool receivedInitialFeed{};
};
