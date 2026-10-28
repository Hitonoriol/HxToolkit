#include "UndoManager.h"

#include "ComponentPresenceCommand.h"
#include "ComponentStateCommand.h"
#include "TabPinCommand.h"
#include "UI/Component.h"
#include "UI/ComponentContainer.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <utility>

UndoManager::UndoManager(QWidget* owner, TabRestorer tabRestorer, CleanStateHandler cleanStateHandler)
	: QObject(owner)
	, tabRestorer(std::move(tabRestorer))
	, cleanStateHandler(std::move(cleanStateHandler))
	, undoStack(this)
{
	undoStack.setUndoLimit(100);
	qApp->installEventFilter(this);
	connect(&undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
		this->cleanStateHandler(clean);
	});
	Reset();
}

void UndoManager::ComponentAdded(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location)
{
	if (applyingState) {
		return;
	}

	SynchronizeComponent(container->GetComponent());
	undoStack.push(new ComponentPresenceCommand(this, tab, container, location, true));
}

void UndoManager::ComponentRemoved(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location)
{
	if (applyingState) {
		return;
	}

	SynchronizeComponent(container->GetComponent());
	undoStack.push(new ComponentPresenceCommand(this, tab, container, location, false));
}

void UndoManager::ComponentChanged(Component* component, bool undoable)
{
	if (applyingState || !component) {
		return;
	}

	auto afterState = component->SaveState();
	if (!componentStates.contains(component) || !undoable || IsEditingText(component)) {
		componentStates[component] = afterState;
		MarkDirty();
		return;
	}

	auto beforeState = componentStates.value(component);
	if (beforeState == afterState) {
		return;
	}

	undoStack.push(new ComponentStateCommand(this, component, beforeState, afterState));
	componentStates[component] = afterState;
}

void UndoManager::RegisterTab(Tab* tab)
{
	for (auto component : tab->findChildren<Component*>()) {
		SynchronizeComponent(component);
	}
}

void UndoManager::ApplyComponentState(Component* component, const QJsonObject& state)
{
	if (!component) {
		return;
	}

	applyingState = true;
	component->LoadState(state);
	componentStates[component] = state;
	if (auto tab = GetTab(component)) {
		tab->Modify();
	}
	applyingState = false;
}

void UndoManager::AttachComponent(Tab* tab, ComponentContainer* container, const Tab::ComponentLocation& location)
{
	applyingState = true;
	tab->AttachComponent(container, location);
	SynchronizeComponent(container->GetComponent());
	tab->Modify();
	applyingState = false;
}

Tab::ComponentLocation UndoManager::DetachComponent(Tab* tab, ComponentContainer* container)
{
	applyingState = true;
	auto location = tab->DetachComponent(container);
	tab->Modify();
	applyingState = false;
	return location;
}

void UndoManager::AddClosedTab(const QJsonObject& state, int tabIdx)
{
	closedTabs.append({tabIdx, state});

	if (closedTabs.size() > 50) {
		closedTabs.removeFirst();
	}

	MarkDirty();
}

void UndoManager::SetTabPinned(DynamicTabWidget* tabs, QWidget* page, bool pinned)
{
	if (!applyingState && tabs && page) {
		undoStack.push(new TabPinCommand(tabs, page, pinned));
	}
}

void UndoManager::MarkDirty()
{
	if (!applyingState) {
		undoStack.resetClean();
	}
}

void UndoManager::Reset()
{
	undoStack.clear();
	closedTabs.clear();
	componentStates.clear();
	auto owner = qobject_cast<QWidget*>(parent());

	if (owner) {
		for (auto component : owner->findChildren<Component*>()) {
			SynchronizeComponent(component);
		}
	}
}

void UndoManager::SetClean()
{
	undoStack.setClean();
}

bool UndoManager::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() != QEvent::KeyPress || QApplication::activeModalWidget()) {
		return QObject::eventFilter(watched, event);
	}

	auto keyEvent = static_cast<QKeyEvent*>(event);
	if (keyEvent->key() == Qt::Key_T && keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
		RestoreClosedTab();
		return true;
	}

	if (keyEvent->matches(QKeySequence::Undo)) {
		if (!UndoTextEdit()) {
			undoStack.undo();
		}

		return true;
	}

	if (keyEvent->matches(QKeySequence::Redo)) {
		if (!RedoTextEdit()) {
			undoStack.redo();
		}

		return true;
	}

	return QObject::eventFilter(watched, event);
}

bool UndoManager::UndoTextEdit()
{
	auto widget = QApplication::focusWidget();
	if (auto lineEdit = qobject_cast<QLineEdit*>(widget); lineEdit && lineEdit->isUndoAvailable()) {
		applyingState = true;
		lineEdit->undo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	if (auto plainTextEdit = qobject_cast<QPlainTextEdit*>(widget);
		plainTextEdit && plainTextEdit->document()->isUndoAvailable()) {
		applyingState = true;
		plainTextEdit->undo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	if (auto textEdit = qobject_cast<QTextEdit*>(widget); textEdit && textEdit->document()->isUndoAvailable()) {
		applyingState = true;
		textEdit->undo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	return false;
}

bool UndoManager::RedoTextEdit()
{
	auto widget = QApplication::focusWidget();
	if (auto lineEdit = qobject_cast<QLineEdit*>(widget); lineEdit && lineEdit->isRedoAvailable()) {
		applyingState = true;
		lineEdit->redo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	if (auto plainTextEdit = qobject_cast<QPlainTextEdit*>(widget);
		plainTextEdit && plainTextEdit->document()->isRedoAvailable()) {
		applyingState = true;
		plainTextEdit->redo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	if (auto textEdit = qobject_cast<QTextEdit*>(widget); textEdit && textEdit->document()->isRedoAvailable()) {
		applyingState = true;
		textEdit->redo();
		applyingState = false;
		SynchronizeComponent(GetComponent(widget));
		return true;
	}

	return false;
}

bool UndoManager::IsEditingText(Component* component) const
{
	auto widget = QApplication::focusWidget();
	if (!widget || !component->isAncestorOf(widget)) {
		return false;
	}

	return qobject_cast<QLineEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTextEdit*>(widget);
}

Component* UndoManager::GetComponent(QWidget* widget) const
{
	while (widget) {
		if (auto component = qobject_cast<Component*>(widget)) {
			return component;
		}

		widget = widget->parentWidget();
	}

	return {};
}

Tab* UndoManager::GetTab(QWidget* widget) const
{
	while (widget) {
		if (auto tab = qobject_cast<Tab*>(widget)) {
			return tab;
		}

		widget = widget->parentWidget();
	}

	return {};
}

void UndoManager::SynchronizeComponent(Component* component)
{
	if (component) {
		if (!componentStates.contains(component)) {
			connect(component, &QObject::destroyed, this, [this, component] {
				componentStates.remove(component);
			});
		}

		componentStates[component] = component->SaveState();
	}
}

void UndoManager::RestoreClosedTab()
{
	if (closedTabs.isEmpty()) {
		return;
	}

	auto [tabIdx, state] = closedTabs.takeLast();
	applyingState = true;
	tabRestorer(state, tabIdx);
	applyingState = false;
	MarkDirty();
}
