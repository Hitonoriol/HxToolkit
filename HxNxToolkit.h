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
	void SaveCurrentTab();

	QAction* AddComponentMenuAction(const QString& categoryName, const QString& componentName);

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

	void SaveTabTriggered();
	void LoadTabTriggered();
	void CloseTabTriggered();

	void LoadComponent(ToolType componentType, const QJsonObject& state);

private slots:
	void IconActivated(QSystemTrayIcon::ActivationReason reason);
	void AlwaysOnTopToggled(bool onTop);
	void SettingsTriggered();
	void TabContextMenuRequested(const QPoint& pos);
	void TabRenameTriggered(int tabIdx);
	void TabExpandWidgetsTriggered(int tabIdx);

private:
	void CreateDefaultSettings();

	bool SaveTab(int idx);
	bool SaveTab();
	void LoadTab();
	void LoadTab(Tab* tab, const QString& tabPath);

	void SetTabTitle(int tabIdx, const QString& newTitle);
	QString GetTabTitle(int tabIdx);

	void Quit();

	Ui::HxNxToolkitClass ui;

	QSystemTrayIcon* trayIcon;
	QMenu* trayMenu;

	QTimer autosaveTimer;
	int curTabIdx{};
};
