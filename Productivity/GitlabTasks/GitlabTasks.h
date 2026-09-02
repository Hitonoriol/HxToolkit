#pragma once

#include <QWidget>
#include "ui_GitlabTasks.h"

#include "UI/Component.h"

class GitlabClient;

class GitlabTasks : public Component
{
	Q_OBJECT

public:
	GitlabTasks(QWidget* parent = nullptr);

	virtual QSize sizeHint() const override;
	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

public slots:
	void RefreshWorkItems();

private slots:
	void OnWorkItemsUpdated();
	void OnWorkItemsChanged();
	void OnWorkItemUpdated();
	void OnElapsedTimeAdded();
	void OnGitlabRequestFailed(const QString& errorMessage);

private:
	Ui::GitlabTasksClass ui;
	GitlabClient* gitlabClient;
	bool isRefreshing = false;
};
