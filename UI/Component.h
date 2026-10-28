#pragma once

#include <QWidget>

#include "Enums/ToolType.h"

#include <QJsonObject>
#include <QList>
#include <QPointer>

class ComponentContainer;
class ToolConfigurationParameter;

class Component : public QWidget
{
	Q_OBJECT
	friend class ComponentContainer;

public:
	Component(QWidget* parent = nullptr, ToolType type = ToolType::None);
	virtual ~Component();

	ToolType GetType();
	QString GetId() const;
	QString GetTitle() const;
	ComponentContainer* GetContainer() const;
	virtual QList<ToolConfigurationParameter*> GetConfigurationParameters() const;
	virtual void OnPopOut();

	virtual QJsonObject SaveState();
	virtual void LoadState(const QJsonObject& state);

signals:
	void Modified(Component* component, bool undoable = true);

private:
	ToolType type;
	QString id;
	QPointer<ComponentContainer> container;
};

