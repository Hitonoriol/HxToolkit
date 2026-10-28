#pragma once

#include <QIcon>
#include <QList>
#include <QPoint>
#include <QString>
#include <qt_windows.h>

#include <optional>

namespace WindowsHelper
{
struct WindowInfo
{
	HWND Handle;
	QString Title;
	QIcon Icon;
};

QList<WindowInfo> GetWindows();
std::optional<QPoint> GetTextCursorPosition();
bool IsSelectableWindow(HWND window);
bool IsAlwaysOnTop(HWND window);
bool SetAlwaysOnTop(HWND window, bool alwaysOnTop);
}
