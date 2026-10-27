#pragma once

#include "UI/Tab.h"

#include <QPointer>
#include <QUndoCommand>

class ComponentContainer;
class UndoManager;

class ComponentPresenceCommand : public QUndoCommand
{
public:
	ComponentPresenceCommand(UndoManager* undoManager, Tab* tab, ComponentContainer* container,
		const Tab::ComponentLocation& location, bool addition);
	~ComponentPresenceCommand();

	void undo() override;
	void redo() override;

private:
	void SetAttached(bool attached);

	UndoManager* undoManager;
	QPointer<Tab> tab;
	QPointer<ComponentContainer> container;
	Tab::ComponentLocation location;
	bool addition;
	bool firstRedo{true};
};
