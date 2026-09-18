#pragma once

#include <QWidget>
#include "ui_Tab.h"

#include "UI/Component.h"
#include <QJsonObject>

class Tab : public QWidget
{
	Q_OBJECT

public:
	Tab(QWidget* parent = nullptr);
	~Tab();

	void SetSavePath(const QString& savePath);
	QString GetSavePath() const;

	void AddComponent(Component* component, const QString& title = "Tool", bool fillContainer = true);

	QJsonObject SaveState();
	void LoadState(const QJsonObject& state);

	bool IsModified();

	// For external modifications only
	void Modify();

signals:
	void LoadComponent(ToolType componentType, const QJsonObject& state);

	void TabModified(Tab* tab);
	void TabSaved(Tab* tab);

private slots:
	void ComponentModified(Component* component);
	void OnComponentClosed(ComponentContainer* component);

private:
	QWidget* GetLastRow() const;
	void OnComponentCollapse(ComponentContainer* container, bool collapse);
	void UpdateRowStretch(QWidget* row);

	Ui::TabClass ui;

	QString savePath;
	bool modified{};
};
