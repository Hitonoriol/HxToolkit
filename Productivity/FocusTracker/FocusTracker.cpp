#include "FocusTracker.h"
#include "DurationTreeWidgetItem.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QScrollBar>
#include <QSet>

#include <Windows.h>

#include <algorithm>
#include <array>

FocusTracker::FocusTracker(QWidget* parent)
	: Component(parent, ToolType::FocusTracker)
{
	ui.setupUi(this);
	ui.ActivityTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.ActivityTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	ui.ActivityTree->header()->setSortIndicator(1, Qt::DescendingOrder);
	ui.ActivityTree->setSortingEnabled(true);

	connect(&trackingTimer, &QTimer::timeout, this, &FocusTracker::TrackForegroundWindow);
	connect(ui.PeriodSelect, &QComboBox::currentIndexChanged, this, &FocusTracker::UpdateTable);
	TrackForegroundWindow();
	trackingTimer.start(1'000);
}

FocusTracker::~FocusTracker()
{
}

QJsonObject FocusTracker::SaveState()
{
	auto state = Component::SaveState();
	auto sessionState = QJsonArray{};
	auto now = QDateTime::currentDateTime();

	for (const auto& session : sessions) {
		auto sessionObject = QJsonObject{};
		sessionObject["ProcessName"] = session.ProcessName;
		sessionObject["Title"] = session.Title;
		sessionObject["ExecutablePath"] = session.ExecutablePath;
		sessionObject["StartTime"] = session.StartTime.toString(Qt::ISODateWithMs);
		sessionObject["EndTime"] = (session.EndTime.isValid() ? session.EndTime : now).toString(Qt::ISODateWithMs);
		sessionState.append(sessionObject);
	}

	state["Sessions"] = sessionState;
	state["Period"] = ui.PeriodSelect->currentIndex();
	return state;
}

void FocusTracker::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	sessions.clear();
	currentWindowId = 0;
	currentProcessName.clear();
	currentTitle.clear();
	currentExecutablePath.clear();

	for (const auto& sessionValue : state["Sessions"].toArray()) {
		auto sessionObject = sessionValue.toObject();
		auto executablePath = sessionObject["ExecutablePath"].toString();
		auto processName = sessionObject["ProcessName"].toString();
		if (processName.isEmpty()) {
			processName = QFileInfo(executablePath).fileName();
		}
		if (processName.isEmpty()) {
			processName = "Unknown process";
		}

		auto session = FocusSession{
			.ProcessName = processName,
			.Title = sessionObject["Title"].toString(),
			.ExecutablePath = executablePath,
			.StartTime = QDateTime::fromString(sessionObject["StartTime"].toString(), Qt::ISODateWithMs),
			.EndTime = QDateTime::fromString(sessionObject["EndTime"].toString(), Qt::ISODateWithMs)
		};

		if (session.StartTime.isValid() && session.EndTime.isValid() && session.EndTime > session.StartTime) {
			sessions.append(session);
		}
	}

	ui.PeriodSelect->setCurrentIndex(std::clamp(state["Period"].toInt(), 0, ui.PeriodSelect->count() - 1));
	TrackForegroundWindow();
}

void FocusTracker::TrackForegroundWindow()
{
	auto window = GetForegroundWindow();
	auto now = QDateTime::currentDateTime();
	if (!window) {
		FinishCurrentSession(now);
		return;
	}

	auto titleBuffer = std::array<wchar_t, 32'768>{};
	GetWindowTextW(window, titleBuffer.data(), static_cast<int>(titleBuffer.size()));
	auto title = QString::fromWCharArray(titleBuffer.data()).trimmed();
	if (title.isEmpty()) {
		title = "Untitled window";
	}

	DWORD processId{};
	GetWindowThreadProcessId(window, &processId);
	auto executablePath = QString{};
	if (auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processId)) {
		auto pathBuffer = std::array<wchar_t, 32'768>{};
		DWORD pathSize = static_cast<DWORD>(pathBuffer.size());
		if (QueryFullProcessImageNameW(process, 0, pathBuffer.data(), &pathSize)) {
			executablePath = QString::fromWCharArray(pathBuffer.data(), static_cast<int>(pathSize));
		}
		CloseHandle(process);
	}

	auto windowId = reinterpret_cast<quintptr>(window);
	auto processName = QFileInfo(executablePath).fileName();
	if (processName.isEmpty()) {
		processName = "Unknown process";
	}

	if (windowId == currentWindowId && processName == currentProcessName && title == currentTitle && executablePath == currentExecutablePath) {
		UpdateTable();
		emit Modified(this);
		return;
	}

	FinishCurrentSession(now);
	currentWindowId = windowId;
	currentProcessName = processName;
	currentTitle = title;
	currentExecutablePath = executablePath;
	sessions.append({ processName, title, executablePath, now, {} });
	UpdateTable();
	emit Modified(this);
}

