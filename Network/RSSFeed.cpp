#include "RSSFeed.h"

#include "UI/ToolConfigurationParameter.h"

#include <QDateTime>
#include <QInputDialog>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScrollArea>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <algorithm>

RSSFeed::RSSFeed(QWidget* parent)
	: Component(parent, ToolType::RSSFeed)
	, networkManager(new QNetworkAccessManager(this))
	, statusLabel(new QLabel("Configure an RSS URL to load entries.", this))
{
	auto rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);

	auto scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);
	auto content = new QWidget(scrollArea);
	entriesLayout = new QVBoxLayout(content);
	entriesLayout->setContentsMargins(0, 0, 0, 0);
	entriesLayout->setSpacing(8);
	entriesLayout->setAlignment(Qt::AlignTop);
	entriesLayout->addWidget(statusLabel);
	scrollArea->setWidget(content);
	rootLayout->addWidget(scrollArea);

	configurationParameters = {
		new ToolConfigurationParameter("RSS URL", [this](QWidget* parent) { return EditUrl(parent); }, this),
		new ToolConfigurationParameter("Update interval (seconds)", [this](QWidget* parent) { return EditUpdateInterval(parent); }, this)
	};

	connect(&updateTimer, &QTimer::timeout, this, &RSSFeed::FetchFeed);
	connect(&elapsedTimeTimer, &QTimer::timeout, this, &RSSFeed::UpdateElapsedTimes);
	elapsedTimeTimer.start(std::chrono::seconds{10});
}

QList<ToolConfigurationParameter*> RSSFeed::GetConfigurationParameters() const
{
	return configurationParameters;
}

QJsonObject RSSFeed::SaveState()
{
	auto state = Component::SaveState();
	state["Url"] = url;
	state["UpdateIntervalSeconds"] = updateIntervalSeconds;
	return state;
}

void RSSFeed::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	url = state["Url"].toString();
	updateIntervalSeconds = std::clamp(state["UpdateIntervalSeconds"].toInt(300), 1, 86'400);
	UpdateSchedule();
	FetchFeed();
}

bool RSSFeed::EditUrl(QWidget* parent)
{
	bool accepted = false;
	auto newUrl = QInputDialog::getText(parent, "RSS Feed", "RSS URL:", QLineEdit::Normal, url, &accepted).trimmed();
	if (!accepted || newUrl == url) {
		return false;
	}

	url = newUrl;
	if (activeReply) {
		auto reply = activeReply;
		activeReply = {};
		reply->abort();
	}
	knownEntryKeys.clear();
	receivedInitialFeed = false;
	feedTitle.clear();
	UpdateSchedule();
	FetchFeed();
	return true;
}

