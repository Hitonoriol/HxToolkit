#pragma once

#include <QObject>

#include "System/KeystrokeListener.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

class QComboBox;
class QGridLayout;
class QLineEdit;
class QPushButton;

class SystemShortcutEntry : public QObject
{
	Q_OBJECT

public:
	enum class Action
	{
		MinimizeAllWindows,
		RunCommand,
		KillCurrentWindow
	};

	explicit SystemShortcutEntry(QObject* parent = nullptr);
	~SystemShortcutEntry();

	void AddToLayout(QGridLayout* layout, int row);
	void RemoveFromLayout(QGridLayout* layout);

	QJsonObject SaveState() const;
	void LoadState(const QJsonObject& state);

signals:
	void Changed();
	void RemoveRequested(SystemShortcutEntry* entry);

private slots:
	void EditShortcut();
	void ActionChanged(int idx);
	void CommandChanged(const QString&);
	void RequestRemoval();

private:
	void RegisterShortcut();
	void Execute();
	void UpdateUi();

#ifdef Q_OS_WIN
	static BOOL CALLBACK MinimizeWindow(HWND window, LPARAM data);
	void MinimizeAllWindows();
	void KillCurrentWindow();
#endif

	KeystrokeListener::Chord chord;
	Action action{Action::MinimizeAllWindows};
	int shortcutId{};
	QPushButton* shortcutButton;
	QPushButton* removeButton;
	QComboBox* actionBox;
	QLineEdit* commandField;
};