void FocusTracker::UpdateTable()
{
	auto now = QDateTime::currentDateTime();
	auto periodStart = GetPeriodStart();
	auto activities = QHash<QString, ProcessActivity>{};
	auto knownProcessKeys = QSet<QString>{};
	auto expandedProcessKeys = QSet<QString>{};
	auto selectedItemKey = QString{};
	auto scrollPosition = ui.ActivityTree->verticalScrollBar()->value();

	for (int idx = 0; idx < ui.ActivityTree->topLevelItemCount(); ++idx) {
		auto item = ui.ActivityTree->topLevelItem(idx);
		auto processKey = item->data(0, Qt::UserRole).toString();
		knownProcessKeys.insert(processKey);
		if (item->isExpanded()) {
			expandedProcessKeys.insert(processKey);
		}
	}

	auto selectedItems = ui.ActivityTree->selectedItems();
	if (!selectedItems.isEmpty()) {
		selectedItemKey = selectedItems.first()->data(0, Qt::UserRole).toString();
	}

	for (const auto& session : sessions) {
		auto durationMs = GetSessionDurationInPeriod(session, periodStart, now);
		if (durationMs == 0) {
			continue;
		}

		auto key = session.ProcessName.toCaseFolded();
		auto& activity = activities[key];
		activity.ProcessName = session.ProcessName;
		activity.ExecutablePath = session.ExecutablePath;
		activity.DurationMs += durationMs;
		activity.TitleDurations[session.Title] += durationMs;
	}

	auto activityList = activities.values();
	auto restoredSelectedItem = static_cast<QTreeWidgetItem*>(nullptr);
	ui.ActivityTree->setSortingEnabled(false);
	ui.ActivityTree->clear();
	for (const auto& activity : activityList) {
		auto processKey = "process:" + activity.ProcessName.toCaseFolded();
		auto icon = activity.ExecutablePath.isEmpty() ? QIcon{} : QFileIconProvider{}.icon(QFileInfo(activity.ExecutablePath));
		auto processItem = new DurationTreeWidgetItem(ui.ActivityTree);
		processItem->setIcon(0, icon);
		processItem->setText(0, activity.ProcessName);
		processItem->setData(0, Qt::UserRole, processKey);
		processItem->setText(1, GetDurationString(activity.DurationMs));
		processItem->setData(1, Qt::UserRole, activity.DurationMs);
		processItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
		processItem->setExpanded(knownProcessKeys.contains(processKey) && expandedProcessKeys.contains(processKey));
		if (processKey == selectedItemKey) {
			restoredSelectedItem = processItem;
		}

		for (auto it = activity.TitleDurations.constKeyValueBegin(); it != activity.TitleDurations.constKeyValueEnd(); ++it) {
			auto titleKey = processKey + "\ntitle:" + it->first;
			auto titleItem = new DurationTreeWidgetItem(processItem);
			titleItem->setText(0, it->first);
			titleItem->setData(0, Qt::UserRole, titleKey);
			titleItem->setText(1, GetDurationString(it->second));
			titleItem->setData(1, Qt::UserRole, it->second);
			titleItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
			if (titleKey == selectedItemKey) {
				restoredSelectedItem = titleItem;
			}
		}
	}

	ui.ActivityTree->setSortingEnabled(true);
	ui.ActivityTree->sortItems(ui.ActivityTree->header()->sortIndicatorSection(), ui.ActivityTree->header()->sortIndicatorOrder());
	if (restoredSelectedItem) {
		restoredSelectedItem->setSelected(true);
	}

	ui.ActivityTree->verticalScrollBar()->setValue(scrollPosition);
}

void FocusTracker::FinishCurrentSession(const QDateTime& endTime)
{
	if (currentWindowId == 0 || sessions.isEmpty()) {
		return;
	}

	sessions.last().EndTime = endTime;
	currentWindowId = 0;
	currentProcessName.clear();
	currentTitle.clear();
	currentExecutablePath.clear();
}

QDateTime FocusTracker::GetPeriodStart() const
{
	auto now = QDateTime::currentDateTime();
	auto date = now.date();
	switch (ui.PeriodSelect->currentIndex()) {
	case 0: return QDateTime(date, QTime{});
	case 1: return QDateTime(date.addDays(1 - date.dayOfWeek()), QTime{});
	case 2: return QDateTime(QDate(date.year(), date.month(), 1), QTime{});
	case 3: return QDateTime(QDate(date.year(), 1, 1), QTime{});
	default: return now;
	}
}

qint64 FocusTracker::GetSessionDurationInPeriod(const FocusSession& session, const QDateTime& periodStart, const QDateTime& now) const
{
	auto endTime = session.EndTime.isValid() ? session.EndTime : now;
	auto startTime = (std::max)(session.StartTime, periodStart);
	auto clampedEndTime = (std::min)(endTime, now);
	return startTime < clampedEndTime ? startTime.msecsTo(clampedEndTime) : 0;
}

QString FocusTracker::GetDurationString(qint64 durationMs) const
{
	auto totalSeconds = durationMs / 1'000;
	auto hours = totalSeconds / 3'600;
	auto minutes = totalSeconds % 3'600 / 60;
	auto seconds = totalSeconds % 60;
	return QString("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
}
