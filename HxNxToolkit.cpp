#include "HxNxToolkit.h"

#include "App/SettingsWindow.h"
#include "UI/Tab.h"
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

HxNxToolkit::HxNxToolkit(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	ComponentFactory::Register(this);
	CreateDefaultSettings();
	auto defaultTab = NewTab();

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
		LoadTab(defaultTab, launchArgs[1]);
	}
	else if (Settings::GetBool(Option::RestorePreviousSession)) {
		auto path = Settings::GetString(Option::LastSavedTabPath);
		LoadTab(defaultTab, path);
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
	curTabIdx = ui.Tabs->addTab(tab, title);
	ui.Tabs->setCurrentWidget(tab);
	connect(tab, &Tab::LoadComponent, this, &HxNxToolkit::LoadComponent);
	connect(tab, &Tab::TabModified, this, &HxNxToolkit::OnTabModified);
	connect(tab, &Tab::TabSaved, this, &HxNxToolkit::OnTabSaved);
	return tab;
}

Tab* HxNxToolkit::GetCurrentTab()
{
	return dynamic_cast<Tab*>(ui.Tabs->currentWidget());
}

void HxNxToolkit::SaveCurrentTab()
{
	auto tab = GetCurrentTab();
	if (tab && tab->IsModified()) {
		SaveTab();
	}
}

QAction* HxNxToolkit::AddComponentMenuAction(const QString& categoryName, const QString& componentName)
{
	auto menus = ui.MenuBar->findChildren<QMenu*>();
	auto categoryIt = std::find_if(menus.begin(), menus.end(), [=](QMenu* menu) {
		return menu->title() == categoryName;
	});

	QMenu* category{};
	if (categoryIt == menus.end()) {
		category = new QMenu(categoryName, ui.MenuBar);
		category->setObjectName(categoryName);
		ui.MenuBar->addMenu(category);
	} else {
		category = *categoryIt;
	}

	auto componentAction = new QAction(componentName, category);
	category->addAction(componentAction);
	return componentAction;
}

void HxNxToolkit::NewTabTriggered()
{
	NewTab();
}

