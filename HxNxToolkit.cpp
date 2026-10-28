#include "HxNxToolkit.h"

#include "App/SettingsWindow.h"
#include "App/UndoManager.h"
#include "App/Workspace.h"
#include "UI/Tab.h"
#include "UI/DynamicTabWidget.h"
#include "UI/ComponentContainer.h"
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
#include <QPainter>

#include <algorithm>

HxNxToolkit::HxNxToolkit(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	workspace = new Workspace(this);
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
	connect(trayIcon, &QSystemTrayIcon::messageClicked, this, &HxNxToolkit::NotificationClicked);
	connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
		if (state == Qt::ApplicationActive) {
			trayIcon->setIcon(windowIcon());
		}
	});

	ui.ActionAlwaysOnTop->setChecked(Settings::GetBool(Option::AlwaysOnTop));

	ui.Tabs->GetTabBar()->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	connect(ui.Tabs->GetTabBar(), &QWidget::customContextMenuRequested, this, &HxNxToolkit::TabContextMenuRequested);
	connect(ui.Tabs, &DynamicTabWidget::AddTabRequested, ui.ActionNewTab, &QAction::trigger);
	connect(ui.Tabs, &DynamicTabWidget::CurrentChanged, this, [this](int tabIdx) {
		if (tabIdx >= 0) {
			ui.Tabs->SetTabIcon(tabIdx, {});
		}
	});
	connect(ui.Tabs->GetTabBar(), &QTabBar::tabMoved, this, [this] {
		workspace->SetModified(true);
		UpdateWindowTitle();
		if (undoManager) {
			undoManager->MarkDirty();
		}
	});

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

	undoManager = new UndoManager(this,
		[this](const QJsonObject& state, int tabIdx) { RestoreClosedTab(state, tabIdx); },
		[this](bool clean) { UndoCleanStateChanged(clean); });
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

void HxNxToolkit::NotificationClicked()
{
	show();
	setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
	raise();
	activateWindow();
	trayIcon->setIcon(windowIcon());
}

void HxNxToolkit::LoadComponent(ToolType componentType, const QJsonObject& state)
{
	if (auto component = ComponentFactory::CreateComponent(this, componentType)) {
		component->LoadState(state);
		workspace->NotifyComponentsChanged();
	}
}

Tab* HxNxToolkit::NewTab()
{
	auto tab = new Tab;
	auto title = Time::GetTimeString(QDateTime::currentDateTime());
	ui.Tabs->AddTab(tab, title);
	ui.Tabs->SetCurrentWidget(tab);
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
	connect(tab, &Tab::ComponentAttached, this, [this](Tab* componentTab, Component* component) {
		workspace->RegisterComponent(componentTab, component);
	});
	connect(tab, &Tab::ComponentDetached, workspace, &Workspace::UnregisterComponent);
	connect(tab, &Tab::ComponentAdded, this,
		[this](Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location) {
			if (undoManager) {
				undoManager->ComponentAdded(tab, container, location);
			}

			tab->MarkModified();
			OnTabModified(tab, container->GetComponent());
		});
	connect(tab, &Tab::ComponentRemoved, this,
		[this](Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location) {
			if (undoManager) {
				undoManager->ComponentRemoved(tab, container, location);
			}

			tab->MarkModified();
			OnTabModified(tab, container->GetComponent());
		});
	return tab;
}

Tab* HxNxToolkit::GetCurrentTab()
{
	return dynamic_cast<Tab*>(ui.Tabs->GetCurrentWidget());
}

Workspace* HxNxToolkit::GetWorkspace() const
{
	return workspace;
}

void HxNxToolkit::ShowComponent(Component* component)
{
	if (!component) {
		return;
	}

	auto container = component->GetContainer();
	if (container && container->window() != this) {
		container->ShowPopoutAt(container->window()->geometry().center());
		return;
	}

	if (auto tab = workspace->GetTab(component)) {
		ui.Tabs->SetCurrentWidget(tab);
	}

	show();
	setWindowState(windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
	raise();
	activateWindow();
}

void HxNxToolkit::SaveCurrentWorkspace()
{
	if (workspace->GetModified()) {
		SaveWorkspace();
	}
}

void HxNxToolkit::ShowNotification(Component* component, const QString& title, const QString& message)
{
	auto widget = component->parentWidget();
	while (widget) {
		if (auto tab = dynamic_cast<Tab*>(widget)) {
			const auto tabIdx = ui.Tabs->GetIndexOf(tab);
			if (tabIdx >= 0 && tabIdx != ui.Tabs->GetCurrentIndex()) {
				auto tabIcon = QPixmap(10, 10);
				tabIcon.fill(Qt::transparent);
				QPainter painter(&tabIcon);
				painter.setPen(Qt::NoPen);
				painter.setBrush(Qt::red);
				painter.drawEllipse(0, 0, tabIcon.width(), tabIcon.height());
				painter.end();
				ui.Tabs->SetTabIcon(tabIdx, tabIcon);
			}

			break;
		}

		widget = widget->parentWidget();
	}

	if (QApplication::applicationState() != Qt::ApplicationActive) {
		auto icon = windowIcon().pixmap(32, 32);
		QPainter painter(&icon);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::red);
		painter.drawEllipse(icon.width() - 12, icon.height() - 12, 10, 10);
		trayIcon->setIcon(icon);
	}

	trayIcon->showMessage(title, message, QSystemTrayIcon::Information);
}