bool RSSFeed::EditUpdateInterval(QWidget* parent)
{
	bool accepted = false;
	const auto newInterval = QInputDialog::getInt(parent, "RSS Feed", "Update interval (seconds):",
		updateIntervalSeconds, 1, 86'400, 1, &accepted);
	if (!accepted || newInterval == updateIntervalSeconds) {
		return false;
	}

	updateIntervalSeconds = newInterval;
	UpdateSchedule();
	return true;
}

void RSSFeed::FetchFeed()
{
	if (url.isEmpty()) {
		updateTimer.stop();
		statusLabel->setText("Configure an RSS URL to load entries.");
		statusLabel->show();
		return;
	}

	if (activeReply) {
		return;
	}

	auto feedUrl = QUrl::fromUserInput(url);
	QNetworkRequest request(feedUrl);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setHeader(QNetworkRequest::UserAgentHeader, "HxNxToolkit RSS Feed");
	auto reply = networkManager->get(request);
	activeReply = reply;
	connect(reply, &QNetworkReply::finished, this, [this, reply] { OnReplyFinished(reply); });
}

void RSSFeed::OnReplyFinished(QNetworkReply* reply)
{
	if (reply != activeReply) {
		reply->deleteLater();
		return;
	}

	activeReply = {};
	if (reply->error() != QNetworkReply::NoError) {
		statusLabel->setText(QString("Unable to update feed: %1").arg(reply->errorString()));
		statusLabel->show();
		reply->deleteLater();
		return;
	}

	QString parsedFeedTitle;
	QString parseError;
	auto entries = ParseFeed(reply->readAll(), parsedFeedTitle, parseError);
	reply->deleteLater();
	if (!parseError.isEmpty()) {
		statusLabel->setText(QString("Unable to read feed: %1").arg(parseError));
		statusLabel->show();
		return;
	}

	feedTitle = parsedFeedTitle;
	QStringList newEntryTitles;
	QSet<QString> fetchedEntryKeys;
	for (const auto& entry : entries) {
		if (receivedInitialFeed && !knownEntryKeys.contains(entry.key) && !fetchedEntryKeys.contains(entry.key)) {
			newEntryTitles.append(entry.title);
		}
		fetchedEntryKeys.insert(entry.key);
	}

	knownEntryKeys.unite(fetchedEntryKeys);
	RefreshEntries(entries);
	if (receivedInitialFeed && !newEntryTitles.isEmpty() && !IsToolVisible()) {
		const auto notificationTitle = feedTitle.isEmpty() ? QString("RSS Feed") : feedTitle;
		QString message = QString("%1 new entries").arg(newEntryTitles.size());
		if (newEntryTitles.size() == 1) {
			message = newEntryTitles.first().isEmpty() ? QString("New feed entry") : newEntryTitles.first();
		}
		emit UpdatesAvailable(notificationTitle, message);
	}
	receivedInitialFeed = true;
}

QList<RSSFeed::Entry> RSSFeed::ParseFeed(const QByteArray& data, QString& parsedFeedTitle, QString& error) const
{
	QList<Entry> entries;
	QXmlStreamReader xml(data);
	Entry entry;
	bool readingEntry = false;

	while (!xml.atEnd()) {
		xml.readNext();
		if (xml.isStartElement()) {
			const auto name = xml.name();
			if (name == "item" || name == "entry") {
				entry = {};
				readingEntry = true;
			} else if (readingEntry && name == "title") {
				entry.title = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
			} else if (readingEntry && (name == "description" || name == "summary" || name == "content")) {
				entry.description = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
			} else if (readingEntry && (name == "pubDate" || name == "published" || name == "updated")) {
				entry.date = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
			} else if (readingEntry && (name == "guid" || name == "id")) {
				entry.key = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
			} else if (readingEntry && name == "link") {
				entry.link = xml.attributes().value("href").toString();
				if (entry.link.isEmpty()) {
					entry.link = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
				}
			} else if (!readingEntry && name == "title" && parsedFeedTitle.isEmpty()) {
				parsedFeedTitle = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
			}
		} else if (xml.isEndElement() && (xml.name() == "item" || xml.name() == "entry")) {
			readingEntry = false;
			if (entry.key.isEmpty()) {
				entry.key = entry.link.isEmpty() ? entry.title + entry.date : entry.link;
			}
			if (!entry.title.isEmpty() || !entry.link.isEmpty()) {
				entries.append(entry);
			}
		}
	}

	if (xml.hasError()) {
		error = xml.errorString();
	}
	return entries;
}

void RSSFeed::RefreshEntries(const QList<Entry>& entries)
{
	dateLabels.clear();
	while (auto item = entriesLayout->takeAt(0)) {
		if (item->widget()) {
			item->widget()->deleteLater();
		}
		delete item;
	}

	if (entries.isEmpty()) {
		statusLabel = new QLabel("The feed contains no entries.", this);
		entriesLayout->addWidget(statusLabel);
		return;
	}

	for (const auto& entry : entries) {
		auto card = new QWidget(this);
		card->setObjectName("rssEntry");
		card->setStyleSheet("#rssEntry { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }");
		auto cardLayout = new QVBoxLayout(card);
		cardLayout->setSpacing(4);

		if (!entry.date.isEmpty()) {
			auto dateText = entry.date;
			auto dateTime = QDateTime::fromString(entry.date, Qt::ISODate);
			if (!dateTime.isValid()) {
				dateTime = QDateTime::fromString(entry.date, Qt::RFC2822Date);
			}
			if (dateTime.isValid()) {
				dateText = GetDateText(dateTime);
			}

			auto date = new QLabel(dateText, card);
			date->setStyleSheet("color: palette(window-text); font-size: 11px;");
			cardLayout->addWidget(date);
			if (dateTime.isValid()) {
				dateLabels.append({date, dateTime});
			}
		}

		auto title = new QLabel(card);
		title->setTextFormat(Qt::RichText);
		title->setTextInteractionFlags(Qt::TextBrowserInteraction);
		title->setOpenExternalLinks(true);
		const auto escapedTitle = entry.title.toHtmlEscaped();
		if (entry.link.isEmpty()) {
			title->setText(QString("<b>%1</b>").arg(escapedTitle));
		} else {
			title->setText(QString("<b><a href=\"%1\">%2</a></b>").arg(entry.link.toHtmlEscaped(), escapedTitle));
		}
		cardLayout->addWidget(title);

		if (!entry.description.isEmpty()) {
			QTextDocument document;
			document.setHtml(entry.description);
			auto description = new QLabel(document.toPlainText().trimmed(), card);
			description->setWordWrap(true);
			description->setTextInteractionFlags(Qt::TextSelectableByMouse);
			cardLayout->addWidget(description);
		}

		entriesLayout->addWidget(card);
	}

	entriesLayout->addStretch();
	statusLabel = new QLabel(this);
	statusLabel->hide();
	entriesLayout->addWidget(statusLabel);
}

QString RSSFeed::GetDateText(const QDateTime& dateTime)
{
	return QString("%1 (%2 ago)")
		.arg(dateTime.toLocalTime().toString("d MMM yyyy, HH:mm"), GetElapsedTimeText(dateTime));
}

QString RSSFeed::GetElapsedTimeText(const QDateTime& dateTime)
{
	auto published = dateTime.toUTC();
	auto now = QDateTime::currentDateTimeUtc();
	if (published >= now) {
		return "0 seconds";
	}

	int months = (now.date().year() - published.date().year()) * 12
		+ now.date().month() - published.date().month();
	auto remainderStart = published.addMonths(months);
	if (remainderStart > now) {
		--months;
		remainderStart = published.addMonths(months);
	}

	auto remainingSeconds = remainderStart.secsTo(now);
	int days = static_cast<int>(remainingSeconds / 86'400);
	remainingSeconds %= 86'400;
	int hours = static_cast<int>(remainingSeconds / 3'600);
	remainingSeconds %= 3'600;
	int minutes = static_cast<int>(remainingSeconds / 60);
	int seconds = static_cast<int>(remainingSeconds % 60);

	QStringList parts;
	if (months > 0) {
		parts.append(QString("%1 month%2").arg(months).arg(months == 1 ? "" : "s"));
	}

	if (days > 0) {
		parts.append(QString("%1 day%2").arg(days).arg(days == 1 ? "" : "s"));
	}

	if (hours > 0) {
		parts.append(QString("%1 hour%2").arg(hours).arg(hours == 1 ? "" : "s"));
	}

	if (minutes > 0) {
		parts.append(QString("%1 minute%2").arg(minutes).arg(minutes == 1 ? "" : "s"));
	}

	if (seconds > 0) {
		parts.append(QString("%1 second%2").arg(seconds).arg(seconds == 1 ? "" : "s"));
	}

	return parts.isEmpty() ? QString("0 seconds") : parts.join(' ');
}

void RSSFeed::UpdateElapsedTimes()
{
	for (const auto& dateLabel : dateLabels) {
		if (dateLabel.label) {
			dateLabel.label->setText(GetDateText(dateLabel.dateTime));
		}
	}
}

void RSSFeed::UpdateSchedule()
{
	if (url.isEmpty()) {
		updateTimer.stop();
		return;
	}

	updateTimer.start(std::chrono::seconds{updateIntervalSeconds});
}

bool RSSFeed::IsToolVisible() const
{
	return isVisible() && !window()->isMinimized();
}
