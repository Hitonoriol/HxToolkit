#pragma once

#include <QJsonObject>
#include <QPointer>
#include <QUndoCommand>

class Component;
class UndoManager;

class ComponentStateCommand : public QUndoCommand
{
public:
	ComponentStateCommand(UndoManager* undoManager, Component* component, const QJsonObject& beforeState,
		const QJsonObject& afterState);

	void undo() override;
	void redo() override;

private:
	UndoManager* undoManager;
	QPointer<Component> component;
	QJsonObject beforeState;
	QJsonObject afterState;
	bool firstRedo{true};
};
