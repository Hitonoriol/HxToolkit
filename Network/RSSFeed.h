#pragma once

#include "UI/Component.h"

#include <QDateTime>
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
	void UpdateElapsedTimes();

private:
	struct Entry
	{
		QString key;
		QString title;
		QString description;
		QString date;
		QString link;
	};
	struct DateLabel
	{
		QPointer<QLabel> label;
		QDateTime dateTime;
	};

	bool EditUrl(QWidget* parent);
	bool EditUpdateInterval(QWidget* parent);
	QList<Entry> ParseFeed(const QByteArray& data, QString& parsedFeedTitle, QString& error) const;
	void RefreshEntries(const QList<Entry>& entries);
	static QString GetDateText(const QDateTime& dateTime);
	static QString GetElapsedTimeText(const QDateTime& dateTime);
	void UpdateSchedule();
	bool IsToolVisible() const;

	QNetworkAccessManager* networkManager;
	QPointer<QNetworkReply> activeReply;
	QLabel* statusLabel;
	QVBoxLayout* entriesLayout;
	QTimer updateTimer;
	QTimer elapsedTimeTimer;
	QString url;
	QString feedTitle;
	QSet<QString> knownEntryKeys;
	QList<ToolConfigurationParameter*> configurationParameters;
	QList<DateLabel> dateLabels;
	int updateIntervalSeconds{300};
	bool receivedInitialFeed{};
};
