#pragma once

#include <QFrame>
#include "ui_ProjectEntry.h"

class WorkItemEntry;

class ProjectEntry : public QFrame
{
	Q_OBJECT

public:
	ProjectEntry(const QString& projectName, QWidget* parent = nullptr);

	void AddWorkItemEntry(WorkItemEntry* workItemEntry);
	void SetInProgressTaskCount(int count);

private slots:
	void OnExpandButtonPressed();

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
	Ui::ProjectEntryClass ui;
};
