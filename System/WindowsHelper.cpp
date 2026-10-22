#include "WindowsHelper.h"

#include <QImage>
#include <QPixmap>

#include <dwmapi.h>
#include <shellapi.h>

#include <cstring>

namespace WindowsHelper
{
bool IsSelectableWindow(HWND window)
{
	if (!IsWindowVisible(window) || window == GetShellWindow() || GetWindowTextLengthW(window) == 0) {
		return false;
	}

	DWORD processId{};
	GetWindowThreadProcessId(window, &processId);
	if (processId == GetCurrentProcessId()) {
		return false;
	}

	BOOL cloaked = FALSE;
	if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
		return false;
	}

	const auto style = GetWindowLongPtrW(window, GWL_STYLE);
	const auto extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
	if ((style & WS_CHILD) || ((extendedStyle & WS_EX_TOOLWINDOW) && !(extendedStyle & WS_EX_APPWINDOW))
		|| ((extendedStyle & WS_EX_NOACTIVATE) && !(extendedStyle & WS_EX_APPWINDOW))) {
		return false;
	}

	auto rootOwner = GetAncestor(window, GA_ROOTOWNER);
	auto lastVisiblePopup = rootOwner;
	while (true) {
		auto popup = GetLastActivePopup(lastVisiblePopup);
		if (popup == lastVisiblePopup) {
			break;
		}

		lastVisiblePopup = popup;
		if (IsWindowVisible(lastVisiblePopup)) {
			break;
		}
	}

	return window == lastVisiblePopup;
}

static QIcon GetWindowIcon(HWND window)
{
	HICON icon{};
	DWORD_PTR iconResult{};
	SendMessageTimeoutW(window, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &iconResult);
	icon = reinterpret_cast<HICON>(iconResult);

	if (!icon) {
		icon = reinterpret_cast<HICON>(GetClassLongPtrW(window, GCLP_HICONSM));
	}

	bool destroyIcon = false;
	if (!icon) {
		DWORD processId{};
		GetWindowThreadProcessId(window, &processId);
		auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (process) {
			wchar_t executablePath[MAX_PATH];
			DWORD pathLength = MAX_PATH;
			if (QueryFullProcessImageNameW(process, 0, executablePath, &pathLength)) {
				SHFILEINFOW fileInfo{};
				if (SHGetFileInfoW(executablePath, FILE_ATTRIBUTE_NORMAL, &fileInfo, sizeof(fileInfo),
					SHGFI_ICON | SHGFI_SMALLICON)) {
					icon = fileInfo.hIcon;
					destroyIcon = true;
				}
			}

			CloseHandle(process);
		}
	}

	if (!icon) {
		return {};
	}

	BITMAPINFO bitmapInfo{};
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = 32;
	bitmapInfo.bmiHeader.biHeight = -32;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	void* pixels{};
	auto deviceContext = CreateCompatibleDC(nullptr);
	auto bitmap = CreateDIBSection(deviceContext, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
	if (!deviceContext || !bitmap || !pixels) {
		DeleteObject(bitmap);
		DeleteDC(deviceContext);

		if (destroyIcon) {
			DestroyIcon(icon);
		}

		return {};
	}

	auto previousBitmap = SelectObject(deviceContext, bitmap);
	memset(pixels, 0, 32 * 32 * 4);
	DrawIconEx(deviceContext, 0, 0, icon, 32, 32, 0, nullptr, DI_NORMAL);
	auto image = QImage(static_cast<uchar*>(pixels), 32, 32, QImage::Format_ARGB32_Premultiplied).copy();
	SelectObject(deviceContext, previousBitmap);
	DeleteObject(bitmap);
	DeleteDC(deviceContext);

	if (destroyIcon) {
		DestroyIcon(icon);
	}

	return QIcon(QPixmap::fromImage(image));
}

static BOOL CALLBACK EnumerateWindow(HWND window, LPARAM context)
{
	if (!IsSelectableWindow(window)) {
		return TRUE;
	}

	const auto titleLength = GetWindowTextLengthW(window);
	QString title(titleLength + 1, Qt::Uninitialized);
	const auto copiedLength = GetWindowTextW(window, reinterpret_cast<wchar_t*>(title.data()), title.size());
	title.resize(copiedLength);
	if (title.isEmpty()) {
		return TRUE;
	}

	auto windows = reinterpret_cast<QList<WindowInfo>*>(context);
	windows->append({window, title, GetWindowIcon(window)});
	return TRUE;
}

QList<WindowInfo> GetWindows()
{
	QList<WindowInfo> windows;
	EnumWindows(EnumerateWindow, reinterpret_cast<LPARAM>(&windows));
	return windows;
}

bool IsAlwaysOnTop(HWND window)
{
	return IsWindow(window) && (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST);
}

bool SetAlwaysOnTop(HWND window, bool alwaysOnTop)
{
	if (!IsWindow(window)) {
		return false;
	}

	const auto insertAfter = alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
	if (!SetWindowPos(window, insertAfter, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
		return false;
	}

	return IsAlwaysOnTop(window) == alwaysOnTop;
}
}