void HxNxToolkit::NewTabTriggered()
{
	NewTab();
	workspace->SetModified(true);
	UpdateWindowTitle();
	undoManager->MarkDirty();
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
	if (idx < 0 || idx >= ui.Tabs->GetCount() || ui.Tabs->GetTabPinned(idx)) {
		return;
	}

	auto tab = dynamic_cast<Tab*>(ui.Tabs->GetWidget(idx));
	if (undoManager && tab) {
		auto state = tab->GetState();
		auto title = ui.Tabs->GetTabText(idx);
		if (title.endsWith('*')) {
			title.chop(1);
		}
		state["Title"] = title;
		state["Modified"] = tab->IsModified();
		undoManager->AddClosedTab(state, idx);
	}

	ui.Tabs->RemoveTab(idx);
	if (tab) {
		workspace->RemoveTab(tab);
		tab->deleteLater();
	}
	workspace->SetModified(true);
	UpdateWindowTitle();
}

void HxNxToolkit::OnTabModified(Tab* tab, Component* component, bool undoable)
{
	const auto tabIdx = ui.Tabs->GetIndexOf(tab);
	if (tabIdx >= 0 && tab->IsModified() && !ui.Tabs->GetTabText(tabIdx).endsWith('*')) {
		ui.Tabs->SetTabText(tabIdx, ui.Tabs->GetTabText(tabIdx) + "*");
	}
	workspace->SetModified(true);
	workspace->NotifyComponentsChanged();
	UpdateWindowTitle();
	if (undoManager) {
		if (component) {
			undoManager->ComponentChanged(component, undoable);
		} else {
			undoManager->MarkDirty();
		}
	}
}

void HxNxToolkit::OnTabSaved(Tab* tab)
{
	const auto tabIdx = ui.Tabs->GetIndexOf(tab);
	if (tabIdx >= 0 && tab->IsModified() && ui.Tabs->GetTabText(tabIdx).endsWith('*')) {
		ui.Tabs->SetTabText(tabIdx, ui.Tabs->GetTabText(tabIdx).chopped(1));
	}
}

void HxNxToolkit::Autosave()
{
	if (!workspace->GetPath().isEmpty() && workspace->GetModified()) {
		SaveWorkspaceToPath(workspace->GetPath());
	}
}

void HxNxToolkit::NewWorkspaceTriggered()
{
	if (!ConfirmWorkspaceReplacement()) {
		return;
	}
	ClearTabs();
	workspace->SetPath({});
	workspace->SetName("Untitled Workspace");
	NewTab();
	workspace->SetModified(false);
	UpdateWindowTitle();
	undoManager->Reset();
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
	OnTabClose(ui.Tabs->GetCurrentIndex());
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
	auto tabBar = ui.Tabs->GetTabBar();
	auto tabIdx = tabBar->tabAt(pos);
	
	if (tabIdx == -1) {
		return;
	}

	QMenu menu;

	connect(menu.addAction(ui.Tabs->GetTabPinned(tabIdx) ? "Unpin" : "Pin"), &QAction::triggered,
		this, [this, tabIdx] { TabPinTriggered(tabIdx); });
	connect(menu.addAction("Rename"), &QAction::triggered, this, [this, tabIdx] { TabRenameTriggered(tabIdx); });

	menu.exec(tabBar->mapToGlobal(pos));
}

void HxNxToolkit::TabPinTriggered(int tabIdx)
{
	if (undoManager) {
		undoManager->SetTabPinned(ui.Tabs, ui.Tabs->GetWidget(tabIdx), !ui.Tabs->GetTabPinned(tabIdx));
	}
}

void HxNxToolkit::TabRenameTriggered(int tabIdx)
{
	auto tabBar = ui.Tabs->GetTabBar();

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
	if (workspace->GetPath().isEmpty()) {
		QFileDialog dialog(this);
		dialog.setDirectory(Settings::GetString(Option::LastSaveDir));
		dialog.selectFile(workspace->GetName());
		dialog.setDefaultSuffix("hxnx-workspace");
		dialog.setNameFilter("HxNx Workspace File (*.hxnx-workspace)");
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
			return false;
		}
		workspace->SetPath(dialog.selectedFiles().first());
		workspace->SetName(QFileInfo(workspace->GetPath()).completeBaseName());
	}
	return SaveWorkspaceToPath(workspace->GetPath());
}

