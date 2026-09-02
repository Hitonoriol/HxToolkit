#include "GitlabMergeRequests.h"

#include "Gitlab/GitlabClient.h"
#include "Gitlab/MergeRequest.h"
#include "MergeRequestEntry.h"

#include <QComboBox>

GitlabMergeRequests::GitlabMergeRequests(QWidget* parent)
	: Component(parent, ToolType::GitlabMergeRequests)
{
	ui.setupUi(this);
	ui.RefreshButton->setIcon(QIcon(":/icons/refresh.svg"));
	ui.RefreshButton->setStyleSheet("QToolButton { border: 1px solid palette(mid); border-radius: 3px; padding: 3px; background: palette(button); }");
	gitlabClient = new GitlabClient(this);
	connect(ui.PeriodComboBox, &QComboBox::currentIndexChanged, this, &GitlabMergeRequests::RefreshMergeRequests);
	connect(ui.RefreshButton, &QToolButton::clicked, this, &GitlabMergeRequests::RefreshMergeRequests);
	connect(gitlabClient, &GitlabClient::MergeRequestsUpdated, this, &GitlabMergeRequests::OnMergeRequestsUpdated);
	connect(gitlabClient, &GitlabClient::MergeRequestUpdated, this, &GitlabMergeRequests::OnMergeRequestUpdated);
	connect(gitlabClient, &GitlabClient::requestFailed, this, &GitlabMergeRequests::OnGitlabRequestFailed);
	RefreshMergeRequests();
}

QSize GitlabMergeRequests::sizeHint() const
{
	return QSize(600, 600);
}

void GitlabMergeRequests::RefreshMergeRequests()
{
	ui.StatusLabel->setText("Refreshing merge requests...");
	ui.TotalStatsLabel->setText({});
	gitlabClient->getMergeRequests(getSelectedDays());
}

void GitlabMergeRequests::OnMergeRequestsUpdated()
{
	auto entries = ui.MergeRequestsContents->findChildren<MergeRequestEntry*>();
	for (auto entry : entries) {
		ui.MergeRequestLayout->removeWidget(entry);
		delete entry;
	}

	for (auto& mergeRequest : gitlabClient->getMergeRequests()) {
		auto entry = new MergeRequestEntry(mergeRequest, ui.MergeRequestsContents);
		entry->setObjectName(mergeRequest.Id);
		ui.MergeRequestLayout->insertWidget(ui.MergeRequestLayout->indexOf(ui.BottomSpacer), entry);
	}

	ui.StatusLabel->setText(QString("%1 merge requests").arg(gitlabClient->getMergeRequests().size()));
	UpdateTotalStats();
}

void GitlabMergeRequests::OnMergeRequestUpdated(const QString& mergeRequestId)
{
	auto entry = ui.MergeRequestsContents->findChild<MergeRequestEntry*>(mergeRequestId);
	if (!entry) {
		return;
	}

	for (auto& mergeRequest : gitlabClient->getMergeRequests()) {
		if (mergeRequest.Id == mergeRequestId) {
			entry->UpdateMergeRequest(mergeRequest);
			break;
		}
	}

	UpdateTotalStats();
}

void GitlabMergeRequests::OnGitlabRequestFailed(const QString& errorMessage)
{
	ui.StatusLabel->setText("Refresh failed: " + errorMessage);
}

int GitlabMergeRequests::getSelectedDays() const
{
	switch (ui.PeriodComboBox->currentIndex()) {
	case 0:
		return 7;

	case 1:
		return 14;

	case 2:
		return 30;

	default:
		return 0;
	}
}

void GitlabMergeRequests::UpdateTotalStats()
{
	int additions{};
	int deletions{};
	for (auto& mergeRequest : gitlabClient->getMergeRequests()) {
		additions += mergeRequest.Additions;
		deletions += mergeRequest.Deletions;
	}

	ui.TotalStatsLabel->setText(QString("<span style=\"color: #3daee9\">+%1</span> <span style=\"color: #d65d5d\">-%2</span>").arg(additions).arg(deletions));
}
