#pragma once

#include <QWidget>

#include "UI/Component.h"
#include "ui_GitlabMergeRequests.h"

class GitlabClient;

class GitlabMergeRequests : public Component
{
	Q_OBJECT

public:
	GitlabMergeRequests(QWidget* parent = nullptr);

	virtual QSize sizeHint() const override;

private slots:
	void RefreshMergeRequests();
	void OnMergeRequestsUpdated();
	void OnMergeRequestUpdated(const QString& mergeRequestId);
	void OnGitlabRequestFailed(const QString& errorMessage);

private:
	int getSelectedDays() const;
	void UpdateTotalStats();

private:
	Ui::GitlabMergeRequestsClass ui;
	GitlabClient* gitlabClient;
};
