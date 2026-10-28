#pragma once

#include "Enums/ToolType.h"

#include <QList>
#include <QObject>
#include <QPointer>

class Component;
class Tab;

class Workspace : public QObject
{
	Q_OBJECT

public:
	explicit Workspace(QObject* parent = nullptr);

	QString GetName() const;
	void SetName(const QString& name);
	QString GetPath() const;
	void SetPath(const QString& path);
	bool GetModified() const;
	void SetModified(bool modified);

	void RegisterComponent(Tab* tab, Component* component);
	void UnregisterComponent(Component* component);
	void RemoveTab(Tab* tab);
	QList<Component*> GetComponents() const;
	Component* FindComponent(ToolType type) const;
	Component* FindComponent(const QString& id) const;
	Tab* GetTab(Component* component) const;
	void NotifyComponentsChanged();

signals:
	void ComponentsChanged();

private:
	struct ComponentRecord
	{
		QPointer<Tab> tab;
		QPointer<Component> component;
	};

	QList<ComponentRecord> components;
	QString name{"Untitled Workspace"};
	QString path;
	bool modified{};
};
