#pragma once

#include <QMainWindow>
#include "ui_HxNxToolkit.h"

#include "Enums/ToolType.h"

#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>

class Tab;
class Component;

class HxNxToolkit : public QMainWindow
{
	Q_OBJECT

public:
	HxNxToolkit(QWidget *parent = nullptr);
	~HxNxToolkit();

	Tab* NewTab();
	Tab* GetCurrentTab();
	void SaveCurrentWorkspace();

	using Tool = ToolType;
	Q_ENUM(Tool)

protected:
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void changeEvent(QEvent* event) override;

public slots:
	void NewTabTriggered();
	void OnTabClose(int idx);
	void OnTabModified(Tab* tab);
	void OnTabSaved(Tab* tab);

	void Autosave();

	void NewWorkspaceTriggered();
	void SaveWorkspaceTriggered();
	void LoadWorkspaceTriggered();
	void CloseTabTriggered();

	void LoadComponent(ToolType componentType, const QJsonObject& state);

private slots:
	void IconActivated(QSystemTrayIcon::ActivationReason reason);
	void AlwaysOnTopToggled(bool onTop);
	void SettingsTriggered();
	void TabContextMenuRequested(const QPoint& pos);
	void TabRenameTriggered(int tabIdx);

private:
	void CreateDefaultSettings();

	bool SaveWorkspace();
	bool SaveWorkspaceToPath(const QString& workspacePath);
	void LoadWorkspace();
	bool LoadWorkspaceFromPath(const QString& workspacePath);
	bool ConfirmWorkspaceReplacement();
	bool IsSupportedTool(ToolType toolType) const;
	bool HasModifiedTabs() const;
	void ClearTabs();
	void UpdateWindowTitle();

	void SetTabTitle(int tabIdx, const QString& newTitle);

	void Quit();

	Ui::HxNxToolkitClass ui;

	QSystemTrayIcon* trayIcon;
	QMenu* trayMenu;

	QTimer autosaveTimer;
	QString workspacePath;
	QString workspaceName{"Untitled Workspace"};
	bool workspaceModified{};
	bool quitting{};
};
