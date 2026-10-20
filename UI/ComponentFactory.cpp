#include "ComponentFactory.h"

#include "General/BaseConverter.h"
#include "General/Calculator.h"
#include "General/ColorPicker.h"
#include "General/MarkdownEditor.h"

#include "Productivity/Checklist/Checklist.h"
#include "Productivity/GitlabTasks/GitlabTasks.h"
#include "Productivity/GitlabMergeRequests/GitlabMergeRequests.h"
#include "Productivity/GitlabTimeStats/GitlabTimeStats.h"
#include "Productivity/TaskTracker/TaskTracker.h"
#include "Productivity/FocusTracker/FocusTracker.h"

#include "Time/Stopwatch.h"
#include "Time/Timer.h"
#include "Time/DateCountdown.h"

#include "Random/RandomNumber.h"
#include "Random/RandomString.h"

#include "Filesystem/FileSearch.h"
#include "Filesystem/LogExtractor.h"
#include "Filesystem/SymlinkMover.h"

#include "System/RamMonitor.h"
#include "System/ClipboardHistory.h"
#include "System/SystemShortcuts.h"
#ifdef _WIN32
#include "System/WindowBox.h"
#endif

#include "Network/Ping.h"
#include "Network/NetworkInterfaces.h"
#ifdef _WIN32
#include "Network/Webview.h"
#endif

#include "Screensavers/MeltingScreen.h"

#include "UI/Tab.h"

#include <QMessageBox>

std::map<ToolType, ComponentSupplierEntry> ComponentFactory::componentSuppliers{
	{ToolType::BaseConverter, {"General", "Base converter", ":/icons/calculator.svg", DefaultSupplier<BaseConverter>(false)}},
	{ToolType::Calculator, {"General", "Calculator", ":/icons/calculator.svg", DefaultSupplier<Calculator>(false)}},
	{ToolType::ColorPicker, {"General", "Color picker", ":/icons/pipette.svg", DefaultSupplier<ColorPicker>(false)}},
	{ToolType::MarkdownEditor, {"General", "Markdown editor", ":/icons/text.svg", DefaultSupplier<MarkdownEditor>()}},

	{ToolType::Checklist, {"Productivity", "Checklist", ":/icons/checklist.svg", DefaultSupplier<Checklist>(false)}},
	{ToolType::GitlabTasks, {"GitLab", "GitLab Tasks", ":/icons/gitlab.svg", DefaultSupplier<GitlabTasks>()}},
	{ToolType::GitlabMergeRequests, {"GitLab", "GitLab MRs", ":/icons/gitlab.svg", DefaultSupplier<GitlabMergeRequests>()}},
	{ToolType::GitlabTimeStats, {"GitLab", "GitLab Time Stats", ":/icons/gitlab.svg", DefaultSupplier<GitlabTimeStats>()}},
	{ToolType::TaskTracker, {"Productivity", "Task tracker", ":/icons/checklist.svg", DefaultSupplier<TaskTracker>(false)}},
	{ToolType::FocusTracker, {"Productivity", "Focus Tracker", ":/icons/clock.svg", DefaultSupplier<FocusTracker>()}},

	{ToolType::Stopwatch, {"Time", "Stopwatch", ":/icons/clock.svg", DefaultSupplier<Stopwatch>(false)}},
	{ToolType::DateCountdown, {"Time", "Date Countdown", ":/icons/clock.svg", DefaultSupplier<DateCountdown>(false)}},

	{ToolType::Timer, {"Time", "Timer", ":/icons/clock.svg", [](HxNxToolkit* toolkit, const QString& name) -> Component* {
		auto timer = new Timer;
		toolkit->connect(timer, &Timer::TimerCompleted, toolkit, [toolkit](int64_t startTime, Time duration) {
			if (!toolkit->isVisible()) {
				toolkit->setVisible(true);
			}

			toolkit->setWindowState(toolkit->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
			toolkit->activateWindow();

			auto durationStr = Time::GetTimeString(duration.GetTimeMs());
			auto startTimeStr = Time::GetTimeString(QDateTime::fromMSecsSinceEpoch(startTime));
			auto notification = new QMessageBox(toolkit);
			notification->setWindowTitle("Timer");
			notification->setText("Timer has completed!");
			notification->setInformativeText(QString("Duration: %1\nStarted at: %2").arg(durationStr).arg(startTimeStr));
			notification->show();
		});

		auto tab = toolkit->GetCurrentTab();
		if (!tab) {
			tab = toolkit->NewTab();
		}

		tab->AddComponent(timer, name, false);
		return timer;
	}}},

	{ToolType::RandomNumber, {"Random", "Random number", ":/icons/random.svg", DefaultSupplier<RandomNumber>(false)}},
	{ToolType::RandomString, {"Random", "Random string", ":/icons/random.svg", DefaultSupplier<RandomString>(false)}},
	{ToolType::FileSearch, {"Filesystem", "File search", ":/icons/folder-search.svg", DefaultSupplier<FileSearch>()}},
	{ToolType::LogExtractor, {"Filesystem", "Log Extractor", ":/icons/text.svg", DefaultSupplier<LogExtractor>()}},
	{ToolType::SymlinkMover, {"Filesystem", "Symlink mover", ":/icons/link.svg", DefaultSupplier<SymlinkMover>()}},
	{ToolType::Ping, {"Network", "Ping", ":/icons/network.svg", DefaultSupplier<Ping>(false)}},
	{ToolType::NetworkInterfaces, {"Network", "Network interfaces", ":/icons/network.svg", DefaultSupplier<NetworkInterfaces>()}},
#ifdef _WIN32
	{ToolType::Webview, {"Network", "Webview", ":/icons/network.svg", DefaultSupplier<Webview>()}},
#endif
	{ToolType::MeltingScreen, {"Screensavers", "Melting screen", ":/icons/melting-screen.svg", DefaultSupplier<MeltingScreen>(false)}},
	{ToolType::RamMonitor, {"System", "RAM monitor", ":/icons/memory.svg", DefaultSupplier<RamMonitor>(false)}},
	{ToolType::ClipboardManager, {"System", "Clipboard history", ":/icons/clipboard.svg", DefaultSupplier<ClipboardHistory>()}},
	{ToolType::SystemShortcuts, {"System", "System Shortcuts", ":/icons/keyboard.svg", DefaultSupplier<SystemShortcuts>(false)}},
#ifdef _WIN32
	{ToolType::WindowBox, {"System", "Window Box", ":/icons/expand.svg", DefaultSupplier<WindowBox>()}}
#endif
};

QList<ToolInfo> ComponentFactory::AvailableTools()
{
	QList<ToolInfo> tools;
	for (auto& [toolType, supplierEntry] : componentSuppliers) {
		tools.append({toolType, supplierEntry.CategoryName, supplierEntry.ToolName, supplierEntry.IconPath});
	}
	return tools;
}

Component* ComponentFactory::CreateComponent(HxNxToolkit* toolkit, ToolType toolType)
{
	auto supplierIt = componentSuppliers.find(toolType);
	if (supplierIt == componentSuppliers.end()) {
		return {};
	}

	return supplierIt->second.Supplier(toolkit);
}
