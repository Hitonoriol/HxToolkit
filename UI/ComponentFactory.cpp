#include "ComponentFactory.h"

#include "General/BaseConverter.h"
#include "General/Calculator.h"
#include "General/MarkdownEditor.h"

#include "Productivity/Checklist/Checklist.h"
#include "Productivity/GitlabTasks/GitlabTasks.h"
#include "Productivity/GitlabMergeRequests/GitlabMergeRequests.h"
#include "Productivity/GitlabTimeStats/GitlabTimeStats.h"
#include "Productivity/TaskTracker/TaskTracker.h"

#include "Time/Stopwatch.h"
#include "Time/Timer.h"

#include "Random/RandomNumber.h"
#include "Random/RandomString.h"

#include "Filesystem/FileSearch.h"
#include "Filesystem/SymlinkMover.h"

#include "System/RamMonitor.h"
#include "System/ClipboardHistory.h"

#include "UI/Tab.h"

#include <QMessageBox>

std::map<ToolType, ComponentSupplierEntry> ComponentFactory::componentSuppliers{
	{ToolType::BaseConverter, {"General", "Base converter", DefaultSupplier<BaseConverter>(false)}},
	{ToolType::Calculator, {"General", "Calculator", DefaultSupplier<Calculator>(false)}},
	{ToolType::MarkdownEditor, {"General", "Markdown editor", DefaultSupplier<MarkdownEditor>()}},

	{ToolType::Checklist, {"Productivity", "Checklist", DefaultSupplier<Checklist>(false)}},
	{ToolType::GitlabTasks, {"Productivity", "GitLab Tasks", DefaultSupplier<GitlabTasks>()}},
	{ToolType::GitlabMergeRequests, {"Productivity", "GitLab MRs", DefaultSupplier<GitlabMergeRequests>()}},
	{ToolType::GitlabTimeStats, {"Productivity", "Gitlab Time Stats", DefaultSupplier<GitlabTimeStats>()}},
	{ToolType::TaskTracker, {"Productivity", "Task tracker", DefaultSupplier<TaskTracker>(false)}},

	{ToolType::Stopwatch, {"Time", "Stopwatch", DefaultSupplier<Stopwatch>(false)}},

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
	{ToolType::ClipboardManager, {"System", "Clipboard history", DefaultSupplier<ClipboardHistory>()}}
};

void ComponentFactory::Register(HxNxToolkit* toolkit)
{
	for (auto& [toolType, supplierEntry] : componentSuppliers) {
		auto action = toolkit->AddComponentMenuAction(supplierEntry.CategoryName, supplierEntry.ToolName);
		toolkit->connect(action, &QAction::triggered, toolkit, [=] {
			supplierEntry.Supplier(toolkit);
		});
	}
}

Component* ComponentFactory::CreateComponent(HxNxToolkit* toolkit, ToolType toolType)
{
	auto supplierIt = componentSuppliers.find(toolType);
	if (supplierIt == componentSuppliers.end()) {
		return {};
	}

	return supplierIt->second.Supplier(toolkit);
}
