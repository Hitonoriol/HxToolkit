#include "GitlabTasks.h"

#include "Gitlab/GitlabClient.h"
#include "Gitlab/WorkItem.h"
#include "HxNxToolkit.h"
#include "WorkItemEntry.h"

#include <QComboBox>
#include <QJsonArray>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>

GitlabTasks::GitlabTasks(QWidget* parent)
	: Component(parent, ToolType::GitlabTasks)
{
	ui.setupUi(this);
	ui.RefreshButton->setIcon(QIcon(":/icons/refresh.svg"));
	ui.RefreshButton->setStyleSheet("QToolButton { border: 1px solid palette(mid); border-radius: 3px; padding: 3px; background: palette(button); }");
	gitlabClient = new GitlabClient(this);
	connect(gitlabClient, &GitlabClient::WorkItemsUpdated, this, &GitlabTasks::OnWorkItemsUpdated);
	connect(gitlabClient, &GitlabClient::WorkItemsChanged, this, &GitlabTasks::OnWorkItemsChanged);
	connect(gitlabClient, &GitlabClient::WorkItemUpdated, this, &GitlabTasks::OnWorkItemUpdated);
	connect(gitlabClient, &GitlabClient::ElapsedTimeAdded, this, &GitlabTasks::OnElapsedTimeAdded);
	connect(gitlabClient, &GitlabClient::requestFailed, this, &GitlabTasks::OnGitlabRequestFailed);
	connect(ui.ProjectCombo, &QComboBox::currentTextChanged, this, &GitlabTasks::OnProjectChanged);

	auto refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &GitlabTasks::RefreshWorkItems);
	refreshTimer->start(60'000);

	RefreshWorkItems();
}

QSize GitlabTasks::sizeHint() const
{
	return QSize(600, 600);
}

QJsonObject GitlabTasks::SaveState()
{
	auto state = Component::SaveState();
	state["WorkItems"] = gitlabClient->SaveState();
	state["ProjectId"] = ui.ProjectCombo->currentData().toInt();
	return state;
}

void GitlabTasks::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	gitlabClient->LoadState(state["WorkItems"].toArray());

	auto projectIdx = ui.ProjectCombo->findData(state["ProjectId"].toInt());
	if (projectIdx >= 0) {
		ui.ProjectCombo->setCurrentIndex(projectIdx);
	}
}

void GitlabTasks::RefreshWorkItems()
{
	if (isRefreshing) {
		return;
	}

	isRefreshing = true;
	ui.StatusLabel->setText("Refreshing work items...");
	gitlabClient->getWorkItems();
}

void GitlabTasks::OnWorkItemsUpdated()
{
	const auto selectedProjectId = ui.ProjectCombo->currentData().toInt();
	QSignalBlocker projectComboBlocker(ui.ProjectCombo);
	QSet<int> projectIds;
	ui.ProjectCombo->clear();

	for (const auto& workItem : gitlabClient->getCachedWorkItems()) {
		if (!projectIds.contains(workItem.ProjectId)) {
			ui.ProjectCombo->addItem(workItem.Project, workItem.ProjectId);
			projectIds.insert(workItem.ProjectId);
		}
	}

	auto projectIdx = ui.ProjectCombo->findData(selectedProjectId);
	ui.ProjectCombo->setCurrentIndex(projectIdx >= 0 ? projectIdx : 0);
	UpdateWorkItemList();

	isRefreshing = false;
	ui.StatusLabel->setText(QString("%1 work items").arg(gitlabClient->getCachedWorkItems().size()));
}

void GitlabTasks::OnProjectChanged()
{
	UpdateWorkItemList();
}

void GitlabTasks::UpdateWorkItemList()
{
	const auto selectedProjectId = ui.ProjectCombo->currentData().toInt();
	for (const auto& workItem : gitlabClient->getCachedWorkItems()) {
		if (workItem.ProjectId != selectedProjectId) {
			continue;
		}

		auto entry = ui.WorkItemsContents->findChild<WorkItemEntry*>(workItem.Id, Qt::FindDirectChildrenOnly);
		if (!entry) {
			entry = new WorkItemEntry(gitlabClient, workItem, ui.WorkItemsContents);
			entry->setObjectName(workItem.Id);
			ui.WorkItemLayout->insertWidget(ui.WorkItemLayout->indexOf(ui.BottomSpacer), entry);
		}
		else {
			entry->UpdateWorkItem(workItem);
		}
	}

	auto entries = ui.WorkItemsContents->findChildren<WorkItemEntry*>(QString{}, Qt::FindDirectChildrenOnly);
	for (auto entry : entries) {
		auto workItem = gitlabClient->getWorkItem(entry->objectName());
		if (!workItem || workItem->ProjectId != selectedProjectId) {
			ui.WorkItemLayout->removeWidget(entry);
			delete entry;
		}
	}
}

void GitlabTasks::OnWorkItemUpdated()
{
	OnWorkItemsUpdated();
	emit Modified(this);
}

void GitlabTasks::OnWorkItemsChanged()
{
	emit Modified(this);
}

void GitlabTasks::OnElapsedTimeAdded()
{
	auto toolkit = dynamic_cast<HxNxToolkit*>(window());
	if (toolkit) {
		toolkit->SaveCurrentTab();
	}
}

void GitlabTasks::OnGitlabRequestFailed(const QString& errorMessage)
{
	isRefreshing = false;
	ui.StatusLabel->setText("Refresh failed: " + errorMessage);
}
