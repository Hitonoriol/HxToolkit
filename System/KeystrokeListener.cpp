#include "System/KeystrokeListener.h"

#include <QCoreApplication>
#include <QKeyEvent>

#ifdef Q_OS_WIN
#include "System/WindowsKeystrokeListener.h"
#endif

KeystrokeListener* KeystrokeListener::instance{};

KeystrokeListener::KeystrokeListener(QCoreApplication* application)
	: QObject(application)
{
	instance = this;
#ifdef Q_OS_WIN
	windowsListener = std::make_unique<WindowsKeystrokeListener>(this, application);
	if (!windowsListener->IsInstalled()) {
		application->installEventFilter(this);
	}
#else
	application->installEventFilter(this);
#endif
}

KeystrokeListener::~KeystrokeListener()
{
	if (instance == this) {
		instance = nullptr;
	}
}

int KeystrokeListener::RegisterShortcut(const Chord& chord, QObject* context, std::function<void()> callback)
{
	if (!context || !callback || chord.isEmpty()) {
		return 0;
	}

	for (const auto& stroke : chord) {
		if (stroke.isEmpty()) {
			return 0;
		}
	}

	auto shortcutId = nextShortcutId++;
	shortcuts.push_back({ shortcutId, chord, context, std::move(callback) });
	return shortcutId;
}

void KeystrokeListener::UnregisterShortcut(int shortcutId)
{
	for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
		if (it->id == shortcutId) {
			shortcuts.erase(it);
			return;
		}
	}
}

KeystrokeListener* KeystrokeListener::Get()
{
	return instance;
}

bool KeystrokeListener::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
		return QObject::eventFilter(watched, event);
	}

	return ProcessKeyEvent(static_cast<QKeyEvent*>(event));
}

bool KeystrokeListener::ProcessKeyEvent(QKeyEvent* keyEvent)
{
	if (keyEvent->isAutoRepeat()) {
		return false;
	}

	auto key = static_cast<Qt::Key>(keyEvent->key());
	if (keyEvent->type() == QEvent::KeyPress) {
		if (pressedKeys.isEmpty() && lastStrokeTimer.isValid() && lastStrokeTimer.hasExpired(ChordTimeoutMs)) {
			currentChord.clear();
		}

		pressedKeys.insert(key);
		currentStroke.insert(key);
		return false;
	}

	pressedKeys.remove(key);
	if (!pressedKeys.isEmpty()) {
		return false;
	}

	currentChord.append(currentStroke);
	currentStroke.clear();
	lastStrokeTimer.restart();

	if (ActivateMatchingShortcuts() || !HasShortcutPrefix()) {
		currentChord.clear();
	}

	return false;
}

bool KeystrokeListener::HasShortcutPrefix() const
{
	for (const auto& shortcut : shortcuts) {
		if (!shortcut.context || shortcut.chord.size() < currentChord.size()) {
			continue;
		}

		bool matches = true;
		for (int idx = 0; idx < currentChord.size(); ++idx) {
			if (shortcut.chord.at(idx) != currentChord.at(idx)) {
				matches = false;
				break;
			}
		}

		if (matches) {
			return true;
		}
	}

	return false;
}

bool KeystrokeListener::ActivateMatchingShortcuts()
{
	bool activated = false;
	auto shortcutCount = shortcuts.size();
	for (size_t idx = 0; idx < shortcutCount; ++idx) {
		auto& shortcut = shortcuts.at(idx);
		if (shortcut.context && shortcut.chord == currentChord) {
			auto callback = shortcut.callback;
			callback();
			activated = true;
		}
	}

	return activated;
}