bool HxNxToolkit::SaveWorkspaceToPath(const QString& savePath)
{
	QJsonObject workspace;
	workspace["Name"] = this->workspace->GetName();
	workspace["ActiveTab"] = ui.Tabs->GetCurrentIndex();
	QJsonArray tabs;
	for (int tabIdx = 0; tabIdx < ui.Tabs->GetCount(); ++tabIdx) {
		auto tab = dynamic_cast<Tab*>(ui.Tabs->GetWidget(tabIdx));
		if (!tab) {
			continue;
		}
		auto tabState = tab->SaveState();
		tabState["Title"] = ui.Tabs->GetTabText(tabIdx);
		tabState["Pinned"] = ui.Tabs->GetTabPinned(tabIdx);
		tabs.append(tabState);
	}
	workspace["Tabs"] = tabs;

	QFile saveFile(savePath);
	if (!saveFile.open(QFile::WriteOnly | QFile::Truncate)) {
		QMessageBox::critical(this, "Unable to save workspace", "The workspace file could not be opened for writing.");
		return false;
	}
	saveFile.write(QJsonDocument(workspace).toJson());
	this->workspace->SetPath(savePath);
	this->workspace->SetModified(false);
	Settings::Set(Option::LastSaveDir, QFileInfo(savePath).absolutePath());
	Settings::Set(Option::LastSavedWorkspacePath, savePath);
	UpdateWindowTitle();
	if (undoManager) {
		undoManager->SetClean();
	}
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
		auto tab = NewTab();
		auto tabState = tabValue.toObject();
		auto tabIdx = ui.Tabs->GetIndexOf(tab);
		ui.Tabs->SetTabText(tabIdx, tabState["Title"].toString());
		ui.Tabs->SetTabPinned(tabIdx, tabState["Pinned"].toBool());
		try {
			tab->LoadState(tabState);
		} catch (const std::exception&) {
			QMessageBox::critical(this, "Unable to load workspace", "The workspace could not be loaded.");
			return false;
		}
	}
	if (ui.Tabs->GetCount() == 0) {
		NewTab();
	}
	ui.Tabs->SetCurrentIndex(std::clamp(workspace["ActiveTab"].toInt(), 0, ui.Tabs->GetCount() - 1));
	this->workspace->SetPath(loadPath);
	this->workspace->SetName(workspace["Name"].toString());
	this->workspace->SetModified(false);
	Settings::Set(Option::LastSaveDir, QFileInfo(loadPath).absolutePath());
	Settings::Set(Option::LastSavedWorkspacePath, loadPath);
	UpdateWindowTitle();
	if (undoManager) {
		undoManager->Reset();
	}
	return true;
}

void HxNxToolkit::SetTabTitle(int tabIdx, const QString& newTitle)
{
	if (ui.Tabs->GetTabText(tabIdx) == newTitle) {
		return;
	}

	ui.Tabs->SetTabText(tabIdx, newTitle);
	auto tab = dynamic_cast<Tab*>(ui.Tabs->GetWidget(tabIdx));
	tab->Modify();
}

void HxNxToolkit::RestoreClosedTab(const QJsonObject& state, int tabIdx)
{
	auto tab = NewTab();
	ui.Tabs->RemoveTab(ui.Tabs->GetIndexOf(tab));
	tabIdx = std::clamp(tabIdx, 0, ui.Tabs->GetCount());
	ui.Tabs->InsertTab(tabIdx, tab, state["Title"].toString());
	ui.Tabs->SetCurrentWidget(tab);
	tab->LoadState(state);

	if (state["Modified"].toBool()) {
		tab->Modify();
	}
	undoManager->RegisterTab(tab);

	workspace->SetModified(true);
	UpdateWindowTitle();
}

void HxNxToolkit::UndoCleanStateChanged(bool clean)
{
	workspace->SetModified(!clean);

	if (clean) {
		for (int tabIdx = 0; tabIdx < ui.Tabs->GetCount(); ++tabIdx) {
			auto tab = dynamic_cast<Tab*>(ui.Tabs->GetWidget(tabIdx));
			if (tab) {
				tab->MarkSaved();
			}
		}
	}

	UpdateWindowTitle();
}

bool HxNxToolkit::ConfirmWorkspaceReplacement()
{
	if (!workspace->GetModified()) {
		return true;
	}
	auto result = QMessageBox::question(this, "Unsaved workspace", "Save changes to the current workspace?",
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (result == QMessageBox::Cancel) {
		return false;
	}
	return result == QMessageBox::No || SaveWorkspace();
}

void HxNxToolkit::ClearTabs()
{
	while (ui.Tabs->GetCount() > 0) {
		auto tab = ui.Tabs->GetWidget(0);
		ui.Tabs->RemoveTab(0);
		workspace->RemoveTab(static_cast<Tab*>(tab));
		tab->deleteLater();
	}
}

void HxNxToolkit::UpdateWindowTitle()
{
	setWindowTitle(QString("HxNxToolkit - %1%2")
		.arg(workspace->GetName(), workspace->GetModified() ? " *" : ""));
}

void HxNxToolkit::Quit()
{
	if (!ConfirmWorkspaceReplacement()) {
		return;
	}
	quitting = true;
	QApplication::quit();
}
