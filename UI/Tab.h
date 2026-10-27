#pragma once

#include <QWidget>
#include "ui_Tab.h"

#include "UI/Component.h"
#include <QJsonObject>

class Tab : public QWidget
{
	Q_OBJECT

public:
	struct ComponentLocation
	{
		int Row;
		int Column;
	};

	Tab(QWidget* parent = nullptr);
	~Tab();

	void SetSavePath(const QString& savePath);
	QString GetSavePath() const;

	void AddComponent(Component* component, const QString& title = "Tool", bool fillContainer = true);
	void AttachComponent(ComponentContainer* container, const ComponentLocation& location);
	ComponentLocation DetachComponent(ComponentContainer* container);

	QJsonObject GetState() const;
	QJsonObject SaveState();
	void LoadState(const QJsonObject& state);
	void MarkSaved();
	void MarkModified();

	bool IsModified();
	bool IsVerticalSplit() const;
	void SetVerticalSplit(bool vertical);

	// For external modifications only
	void Modify();

signals:
	void AddToolRequested();
	void LoadComponent(ToolType componentType, const QJsonObject& state);

	void ComponentAdded(Tab* tab, ComponentContainer* container, const ComponentLocation& location);
	void ComponentRemoved(Tab* tab, ComponentContainer* container, const ComponentLocation& location);
	void TabModified(Tab* tab, Component* component, bool undoable);
	void TabSaved(Tab* tab);

private slots:
	void ComponentModified(Component* component, bool undoable);
	void OnComponentClosed(ComponentContainer* component);

private:
	QWidget* GetLastRow() const;
	ComponentLocation GetComponentLocation(ComponentContainer* container) const;
	void OnComponentCollapse(ComponentContainer* container, bool collapse);
	void UpdateRowStretch(QWidget* row);

	Ui::TabClass ui;

	QString savePath;
	bool modified{};
	bool verticalSplit{ true };
};
