#include "System/SystemShortcutEntry.h"

#include "System/KeystrokeEditorDialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>

#include <functional>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

SystemShortcutEntry::SystemShortcutEntry(QObject* parent)
	: QObject(parent)
{
	shortcutButton = new QPushButton("Click to set shortcut");
	connect(shortcutButton, &QPushButton::clicked, this, &SystemShortcutEntry::EditShortcut);
	actionBox = new QComboBox;
	actionBox->addItem("Minimize All Windows", static_cast<int>(Action::MinimizeAllWindows));
	actionBox->addItem("Run command", static_cast<int>(Action::RunCommand));
	connect(actionBox, &QComboBox::currentIndexChanged, this, &SystemShortcutEntry::ActionChanged);
	commandField = new QLineEdit;
	commandField->setPlaceholderText("Command");
	connect(commandField, &QLineEdit::textChanged, this, &SystemShortcutEntry::CommandChanged);
	removeButton = new QPushButton("Remove");
	connect(removeButton, &QPushButton::clicked, this, &SystemShortcutEntry::RequestRemoval);
	UpdateUi();
}

SystemShortcutEntry::~SystemShortcutEntry()
{
	if (auto listener = KeystrokeListener::Get()) {
		listener->UnregisterShortcut(shortcutId);
	}
	delete shortcutButton;
	delete removeButton;
	delete actionBox;
	delete commandField;
}

void SystemShortcutEntry::AddToLayout(QGridLayout* layout, int row)
{
	layout->addWidget(shortcutButton, row, 0);
	layout->addWidget(actionBox, row, 1);
	layout->addWidget(commandField, row, 2);
	layout->addWidget(removeButton, row, 3);
}

void SystemShortcutEntry::RemoveFromLayout(QGridLayout* layout)
{
	layout->removeWidget(shortcutButton);
	layout->removeWidget(actionBox);
	layout->removeWidget(commandField);
	layout->removeWidget(removeButton);
}

QJsonObject SystemShortcutEntry::SaveState() const
{
	QJsonObject state;
	state["Action"] = static_cast<int>(action);
	state["Command"] = commandField->text();
	QJsonArray strokes;
	for (const auto& stroke : chord) {
		QJsonArray keys;
		for (auto key : stroke) {
			keys.append(static_cast<int>(key));
		}
		strokes.append(keys);
	}
	state["Chord"] = strokes;
	return state;
}

void SystemShortcutEntry::LoadState(const QJsonObject& state)
{
	chord.clear();
	for (auto strokeValue : state["Chord"].toArray()) {
		KeystrokeListener::KeyStroke stroke;
		for (auto keyValue : strokeValue.toArray()) {
			stroke.insert(static_cast<Qt::Key>(keyValue.toInt()));
		}
		if (!stroke.isEmpty()) {
			chord.append(stroke);
		}
	}
	action = static_cast<Action>(state["Action"].toInt());
	actionBox->setCurrentIndex(actionBox->findData(static_cast<int>(action)));
	commandField->setText(state["Command"].toString());
	UpdateUi();
	RegisterShortcut();
}

void SystemShortcutEntry::EditShortcut()
{
	KeystrokeEditorDialog dialog(chord, shortcutButton);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	chord = dialog.GetChord();
	UpdateUi();
	RegisterShortcut();
	emit Changed();
}

void SystemShortcutEntry::ActionChanged(int idx)
{
	action = static_cast<Action>(actionBox->itemData(idx).toInt());
	UpdateUi();
	RegisterShortcut();
	emit Changed();
}

void SystemShortcutEntry::CommandChanged(const QString&)
{
	RegisterShortcut();
	emit Changed();
}

void SystemShortcutEntry::RequestRemoval()
{
	emit RemoveRequested(this);
}

void SystemShortcutEntry::RegisterShortcut()
{
	auto listener = KeystrokeListener::Get();
	if (!listener) {
		return;
	}
	listener->UnregisterShortcut(shortcutId);
	shortcutId = chord.isEmpty() ? 0 : listener->RegisterShortcut(chord, this, std::bind(&SystemShortcutEntry::Execute, this));
}

void SystemShortcutEntry::Execute()
{
	if (action == Action::MinimizeAllWindows) {
#ifdef Q_OS_WIN
		MinimizeAllWindows();
#endif
		return;
	}

	if (!commandField->text().isEmpty()) {
		auto command = QProcess::splitCommand(commandField->text());
		auto program = command.takeFirst();
		QProcess::startDetached(program, command);
	}
}

void SystemShortcutEntry::UpdateUi()
{
	shortcutButton->setText(chord.isEmpty() ? "Click to set shortcut" : KeystrokeEditorDialog::GetChordText(chord));
	commandField->setVisible(action == Action::RunCommand);
}

#ifdef Q_OS_WIN
BOOL CALLBACK SystemShortcutEntry::MinimizeWindow(HWND window, LPARAM data)
{
	if (IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
		ShowWindowAsync(window, SW_MINIMIZE);
	}
	return TRUE;
}

void SystemShortcutEntry::MinimizeAllWindows()
{
	EnumWindows(MinimizeWindow, 0);
}
#endif
