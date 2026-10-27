#pragma once

#include "UI/Tab.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QUndoStack>

#include <functional>

class Component;
class ComponentContainer;
class QWidget;

class UndoManager : public QObject
{
public:
	using TabRestorer = std::function<void(const QJsonObject&, int)>;
	using CleanStateHandler = std::function<void(bool)>;

	UndoManager(QWidget* owner, TabRestorer tabRestorer, CleanStateHandler cleanStateHandler);

	void ComponentAdded(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location);
	void ComponentRemoved(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location);
	void ComponentChanged(Component* component, bool undoable);
	void RegisterTab(Tab* tab);
	void ApplyComponentState(Component* component, const QJsonObject& state);
	void AttachComponent(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location);
	Tab::ComponentLocation DetachComponent(Tab* tab, ComponentContainer* container);
	void AddClosedTab(const QJsonObject& state, int tabIdx);
	void MarkDirty();
	void Reset();
	void SetClean();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	bool UndoTextEdit();
	bool RedoTextEdit();
	bool IsEditingText(Component* component) const;
	Component* GetComponent(QWidget* widget) const;
	Tab* GetTab(QWidget* widget) const;
	void SynchronizeComponent(Component* component);
	void RestoreClosedTab();

	TabRestorer tabRestorer;
	CleanStateHandler cleanStateHandler;
	QUndoStack undoStack;
	QHash<Component*, QJsonObject> componentStates;
	QList<QPair<int, QJsonObject>> closedTabs;
	bool applyingState{};
};
