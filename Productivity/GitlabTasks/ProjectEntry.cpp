#include "ProjectEntry.h"

#include "WorkItemEntry.h"

#include <QMouseEvent>

ProjectEntry::ProjectEntry(const QString& projectName, QWidget* parent)
	: QFrame(parent)
{
	ui.setupUi(this);
	ui.ProjectNameLabel->setText(projectName);
	ui.ExpandButton->setIcon(QIcon(":/icons/expand.svg"));
	ui.WorkItemsWidget->setVisible(false);
	setStyleSheet("#ProjectEntryClass { border: 1px solid palette(mid); border-radius: 6px; background: palette(button); } QToolButton { border: 1px solid palette(mid); border-radius: 3px; padding: 4px; background: palette(button); }");
	SetInProgressTaskCount(0);
}

void ProjectEntry::SetInProgressTaskCount(int count)
{
	ui.InProgressIndicator->setVisible(count > 0);
	ui.InProgressIndicator->setToolTip(QString("%1 work items in progress").arg(count));
}

void ProjectEntry::mouseDoubleClickEvent(QMouseEvent* event)
{
	OnExpandButtonPressed();
	event->accept();
}

void ProjectEntry::AddWorkItemEntry(WorkItemEntry* workItemEntry)
{
	ui.WorkItemLayout->insertWidget(ui.WorkItemLayout->indexOf(ui.BottomSpacer), workItemEntry);
}

void ProjectEntry::OnExpandButtonPressed()
{
	bool expanded = !ui.WorkItemsWidget->isVisible();
	ui.WorkItemsWidget->setVisible(expanded);
	ui.ExpandButton->setIcon(QIcon(expanded ? ":/icons/collapse.svg" : ":/icons/expand.svg"));
}
