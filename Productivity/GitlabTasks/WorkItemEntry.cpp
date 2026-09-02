#include "WorkItemEntry.h"

#include "Gitlab/GitlabClient.h"
#include "Gitlab/WorkItem.h"

#include <QMessageBox>
#include <QMouseEvent>
#include <QTimer>

WorkItemEntry::WorkItemEntry(GitlabClient* gitlabClient, const WorkItem& workItem, QWidget* parent)
	: QFrame(parent)
	, gitlabClient(gitlabClient)
{
	ui.setupUi(this);
	ui.headerLayout->setStretch(1, 1);
	ui.titleLayout->setStretch(0, 1);
	ui.ExpandButton->setIcon(QIcon(":/icons/expand.svg"));
	ui.TrackTimeButton->setIcon(QIcon(":/icons/play.svg"));
	ui.TrackTimeButton->installEventFilter(this);
	ui.DescriptionValueBrowser->setOpenExternalLinks(true);
	ui.ReferenceLabel->setOpenExternalLinks(true);
	ui.DetailsWidget->setVisible(false);
	setStyleSheet("#WorkItemEntryClass { border: 1px solid palette(mid); border-radius: 5px; background: palette(base); } #DetailsWidget { border-top: 1px solid palette(mid); } QToolButton { border: 1px solid palette(mid); border-radius: 3px; padding: 4px; background: palette(button); }");

	auto elapsedTimer = new QTimer(this);
	connect(elapsedTimer, &QTimer::timeout, this, &WorkItemEntry::OnElapsedTimer);
	elapsedTimer->start(1'000);

	UpdateWorkItem(workItem);
}

void WorkItemEntry::UpdateWorkItem(const WorkItem& workItem)
{
	workItemId = workItem.Id;
	ui.ReferenceLabel->setText(QString("<a href=\"%1\">%2</a>").arg(workItem.WebUrl.toHtmlEscaped(), workItem.Reference.toHtmlEscaped()));
	ui.TitleLabel->setText(workItem.Title);
	ui.DescriptionValueBrowser->setMarkdown(workItem.Description);
	ui.ElapsedValueLabel->setText(workItem.getElapsedTimeString());
	ui.TrackTimeButton->setIcon(QIcon(workItem.isTracking() ? ":/icons/pause.svg" : ":/icons/play.svg"));
	ui.TrackTimeButton->setToolTip(workItem.isTracking() ? "Pause time tracking" : "Start time tracking");
	ui.TrackTimeButton->setEnabled(!workItem.isSubmittingElapsedTime());
}

void WorkItemEntry::OnExpandButtonPressed()
{
	bool expanded = !ui.DetailsWidget->isVisible();
	ui.DetailsWidget->setVisible(expanded);
	ui.ExpandButton->setIcon(QIcon(expanded ? ":/icons/collapse.svg" : ":/icons/expand.svg"));
}

void WorkItemEntry::OnTrackTimeButtonPressed()
{
	auto workItem = gitlabClient->getWorkItem(workItemId);
	if (!workItem) {
		return;
	}

	if (workItem->isTracking()) {
		gitlabClient->PauseTracking(workItemId);
	}
	else {
		gitlabClient->StartTracking(workItemId);
	}
}

void WorkItemEntry::OnElapsedTimer()
{
	auto workItem = gitlabClient->getWorkItem(workItemId);
	if (workItem) {
		ui.ElapsedValueLabel->setText(workItem->getElapsedTimeString());
	}
}

void WorkItemEntry::mouseDoubleClickEvent(QMouseEvent* event)
{
	OnExpandButtonPressed();
	event->accept();
}

bool WorkItemEntry::eventFilter(QObject* watched, QEvent* event)
{
	if (watched != ui.TrackTimeButton || event->type() != QEvent::MouseButtonPress) {
		return QFrame::eventFilter(watched, event);
	}

	auto mouseEvent = static_cast<QMouseEvent*>(event);
	if (mouseEvent->button() != Qt::RightButton) {
		return QFrame::eventFilter(watched, event);
	}

	auto workItem = gitlabClient->getWorkItem(workItemId);
	if (!workItem || !workItem->isTracking()) {
		return true;
	}

	auto answer = QMessageBox::question(this, "Discard tracked time", "Are you sure you want to discard tracked time?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer == QMessageBox::Yes) {
		gitlabClient->DiscardTracking(workItemId);
	}

	return true;
}
