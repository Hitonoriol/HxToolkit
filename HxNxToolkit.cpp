#include "HxNxToolkit.h"

#include "App/SettingsWindow.h"
#include "UI/Tab.h"
#include "UI/AddToolDialog.h"
#include "UI/ComponentFactory.h"
#include "Util/Settings.h"
#include "Util/Time.h"

#include <QDateTime>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QByteArray>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QTabBar>
#include <QInputDialog>
#include <QFileInfo>

#include <algorithm>

HxNxToolkit::HxNxToolkit(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	CreateDefaultSettings();
	NewTab();
	UpdateWindowTitle();

	connect(&autosaveTimer, &QTimer::timeout, this, &HxNxToolkit::Autosave);
	autosaveTimer.start(std::chrono::milliseconds{Settings::GetInt(Option::AutosaveInterval) * 1'000});

	trayMenu = new QMenu(this);
	auto closeAction = new QAction("&Quit", this);
	connect(closeAction, &QAction::triggered, this, &HxNxToolkit::Quit);
	connect(ui.ActionQuit, &QAction::triggered, this, &HxNxToolkit::Quit);
	trayMenu->addAction(closeAction);

	QIcon icon(":/icons/icon-app.ico");
	setWindowIcon(icon);
	trayIcon = new QSystemTrayIcon(icon, this);
	trayIcon->show();
	trayIcon->setContextMenu(trayMenu);
	connect(trayIcon, &QSystemTrayIcon::activated, this, &HxNxToolkit::IconActivated);

	ui.ActionAlwaysOnTop->setChecked(Settings::GetBool(Option::AlwaysOnTop));

	ui.Tabs->tabBar()->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	connect(ui.Tabs->tabBar(), &QWidget::customContextMenuRequested, this, &HxNxToolkit::TabContextMenuRequested);

	auto launchArgs = QApplication::arguments();

	if (launchArgs.size() > 1) {
		LoadWorkspaceFromPath(launchArgs[1]);
	}
	else if (Settings::GetBool(Option::RestorePreviousSession)) {
		LoadWorkspaceFromPath(Settings::GetString(Option::LastSavedWorkspacePath));
	}

	if (Settings::GetBool(Option::WindowMaximized)) {
		showMaximized();
	} else {
		if (Settings::Contains(Option::WindowPos)) {
			move(Settings::GetPoint(Option::WindowPos));
		}

		if (Settings::Contains(Option::WindowSize)) {
			resize(Settings::GetSize(Option::WindowSize));
		}
	}
}

HxNxToolkit::~HxNxToolkit()
{}

void HxNxToolkit::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
	bool visible = this->isVisible();
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick) {
		this->setVisible(!visible);
		setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
		activateWindow();
	}
}

void HxNxToolkit::LoadComponent(ToolType componentType, const QJsonObject& state)
{
	if (auto component = ComponentFactory::CreateComponent(this, componentType)) {
		component->LoadState(state);
	}
}

Tab* HxNxToolkit::NewTab()
{
	auto tab = new Tab;
	auto title = Time::GetTimeString(QDateTime::currentDateTime());
	ui.Tabs->addTab(tab, title);
	ui.Tabs->setCurrentWidget(tab);
	connect(tab, &Tab::LoadComponent, this, &HxNxToolkit::LoadComponent);
	connect(tab, &Tab::AddToolRequested, this, [this, tab] {
		AddToolDialog dialog(ComponentFactory::AvailableTools(), tab->IsVerticalSplit(), this);
		connect(&dialog, &AddToolDialog::ToolSelected, this, [this, tab](ToolType toolType, bool verticalSplit) {
			tab->SetVerticalSplit(verticalSplit);
			ComponentFactory::CreateComponent(this, toolType);
		});
		dialog.exec();
	});
	connect(tab, &Tab::TabModified, this, &HxNxToolkit::OnTabModified);
	connect(tab, &Tab::TabSaved, this, &HxNxToolkit::OnTabSaved);
	return tab;
}

