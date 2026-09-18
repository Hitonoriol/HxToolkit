#pragma once

#include <QObject>

#ifdef Q_OS_WIN

#include <Windows.h>

class KeystrokeListener;
class QCoreApplication;

class WindowsKeystrokeListener : public QObject
{
	Q_OBJECT

public:
	WindowsKeystrokeListener(KeystrokeListener* listener, QCoreApplication* application);
	~WindowsKeystrokeListener();

	bool IsInstalled() const;

protected:
	bool event(QEvent* event) override;

private:
	static LRESULT CALLBACK KeyboardProc(int code, WPARAM message, LPARAM data);
	static Qt::Key GetQtKey(DWORD virtualKey);
	static Qt::KeyboardModifiers GetModifiers();

	static WindowsKeystrokeListener* instance;

	KeystrokeListener* listener;
	HHOOK hook{};
};

#endif
