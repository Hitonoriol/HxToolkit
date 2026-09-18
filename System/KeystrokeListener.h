#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QSet>

#include <functional>
#include <memory>
#include <vector>

class QCoreApplication;
class QKeyEvent;

#ifdef Q_OS_WIN
class WindowsKeystrokeListener;
#endif

class KeystrokeListener : public QObject
{
	Q_OBJECT

public:
	using KeyStroke = QSet<Qt::Key>;
	using Chord = QList<KeyStroke>;

	explicit KeystrokeListener(QCoreApplication* application);
	~KeystrokeListener();

	int RegisterShortcut(const Chord& chord, QObject* context, std::function<void()> callback);
	void UnregisterShortcut(int shortcutId);

	static KeystrokeListener* Get();

	static constexpr int ChordTimeoutMs = 750;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
#ifdef Q_OS_WIN
	friend class WindowsKeystrokeListener;
#endif

	bool ProcessKeyEvent(QKeyEvent* event);

	struct Shortcut
	{
		int id;
		Chord chord;
		QPointer<QObject> context;
		std::function<void()> callback;
	};

	bool HasShortcutPrefix() const;
	bool ActivateMatchingShortcuts();

	QSet<Qt::Key> pressedKeys;
	KeyStroke currentStroke;
	Chord currentChord;
	QElapsedTimer lastStrokeTimer;
	std::vector<Shortcut> shortcuts;
	int nextShortcutId{1};

	static KeystrokeListener* instance;

#ifdef Q_OS_WIN
	std::unique_ptr<WindowsKeystrokeListener> windowsListener;
#endif
};
