#include "ComponentStateCommand.h"

#include "UndoManager.h"

ComponentStateCommand::ComponentStateCommand(UndoManager* undoManager, Component* component,
	const QJsonObject& beforeState, const QJsonObject& afterState)
	: QUndoCommand("Modify tool")
	, undoManager(undoManager)
	, component(component)
	, beforeState(beforeState)
	, afterState(afterState)
{
}

void ComponentStateCommand::undo()
{
	undoManager->ApplyComponentState(component, beforeState);
}

void ComponentStateCommand::redo()
{
	if (firstRedo) {
		firstRedo = false;
		return;
	}

	undoManager->ApplyComponentState(component, afterState);
}

