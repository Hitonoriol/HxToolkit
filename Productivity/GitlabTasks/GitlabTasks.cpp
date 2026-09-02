#include "GitlabTasks.h"

#include "Gitlab/GitlabClient.h"
#include "Gitlab/WorkItem.h"
#include "HxNxToolkit.h"
#include "ProjectEntry.h"
#include "WorkItemEntry.h"

#include <QJsonArray>
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
	return state;
}

void GitlabTasks::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	gitlabClient->LoadState(state["WorkItems"].toArray());
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
	for (auto& workItem : gitlabClient->getCachedWorkItems()) {
		auto projectId = QString::number(workItem.ProjectId);
		auto projectEntry = ui.WorkItemsContents->findChild<ProjectEntry*>(projectId, Qt::FindDirectChildrenOnly);
		if (!projectEntry) {
			projectEntry = new ProjectEntry(workItem.Project, ui.WorkItemsContents);
			projectEntry->setObjectName(projectId);
			ui.WorkItemLayout->insertWidget(ui.WorkItemLayout->indexOf(ui.BottomSpacer), projectEntry);
		}

		auto entry = projectEntry->findChild<WorkItemEntry*>(workItem.Id);
		if (!entry) {
			entry = new WorkItemEntry(gitlabClient, workItem, projectEntry);
			entry->setObjectName(workItem.Id);
			projectEntry->AddWorkItemEntry(entry);
		}
		else {
			entry->UpdateWorkItem(workItem);
		}
	}

	auto entries = ui.WorkItemsContents->findChildren<WorkItemEntry*>();
	for (auto entry : entries) {
		if (!gitlabClient->getWorkItem(entry->objectName())) {
			ui.WorkItemLayout->removeWidget(entry);
			delete entry;
		}
	}

	auto projects = ui.WorkItemsContents->findChildren<ProjectEntry*>(QString{}, Qt::FindDirectChildrenOnly);
	for (auto project : projects) {
		int inProgressTaskCount{};
		for (auto& workItem : gitlabClient->getCachedWorkItems()) {
			if (QString::number(workItem.ProjectId) == project->objectName() && workItem.isTracking()) {
				++inProgressTaskCount;
			}
		}

		project->SetInProgressTaskCount(inProgressTaskCount);
		if (project->findChildren<WorkItemEntry*>().isEmpty()) {
			ui.WorkItemLayout->removeWidget(project);
			delete project;
		}
	}

	isRefreshing = false;
	ui.StatusLabel->setText(QString("%1 work items").arg(gitlabClient->getCachedWorkItems().size()));
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
