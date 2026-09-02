#pragma once

#include <QFrame>
#include "ui_WorkItemEntry.h"

class GitlabClient;
struct WorkItem;

class WorkItemEntry : public QFrame
{
	Q_OBJECT

public:
	WorkItemEntry(GitlabClient* gitlabClient, const WorkItem& workItem, QWidget* parent = nullptr);

	void UpdateWorkItem(const WorkItem& workItem);

private slots:
	void OnExpandButtonPressed();
	void OnTrackTimeButtonPressed();
	void OnElapsedTimer();

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
	virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
	Ui::WorkItemEntryClass ui;
	GitlabClient* gitlabClient;
	QString workItemId;
};