Tab* HxNxToolkit::GetCurrentTab()
{
	return dynamic_cast<Tab*>(ui.Tabs->currentWidget());
}

void HxNxToolkit::SaveCurrentWorkspace()
{
	if (workspaceModified) {
		SaveWorkspace();
	}
}

void HxNxToolkit::NewTabTriggered()
{
	NewTab();
	workspaceModified = true;
	UpdateWindowTitle();
}

void HxNxToolkit::closeEvent(QCloseEvent* event)
{
	Settings::Set(Option::WindowMaximized, isMaximized());
	Settings::Set(Option::WindowPos, pos());
	Settings::Set(Option::WindowSize, size());

	if (Settings::GetBool(Option::HideWhenClosed) && trayIcon->isVisible()) {
		hide();
		event->ignore();
		return;
	}
	if (!quitting && !ConfirmWorkspaceReplacement()) {
		event->ignore();
	}
}

void HxNxToolkit::changeEvent(QEvent* event)
{
	switch (event->type()) {
	case QEvent::WindowStateChange:
		if (Settings::GetBool(Option::HideWhenMinimized) && isMinimized()) {
			hide();
		}
		break;
	default:
		break;
	}
}

void HxNxToolkit::OnTabClose(int idx)
{
	ui.Tabs->removeTab(idx);
	workspaceModified = true;
	UpdateWindowTitle();
}

void HxNxToolkit::OnTabModified(Tab* tab)
{
	const auto index = ui.Tabs->indexOf(tab);
	if (index >= 0 && !tab->IsModified() && !ui.Tabs->tabText(index).endsWith('*')) {
		ui.Tabs->setTabText(index, ui.Tabs->tabText(index) + "*");
	}
	workspaceModified = true;
	UpdateWindowTitle();
}

void HxNxToolkit::OnTabSaved(Tab* tab)
{
	const auto index = ui.Tabs->indexOf(tab);
	if (index >= 0 && tab->IsModified() && ui.Tabs->tabText(index).endsWith('*')) {
		ui.Tabs->setTabText(index, ui.Tabs->tabText(index).chopped(1));
	}
}

void HxNxToolkit::Autosave()
{
	if (!workspacePath.isEmpty() && workspaceModified) {
		SaveWorkspaceToPath(workspacePath);
	}
}

void HxNxToolkit::NewWorkspaceTriggered()
{
	if (!ConfirmWorkspaceReplacement()) {
		return;
	}
	ClearTabs();
	workspacePath.clear();
	workspaceName = "Untitled Workspace";
	NewTab();
	workspaceModified = false;
	UpdateWindowTitle();
}

void HxNxToolkit::SaveWorkspaceTriggered()
{
	SaveWorkspace();
}

void HxNxToolkit::LoadWorkspaceTriggered()
{
	LoadWorkspace();
}

void HxNxToolkit::CloseTabTriggered()
{
	OnTabClose(ui.Tabs->currentIndex());
}

void HxNxToolkit::AlwaysOnTopToggled(bool onTop)
{
	auto flags = windowFlags();
	if (onTop) {
		flags |= Qt::WindowStaysOnTopHint;
	} else {
		flags &= ~Qt::WindowStaysOnTopHint;
	}

	setWindowFlags(flags);
	show();

	Settings::Set(Option::AlwaysOnTop, onTop);
}

void HxNxToolkit::SettingsTriggered()
{
	auto settings = new SettingsWindow(this);
	
	auto flags = settings->windowFlags();
	flags &= ~Qt::WindowMinimizeButtonHint;
	flags &= ~Qt::WindowMaximizeButtonHint;
	flags &= ~Qt::WindowCloseButtonHint;
	settings->setWindowFlags(flags);

	settings->setWindowModality(Qt::ApplicationModal);
	settings->show();
}

void HxNxToolkit::TabContextMenuRequested(const QPoint& pos)
{
	auto tabBar = ui.Tabs->tabBar();
	auto tabIdx = tabBar->tabAt(pos);
	
	if (tabIdx == -1) {
		return;
	}

	QMenu menu;

	connect(menu.addAction("Rename"), &QAction::triggered, this, [=] { TabRenameTriggered(tabIdx); });

	menu.exec(tabBar->mapToGlobal(pos));
}

