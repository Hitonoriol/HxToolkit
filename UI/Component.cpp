#include "Component.h"
#include "ComponentContainer.h"

Component::Component(QWidget* parent, ToolType type)
	: QWidget(parent)
	, type(type)
{
}

Component::~Component()
{
}

ToolType Component::GetType()
{
	return type;
}

QJsonObject Component::SaveState()
{
	QJsonObject state;
	state["Type"] = ToolTypeName(type);

	if (container) {
		state["Container"] = container->SaveState();
	}

	return state;
}

void Component::LoadState(const QJsonObject& state)
{
	if (container) {
		container->LoadState(state["Container"].toObject());
	}
}
