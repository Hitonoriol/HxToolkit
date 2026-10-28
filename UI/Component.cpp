#include "Component.h"
#include "ComponentContainer.h"

#include <QUuid>

Component::Component(QWidget* parent, ToolType type)
	: QWidget(parent)
	, type(type)
	, id(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Component::~Component()
{
}

ToolType Component::GetType()
{
	return type;
}

QString Component::GetId() const
{
	return id;
}

QString Component::GetTitle() const
{
	return container ? container->getTitle() : QString{};
}

ComponentContainer* Component::GetContainer() const
{
	return container;
}

QList<ToolConfigurationParameter*> Component::GetConfigurationParameters() const
{
	return {};
}

void Component::OnPopOut()
{
}

QJsonObject Component::SaveState()
{
	QJsonObject state;
	state["Type"] = ToolTypeName(type);
	state["Id"] = id;

	if (container) {
		state["Container"] = container->SaveState();
	}

	return state;
}

void Component::LoadState(const QJsonObject& state)
{
	if (!state["Id"].toString().isEmpty()) {
		id = state["Id"].toString();
	}

	if (container) {
		container->LoadState(state["Container"].toObject());
	}
}
