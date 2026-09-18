#pragma once

#include "UI/Component.h"
#include "ui_SystemShortcuts.h"

class SystemShortcutEntry;

class SystemShortcuts : public Component
{
	Q_OBJECT

public:
	SystemShortcuts(QWidget* parent = nullptr);
	~SystemShortcuts();

	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private slots:
	void AddEntry();
	void RemoveEntry(SystemShortcutEntry* entry);
	void EntryChanged();

private:
	void AddEntry(const QJsonObject& state);
	void UpdateLayout();

	Ui::SystemShortcutsClass ui;
	QList<SystemShortcutEntry*> entries;
};
