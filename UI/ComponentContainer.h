#pragma once

#include <QWidget>
#include "ui_ComponentContainer.h"

#include "Component.h"

#include <QPointer>

namespace oclero::qlementine
{
class Expander;
}

class ComponentContainer : public QWidget
{
	Q_OBJECT

public:
	ComponentContainer(Component* component, QWidget *parent = nullptr);
	~ComponentContainer();

	QJsonObject SaveState();
	void LoadState(const QJsonObject& state);

	void setTitle(const QString& title);
	QString getTitle();

signals:
	void CloseClicked();
	void UpClicked();
	void DownClicked();
	void CollapseClicked(bool collapse);

private slots:
	void OnCollapseClicked();

	void OnTitleContextMenuRequested(const QPoint& pos);
	void OnRenameTriggered();

private:
	void UpdateCollapseButton();

	Ui::ComponentContainerClass ui;
	QPointer<Component> component;
	QPointer<oclero::qlementine::Expander> expander;
};
