#include "System/SystemShortcuts.h"

#include "System/SystemShortcutEntry.h"

#include <QJsonArray>
#include <QPushButton>
#include <QVBoxLayout>

SystemShortcuts::SystemShortcuts(QWidget* parent)
	: Component(parent, ToolType::SystemShortcuts)
{
	ui.setupUi(this);
	connect(ui.AddButton, &QPushButton::clicked, this, static_cast<void (SystemShortcuts::*)()>(&SystemShortcuts::AddEntry));
}

SystemShortcuts::~SystemShortcuts()
{
}

QJsonObject SystemShortcuts::SaveState()
{
	auto state = Component::SaveState();
	QJsonArray shortcutStates;
	for (auto entry : entries) {
		shortcutStates.append(entry->SaveState());
	}
	state["Shortcuts"] = shortcutStates;
	return state;
}

void SystemShortcuts::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	for (auto entry : entries) {
		entry->RemoveFromLayout(ui.EntryLayout);
		entry->deleteLater();
	}
	entries.clear();
	for (auto shortcutState : state["Shortcuts"].toArray()) {
		AddEntry(shortcutState.toObject());
	}
}

void SystemShortcuts::AddEntry()
{
	AddEntry({});
	emit Modified(this);
}

void SystemShortcuts::RemoveEntry(SystemShortcutEntry* entry)
{
	entries.removeOne(entry);
	entry->RemoveFromLayout(ui.EntryLayout);
	entry->deleteLater();
	UpdateLayout();
	emit Modified(this);
}

void SystemShortcuts::EntryChanged()
{
	emit Modified(this);
}

void SystemShortcuts::AddEntry(const QJsonObject& state)
{
	auto entry = new SystemShortcutEntry(this);
	if (!state.isEmpty()) {
		entry->LoadState(state);
	}
	connect(entry, &SystemShortcutEntry::Changed, this, &SystemShortcuts::EntryChanged);
	connect(entry, &SystemShortcutEntry::RemoveRequested, this, &SystemShortcuts::RemoveEntry);
	entries.append(entry);
	UpdateLayout();
}

void SystemShortcuts::UpdateLayout()
{
	for (auto entry : entries) {
		entry->RemoveFromLayout(ui.EntryLayout);
	}

	for (int idx = 0; idx < entries.size(); ++idx) {
		entries.at(idx)->AddToLayout(ui.EntryLayout, idx);
	}
}
