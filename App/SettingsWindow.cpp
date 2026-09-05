#include "SettingsWindow.h"
#include "Gitlab/GitlabClient.h"
#include "Util/Settings.h"

#include <QMessageBox>

SettingsWindow::SettingsWindow(QWidget *parent)
	: QWidget(parent, Qt::Window)
{
	ui.setupUi(this);
	gitlabClient = new GitlabClient(this);
	connect(gitlabClient, &GitlabClient::WorkItemsUpdated, this, &SettingsWindow::OnWorkItemsUpdated);
	connect(gitlabClient, &GitlabClient::requestFailed, this, &SettingsWindow::OnGitlabRequestFailed);
	ui.MinimizeToTrayBox->setChecked(Settings::GetBool(Option::HideWhenMinimized));
	ui.CloseToTrayBox->setChecked(Settings::GetBool(Option::HideWhenClosed));
	ui.RestoreSessionBox->setChecked(Settings::GetBool(Option::RestorePreviousSession));
	ui.AutosaveIntervalBox->setValue(Settings::GetInt(Option::AutosaveInterval));
	ui.GitlabUrlField->setText(Settings::GetString(Option::GitlabUrl));
	ui.GitlabTokenField->setText(Settings::GetString(Option::GitlabToken));
}

void SettingsWindow::OnTestGitlabConnection()
{
	Settings::Set(Option::GitlabUrl, ui.GitlabUrlField->text().trimmed());
	Settings::Set(Option::GitlabToken, ui.GitlabTokenField->text());
	gitlabClient->getWorkItems();
}

void SettingsWindow::OnWorkItemsUpdated()
{
	QMessageBox::information(this, "GitLab", QString("Retrieved %1 assigned work items.").arg(gitlabClient->getCachedWorkItems().size()));
}

void SettingsWindow::OnGitlabRequestFailed(const QString& errorMessage)
{
	QMessageBox::critical(this, "GitLab", errorMessage);
}

SettingsWindow::~SettingsWindow()
{}

void SettingsWindow::OnApply()
{
	Settings::Set(Option::AutosaveInterval, ui.AutosaveIntervalBox->value());
	Settings::Set(Option::HideWhenClosed, ui.CloseToTrayBox->isChecked());
	Settings::Set(Option::HideWhenMinimized, ui.MinimizeToTrayBox->isChecked());
	Settings::Set(Option::RestorePreviousSession, ui.RestoreSessionBox->isChecked());
	Settings::Set(Option::GitlabUrl, ui.GitlabUrlField->text().trimmed());
	Settings::Set(Option::GitlabToken, ui.GitlabTokenField->text());
	close();
}

void SettingsWindow::OnCancel()
{
	close();
}