void HxNxToolkit::TabRenameTriggered(int tabIdx)
{
	auto tabBar = ui.Tabs->tabBar();

	bool entered = false;
	auto newTitle = QInputDialog::getText(this, "Rename tab", "Tab title:", QLineEdit::Normal, tabBar->tabText(tabIdx), &entered);

	if (entered && !newTitle.isEmpty()) {
		SetTabTitle(tabIdx, newTitle);
	}
}

void HxNxToolkit::CreateDefaultSettings()
{
	Settings::SetDefault(Option::AutosaveInterval, 60);
	Settings::SetDefault(Option::AlwaysOnTop, false);
	Settings::SetDefault(Option::HideWhenClosed, false);
	Settings::SetDefault(Option::HideWhenMinimized, false);
	Settings::SetDefault(Option::RestorePreviousSession, false);

	// Default save path
	auto lastSaveDir = Settings::GetString(Option::LastSaveDir);
	auto savePath = lastSaveDir.isEmpty() ? QApplication::applicationDirPath() + "/Workspace" : lastSaveDir;
	QDir().mkdir(savePath);
	Settings::Set(Option::LastSaveDir, savePath);
}

bool HxNxToolkit::SaveWorkspace()
{
	if (workspacePath.isEmpty()) {
		QFileDialog dialog(this);
		dialog.setDirectory(Settings::GetString(Option::LastSaveDir));
		dialog.selectFile(workspaceName);
		dialog.setDefaultSuffix("hxnx-workspace");
		dialog.setNameFilter("HxNx Workspace File (*.hxnx-workspace)");
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
			return false;
		}
		workspacePath = dialog.selectedFiles().first();
		workspaceName = QFileInfo(workspacePath).completeBaseName();
	}
	return SaveWorkspaceToPath(workspacePath);
}

bool HxNxToolkit::SaveWorkspaceToPath(const QString& savePath)
{
	QJsonObject workspace;
	workspace["Name"] = workspaceName;
	workspace["ActiveTab"] = ui.Tabs->currentIndex();
	QJsonArray tabs;
	for (int index = 0; index < ui.Tabs->count(); ++index) {
		auto* tab = dynamic_cast<Tab*>(ui.Tabs->widget(index));
		if (!tab) {
			continue;
		}
		auto tabState = tab->SaveState();
		tabState["Title"] = ui.Tabs->tabText(index);
		tabs.append(tabState);
	}
	workspace["Tabs"] = tabs;

	QFile saveFile(savePath);
	if (!saveFile.open(QFile::WriteOnly | QFile::Truncate)) {
		QMessageBox::critical(this, "Unable to save workspace", "The workspace file could not be opened for writing.");
		return false;
	}
	saveFile.write(QJsonDocument(workspace).toJson());
	workspacePath = savePath;
	workspaceModified = false;
	Settings::Set(Option::LastSaveDir, QFileInfo(savePath).absolutePath());
	Settings::Set(Option::LastSavedWorkspacePath, savePath);
	UpdateWindowTitle();
	return true;
}

void HxNxToolkit::LoadWorkspace()
{
	if (!ConfirmWorkspaceReplacement()) {
		return;
	}
	QFileDialog dialog(this);
	dialog.setDirectory(Settings::GetString(Option::LastSaveDir));
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setNameFilter("HxNx Workspace File (*.hxnx-workspace)");
	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
		LoadWorkspaceFromPath(dialog.selectedFiles().first());
	}
}

