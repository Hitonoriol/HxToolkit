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

#include "Time/Stopwatch.h"
#include "Time/Timer.h"
#include "Time/DateCountdown.h"

#include "Random/RandomNumber.h"
#include "Random/RandomString.h"

#include "Filesystem/FileSearch.h"
#include "Filesystem/SymlinkMover.h"

#include "System/RamMonitor.h"
#include "System/ClipboardHistory.h"
#include "System/SystemShortcuts.h"

#include "UI/Tab.h"

#include <QMessageBox>

std::map<ToolType, ComponentSupplierEntry> ComponentFactory::componentSuppliers{
	{ToolType::BaseConverter, {"General", "Base converter", DefaultSupplier<BaseConverter>(false)}},
	{ToolType::Calculator, {"General", "Calculator", DefaultSupplier<Calculator>(false)}},
	{ToolType::ColorPicker, {"General", "Color picker", DefaultSupplier<ColorPicker>(false)}},
	{ToolType::MarkdownEditor, {"General", "Markdown editor", DefaultSupplier<MarkdownEditor>()}},

	{ToolType::Checklist, {"Productivity", "Checklist", DefaultSupplier<Checklist>(false)}},
	{ToolType::GitlabTasks, {"GitLab", "GitLab Tasks", DefaultSupplier<GitlabTasks>()}},
	{ToolType::GitlabMergeRequests, {"GitLab", "GitLab MRs", DefaultSupplier<GitlabMergeRequests>()}},
	{ToolType::GitlabTimeStats, {"GitLab", "GitLab Time Stats", DefaultSupplier<GitlabTimeStats>()}},
	{ToolType::TaskTracker, {"Productivity", "Task tracker", DefaultSupplier<TaskTracker>(false)}},

	{ToolType::Stopwatch, {"Time", "Stopwatch", DefaultSupplier<Stopwatch>(false)}},
	{ToolType::DateCountdown, {"Time", "Date Countdown", DefaultSupplier<DateCountdown>(false)}},

	{ToolType::Timer, {"Time", "Timer", [](HxNxToolkit* toolkit, const QString& name) -> Component* {
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

	{ToolType::RandomNumber, {"Random", "Random number", DefaultSupplier<RandomNumber>(false)}},
	{ToolType::RandomString, {"Random", "Random string", DefaultSupplier<RandomString>(false)}},
	{ToolType::FileSearch, {"Filesystem", "File search", DefaultSupplier<FileSearch>()}},
	{ToolType::SymlinkMover, {"Filesystem", "Symlink mover", DefaultSupplier<SymlinkMover>()}},
	{ToolType::RamMonitor, {"System", "RAM monitor", DefaultSupplier<RamMonitor>(false)}},
	{ToolType::ClipboardManager, {"System", "Clipboard history", DefaultSupplier<ClipboardHistory>()}},
	{ToolType::SystemShortcuts, {"System", "System Shortcuts", DefaultSupplier<SystemShortcuts>(false)}}
};

QList<ToolInfo> ComponentFactory::AvailableTools()
{
	QList<ToolInfo> tools;
	for (auto& [toolType, supplierEntry] : componentSuppliers) {
		tools.append({toolType, supplierEntry.CategoryName, supplierEntry.ToolName});
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
