#pragma once

#include <QWidget>
#include "ui_ComponentContainer.h"

#include "Component.h"

#include <QPointer>

class ComponentContainer : public QWidget
{
	Q_OBJECT

public:
	ComponentContainer(Component* component, QWidget *parent = nullptr);
	~ComponentContainer();

	QJsonObject SaveState();
	void LoadState(const QJsonObject& state);

	void setTitle(const QString& title);
	QString getTitle();
	void setFillContainer(bool fill);
	bool getCollapsed() const;
	Component* GetComponent() const;
	void ShowPopoutAt(const QPoint& position);

signals:
	void CloseClicked();
	void CollapseClicked(bool collapse);
	void WorkspaceLayoutChanged(QWidget* row, bool restored);

private slots:
	void OnCloseClicked();
	void OnCollapseClicked();

	void OnTitleContextMenuRequested(const QPoint& pos);
	void OnRenameTriggered();
	void OnAlwaysOnTopToggled(bool alwaysOnTop);

private:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void PopOut(bool independent = false);
	void RestoreToWorkspace();
	void UpdateCollapseButton();
	void UpdatePopoutSize(bool collapsed);
	void PopulateConfigurationMenu();

	Ui::ComponentContainerClass ui;
	QPointer<Component> component;
	QPointer<QWidget> popoutWindow;
	QPointer<QWidget> workspaceRow;
	QPointer<QWidget> workspacePlaceholder;
	QSize expandedWindowSize;
	QPoint titleDragOffset;
	int workspacePosition{};
	bool titleDragging{};
};
