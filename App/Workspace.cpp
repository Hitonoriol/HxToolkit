#include "Workspace.h"

#include "UI/Component.h"
#include "UI/Tab.h"

Workspace::Workspace(QObject* parent)
	: QObject(parent)
{
}

QString Workspace::GetName() const
{
	return name;
}

void Workspace::SetName(const QString& name)
{
	this->name = name;
}

QString Workspace::GetPath() const
{
	return path;
}

void Workspace::SetPath(const QString& path)
{
	this->path = path;
}

bool Workspace::GetModified() const
{
	return modified;
}

void Workspace::SetModified(bool modified)
{
	this->modified = modified;
}

void Workspace::RegisterComponent(Tab* tab, Component* component)
{
	if (!tab || !component || GetTab(component)) {
		return;
	}

	components.append({tab, component});
	emit ComponentsChanged();
}

void Workspace::UnregisterComponent(Component* component)
{
	bool removed = false;
	for (auto it = components.begin(); it != components.end();) {
		if (!it->component || it->component == component) {
			it = components.erase(it);
			removed = true;
		} else {
			++it;
		}
	}

	if (removed) {
		emit ComponentsChanged();
	}
}

void Workspace::RemoveTab(Tab* tab)
{
	bool removed = false;
	for (auto it = components.begin(); it != components.end();) {
		if (!it->tab || it->tab == tab) {
			it = components.erase(it);
			removed = true;
		} else {
			++it;
		}
	}

	if (removed) {
		emit ComponentsChanged();
	}
}

QList<Component*> Workspace::GetComponents() const
{
	QList<Component*> result;
	for (const auto& record : components) {
		if (record.tab && record.component) {
			result.append(record.component);
		}
	}

	return result;
}

Component* Workspace::FindComponent(ToolType type) const
{
	for (auto component : GetComponents()) {
		if (component->GetType() == type) {
			return component;
		}
	}

	return {};
}

Component* Workspace::FindComponent(const QString& id) const
{
	for (auto component : GetComponents()) {
		if (component->GetId() == id) {
			return component;
		}
	}

	return {};
}

Tab* Workspace::GetTab(Component* component) const
{
	for (const auto& record : components) {
		if (record.component == component) {
			return record.tab;
		}
	}

	return {};
}

void Workspace::NotifyComponentsChanged()
{
	emit ComponentsChanged();
}