void HxNxToolkit::closeEvent(QCloseEvent* event)
{
	Settings::Set(Option::WindowMaximized, isMaximized());
	Settings::Set(Option::WindowPos, pos());
	Settings::Set(Option::WindowSize, size());

	if (Settings::GetBool(Option::HideWhenClosed) && trayIcon->isVisible()) {
		hide();
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
	auto tab = dynamic_cast<Tab*>(ui.Tabs->widget(idx));
	if (tab && tab->IsModified()) {
		auto result = QMessageBox::question(
			this, "Closing \"" + ui.Tabs->tabText(idx) + "\"",
			"This tab has unsaved changes. Save now?",
			QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No | QMessageBox::StandardButton::Cancel
		);

		switch (result) {
		case QMessageBox::StandardButton::Cancel:
			return;

		case QMessageBox::StandardButton::Yes:
			if (!SaveTab(idx)) {
				return;
			}
			break;

		default:
			break;
		} 
	}

	ui.Tabs->removeTab(idx);
}

void HxNxToolkit::OnTabModified(Tab* tab)
{
	auto idx = ui.Tabs->indexOf(tab);
	if (!tab->IsModified()) {
		ui.Tabs->setTabText(idx, ui.Tabs->tabText(idx) + "*");
	}
}

void HxNxToolkit::OnTabSaved(Tab* tab)
{
	auto idx = ui.Tabs->indexOf(tab);
	if (tab->IsModified()) {
		ui.Tabs->setTabText(idx, ui.Tabs->tabText(idx).removeLast());
	}
}

void HxNxToolkit::Autosave()
{
	for (size_t i = 0; i < ui.Tabs->count(); ++i) {
		auto tab = dynamic_cast<Tab*>(ui.Tabs->widget(i));
		if (!tab->GetSavePath().isEmpty() && tab->IsModified()) {
			SaveTab(i);
		}
	}
}

void HxNxToolkit::SaveTabTriggered()
{
	SaveTab();
}

void HxNxToolkit::LoadTabTriggered()
{
	LoadTab();
}

void HxNxToolkit::CloseTabTriggered()
{
	ui.Tabs->removeTab(ui.Tabs->currentIndex());
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

bool HxNxToolkit::SaveTab(int idx)
{
	if (idx == -1) {
		return false;
	}

	auto tab = dynamic_cast<Tab*>(ui.Tabs->widget(idx));
	auto title = GetTabTitle(idx);
	
	QFile saveFile;
	auto savePath = tab->GetSavePath();
	
	if (savePath.isEmpty()) {
		auto suggestedName = title;
		suggestedName.replace(QRegularExpression("[^a-zA-Z0-9\\s]"), "-");
		suggestedName.replace("--", "-");

		QFileDialog dialog(this);
		dialog.setDirectory({Settings::GetString(Option::LastSaveDir)});
		dialog.selectFile(suggestedName);
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setNameFilter("HxNx Tab File (*.hxnx-tab)");
		dialog.setAcceptMode(QFileDialog::AcceptSave);

		auto result = dialog.exec();
		auto files = dialog.selectedFiles();

		auto newPath = files.isEmpty() ? QString("") : files.first();
		if (!result || newPath.isEmpty()) {
			return false;
		}

		savePath = newPath;
		saveFile.setFileName(newPath);
		tab->SetSavePath(newPath);

		auto tabName = std::filesystem::path(newPath.toStdWString()).filename().replace_extension().wstring();
		title = QString::fromStdWString(tabName);
	} else {
		saveFile.setFileName(savePath);
	}

	if (!saveFile.open(QFile::WriteOnly)) {
		tab->SetSavePath("");
		return false;
	}

	SetTabTitle(idx, title);

	auto tabJson = tab->SaveState();
	tabJson["Title"] = title;

	ui.Tabs->setTabText(idx, title);

	saveFile.write(QJsonDocument(tabJson).toJson());
	Settings::Set(Option::LastSaveDir, QFileInfo(savePath).absolutePath());
	Settings::Set(Option::LastSavedTabPath, savePath);
	saveFile.close();
	return true;
}

bool HxNxToolkit::SaveTab()
{
	auto currentTab = ui.Tabs->currentIndex();
	return SaveTab(currentTab);
}

void HxNxToolkit::LoadTab()
{
	QFileDialog dialog(this);
	dialog.setDirectory({Settings::GetString(Option::LastSaveDir)});
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setNameFilter("HxNx Tab File (*.hxnx-tab)");
	dialog.setAcceptMode(QFileDialog::AcceptOpen);
	
	auto result = dialog.exec();
	auto files = dialog.selectedFiles();

	auto tabPath = files.isEmpty() ? QString("") : files.first();
	if (!result || tabPath.isEmpty()) {
		return;
	}

	auto tab = NewTab();
	if (!LoadTab(tab, tabPath)) {
		ui.Tabs->removeTab(ui.Tabs->indexOf(tab));
		tab->deleteLater();
	}
}

bool HxNxToolkit::LoadTab(Tab* tab, const QString& tabPath)
{
	QFile tabFile(tabPath);
	if (!tabFile.open(QFile::ReadOnly)) {
		ShowTabLoadError("The file could not be opened.");
		return false;
	}

	QJsonParseError parseError;
	auto tabDoc = QJsonDocument::fromJson(tabFile.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !tabDoc.isObject()) {
		ShowTabLoadError("The file does not contain a valid tab document.");
		return false;
	}

	auto tabObject = tabDoc.object();
	if (!tabObject["Title"].isString() || !tabObject["Components"].isArray()) {
		ShowTabLoadError("The tab document is missing required data.");
		return false;
	}

	auto components = tabObject["Components"].toArray();
	if (components.size() > 50) {
		ShowTabLoadError("The tab document contains too many components.");
		return false;
	}

	for (auto componentValue : components) {
		auto componentObject = componentValue.toObject();
		if (componentObject.isEmpty() || !componentObject["Type"].isDouble() || !componentObject["Container"].isObject()) {
			ShowTabLoadError("The tab document has invalid component data.");
			return false;
		}

		auto componentType = static_cast<ToolType>(componentObject["Type"].toInt());
		switch (componentType) {
		case ToolType::BaseConverter:
		case ToolType::Calculator:
		case ToolType::MarkdownEditor:
		case ToolType::Checklist:
		case ToolType::TaskTracker:
		case ToolType::GitlabTasks:
		case ToolType::GitlabMergeRequests:
		case ToolType::GitlabTimeStats:
		case ToolType::Stopwatch:
		case ToolType::Timer:
		case ToolType::RandomNumber:
		case ToolType::RandomString:
		case ToolType::FileSearch:
		case ToolType::RamMonitor:
		case ToolType::ClipboardManager:
		case ToolType::SystemShortcuts:
			break;

		default:
			ShowTabLoadError("The tab document contains an unsupported component.");
			return false;
		}
	}

	try {
		tab->LoadState(tabObject);
	}
	catch (const std::exception&) {
		ShowTabLoadError("The tab document could not be loaded.");
		return false;
	}

	ui.Tabs->setTabText(ui.Tabs->currentIndex(), tabObject["Title"].toString());
	tab->SetSavePath(tabPath);
	Settings::Set(Option::LastSavedTabPath, tabPath);
	return true;
}

void HxNxToolkit::ShowTabLoadError(const QString& errorMessage)
{
	QMessageBox::critical(this, "Unable to load tab", errorMessage);
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

QString HxNxToolkit::GetTabTitle(int tabIdx)
{
	auto tab = dynamic_cast<Tab*>(ui.Tabs->widget(tabIdx));
	auto title = ui.Tabs->tabText(tabIdx);

	if (tab->IsModified()) {
		title.removeLast();
	}
	
	return title;
}

void HxNxToolkit::Quit()
{
	for (size_t i = 0; i < ui.Tabs->count(); ++i) {
		OnTabClose(i);
	}

	QApplication::quit();
}
