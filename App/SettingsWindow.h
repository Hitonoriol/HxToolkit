#pragma once

#include <QWidget>
#include "ui_SettingsWindow.h"

class GitlabClient;

class SettingsWindow : public QWidget
{
	Q_OBJECT

public:
	SettingsWindow(QWidget *parent = nullptr);
	~SettingsWindow();

private slots:
	void OnApply();
	void OnCancel();
	void OnTestGitlabConnection();
	void OnWorkItemsUpdated();
	void OnGitlabRequestFailed(const QString& errorMessage);

private:
	Ui::SettingsWindowClass ui;
	GitlabClient* gitlabClient;
};
