#include "ComponentPresenceCommand.h"

#include "UndoManager.h"
#include "UI/ComponentContainer.h"

ComponentPresenceCommand::ComponentPresenceCommand(UndoManager* undoManager, Tab* tab,
	ComponentContainer* container, const Tab::ComponentLocation& location, bool addition)
	: QUndoCommand(addition ? "Add tool" : "Remove tool")
	, undoManager(undoManager)
	, tab(tab)
	, container(container)
	, location(location)
	, addition(addition)
{
}

ComponentPresenceCommand::~ComponentPresenceCommand()
{
	if (container && !container->parent()) {
		delete container;
	}
}

void ComponentPresenceCommand::undo()
{
	SetAttached(!addition);
}

void ComponentPresenceCommand::redo()
{
	if (firstRedo) {
		firstRedo = false;
		return;
	}

	SetAttached(addition);
}

void ComponentPresenceCommand::SetAttached(bool attached)
{
	if (!tab || !container) {
		return;
	}

	if (attached) {
		undoManager->AttachComponent(tab, container, location);
	} else {
		location = undoManager->DetachComponent(tab, container);
	}
}
