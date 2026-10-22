#pragma once

#include <QIcon>
#include <QList>
#include <QString>
#include <qt_windows.h>

namespace WindowsHelper
{
struct WindowInfo
{
	HWND Handle;
	QString Title;
	QIcon Icon;
};

QList<WindowInfo> GetWindows();
bool IsSelectableWindow(HWND window);
bool IsAlwaysOnTop(HWND window);
bool SetAlwaysOnTop(HWND window, bool alwaysOnTop);
}