bool HxNxToolkit::LoadWorkspaceFromPath(const QString& loadPath)
{
	if (loadPath.isEmpty()) {
		return false;
	}
	QFile workspaceFile(loadPath);
	if (!workspaceFile.open(QFile::ReadOnly)) {
		QMessageBox::critical(this, "Unable to load workspace", "The workspace file could not be opened.");
		return false;
	}
	QJsonParseError parseError;
	auto document = QJsonDocument::fromJson(workspaceFile.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		QMessageBox::critical(this, "Unable to load workspace", "The file does not contain a valid workspace document.");
		return false;
	}

	auto workspace = document.object();
	if (!workspace["Name"].isString() || !workspace["Tabs"].isArray()) {
		QMessageBox::critical(this, "Unable to load workspace", "The workspace document is missing required data.");
		return false;
	}
	auto tabStates = workspace["Tabs"].toArray();
	if (tabStates.size() > 50) {
		QMessageBox::critical(this, "Unable to load workspace", "The workspace contains too many tabs.");
		return false;
	}
	for (const auto& tabValue : tabStates) {
		auto tabState = tabValue.toObject();
		if (tabState.isEmpty() || !tabState["Title"].isString() || !tabState["Components"].isArray()) {
			QMessageBox::critical(this, "Unable to load workspace", "The workspace contains invalid tab data.");
			return false;
		}
		for (const auto& componentValue : tabState["Components"].toArray()) {
			auto component = componentValue.toObject();
			if (component.isEmpty() || !component["Type"].isString() || !component["Container"].isObject()
				|| !ToolTypeFromName(component["Type"].toString())) {
				QMessageBox::critical(this, "Unable to load workspace", "The workspace contains an unsupported component.");
				return false;
			}
		}
	}

	ClearTabs();
	for (const auto& tabValue : tabStates) {
		auto* tab = NewTab();
		auto tabState = tabValue.toObject();
		ui.Tabs->setTabText(ui.Tabs->indexOf(tab), tabState["Title"].toString());
		try {
			tab->LoadState(tabState);
		} catch (const std::exception&) {
			QMessageBox::critical(this, "Unable to load workspace", "The workspace could not be loaded.");
			return false;
		}
	}
	if (ui.Tabs->count() == 0) {
		NewTab();
	}
	ui.Tabs->setCurrentIndex(std::clamp(workspace["ActiveTab"].toInt(), 0, ui.Tabs->count() - 1));
	workspacePath = loadPath;
	workspaceName = workspace["Name"].toString();
	workspaceModified = false;
	Settings::Set(Option::LastSaveDir, QFileInfo(loadPath).absolutePath());
	Settings::Set(Option::LastSavedWorkspacePath, loadPath);
	UpdateWindowTitle();
	return true;
}

void HxNxToolkit::SetTabTitle(int tabIdx, const QString& newTitle)
{
	if (ui.Tabs->tabText(tabIdx) == newTitle) {
		return;
	}

	ui.Tabs->setTabText(tabIdx, newTitle);
	auto tab = dynamic_cast<Tab*>(ui.Tabs->widget(tabIdx));
	tab->Modify();
}

bool HxNxToolkit::ConfirmWorkspaceReplacement()
{
	if (!workspaceModified) {
		return true;
	}
	auto result = QMessageBox::question(this, "Unsaved workspace", "Save changes to the current workspace?",
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (result == QMessageBox::Cancel) {
		return false;
	}
	return result == QMessageBox::No || SaveWorkspace();
}

bool HxNxToolkit::HasModifiedTabs() const
{
	for (int index = 0; index < ui.Tabs->count(); ++index) {
		auto* tab = dynamic_cast<Tab*>(ui.Tabs->widget(index));
		if (tab && tab->IsModified()) {
			return true;
		}
	}
	return false;
}

void HxNxToolkit::ClearTabs()
{
	while (ui.Tabs->count() > 0) {
		auto* tab = ui.Tabs->widget(0);
		ui.Tabs->removeTab(0);
		tab->deleteLater();
	}
}

void HxNxToolkit::UpdateWindowTitle()
{
	setWindowTitle(QString("HxNxToolkit - %1%2").arg(workspaceName, HasModifiedTabs() ? " *" : ""));
}

void HxNxToolkit::Quit()
{
	if (!ConfirmWorkspaceReplacement()) {
		return;
	}
	quitting = true;
	QApplication::quit();
}
