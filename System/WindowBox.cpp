#include "WindowBox.h"

#include "UI/ToolConfigurationParameter.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

#include <dwmapi.h>
#include <shellapi.h>

#include <cstring>

WindowBox::WindowBox(QWidget* parent)
	: Component(parent, ToolType::WindowBox)
	, windowCombo(new QComboBox(this))
	, boxButton(new QPushButton("Box", this))
	, picker(new QWidget(this))
	, unboxAction(new ToolConfigurationParameter("Unbox", [this](QWidget*) { return Unbox(); }, this))
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto pickerLayout = new QHBoxLayout(picker);
	pickerLayout->setContentsMargins(0, 0, 0, 0);
	pickerLayout->addWidget(windowCombo, 1);
	pickerLayout->addWidget(boxButton);
	layout->addWidget(picker);
	layout->addStretch();

	windowCombo->installEventFilter(this);
	connect(boxButton, &QPushButton::clicked, this, &WindowBox::BoxSelectedWindow);

	auto windowTimer = new QTimer(this);
	windowTimer->setInterval(500);
	connect(windowTimer, &QTimer::timeout, this, [this] {
		if (boxedWindow && !IsWindow(boxedWindow)) {
			boxedWindow = {};
			Unbox();
		}
	});
	windowTimer->start();

	RefreshWindows();
}

WindowBox::~WindowBox()
{
	Unbox();
}

QList<ToolConfigurationParameter*> WindowBox::GetConfigurationParameters() const
{
	return {unboxAction};
}

bool WindowBox::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == windowCombo && event->type() == QEvent::MouseButtonPress) {
		RefreshWindows();
	}

	return Component::eventFilter(watched, event);
}

void WindowBox::RefreshWindows()
{
	const auto selectedWindow = windowCombo->currentData().toULongLong();
	windowCombo->clear();
	EnumWindows(EnumerateWindow, reinterpret_cast<LPARAM>(this));

	const auto selectedIdx = windowCombo->findData(selectedWindow);
	if (selectedIdx >= 0) {
		windowCombo->setCurrentIndex(selectedIdx);
	}

	boxButton->setEnabled(windowCombo->count() > 0);
}

BOOL CALLBACK WindowBox::EnumerateWindow(HWND window, LPARAM context)
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

	auto windowBox = reinterpret_cast<WindowBox*>(context);
	windowBox->windowCombo->addItem(GetWindowIcon(window), title, QVariant::fromValue(reinterpret_cast<qulonglong>(window)));
	return TRUE;
}

bool WindowBox::IsSelectableWindow(HWND window)
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

QIcon WindowBox::GetWindowIcon(HWND window)
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

void WindowBox::BoxSelectedWindow()
{
	auto window = reinterpret_cast<HWND>(windowCombo->currentData().toULongLong());
	if (!IsSelectableWindow(window)) {
		RefreshWindows();
		return;
	}

	WINDOWPLACEMENT placement{};
	placement.length = sizeof(placement);
	GetWindowPlacement(window, &placement);

	boxedWindow = window;
	originalParent = GetParent(window);
	originalStyle = GetWindowLongPtrW(window, GWL_STYLE);
	originalExtendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
	originalGeometry = QRect(placement.rcNormalPosition.left, placement.rcNormalPosition.top,
		placement.rcNormalPosition.right - placement.rcNormalPosition.left,
		placement.rcNormalPosition.bottom - placement.rcNormalPosition.top);
	originalShowCommand = placement.showCmd;

	ShowWindow(window, SW_RESTORE);
	foreignWindow = QWindow::fromWinId(reinterpret_cast<WId>(window));
	windowContainer = QWidget::createWindowContainer(foreignWindow, this);
	delete layout()->takeAt(1);
	layout()->addWidget(windowContainer);
	picker->hide();
	QTimer::singleShot(0, this, &WindowBox::UpdateBoxedWindowSize);
}

void WindowBox::UpdateBoxedWindowSize()
{
	if (!boxedWindow || !IsWindow(boxedWindow)) {
		return;
	}

	RECT clientRect{};
	if (!GetClientRect(boxedWindow, &clientRect)) {
		return;
	}

	DWORD_PTR result{};
	SendMessageTimeoutW(boxedWindow, WM_SIZE, SIZE_RESTORED,
		MAKELPARAM(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top),
		SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result);
}

bool WindowBox::Unbox()
{
	if (!windowContainer) {
		return false;
	}

	auto container = windowContainer.data();
	auto window = foreignWindow.data();
	if (foreignWindow) {
		foreignWindow->setParent(nullptr);
	}

	if (boxedWindow && IsWindow(boxedWindow)) {
		SetWindowLongPtrW(boxedWindow, GWL_STYLE, originalStyle);
		SetWindowLongPtrW(boxedWindow, GWL_EXSTYLE, originalExtendedStyle);
		SetParent(boxedWindow, originalParent);

		WINDOWPLACEMENT placement{};
		placement.length = sizeof(placement);
		placement.showCmd = originalShowCommand;
		placement.rcNormalPosition = {
			originalGeometry.left(), originalGeometry.top(),
			originalGeometry.right() + 1, originalGeometry.bottom() + 1
		};
		SetWindowPlacement(boxedWindow, &placement);
		SetWindowPos(boxedWindow, nullptr, 0, 0, 0, 0,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		ShowWindow(boxedWindow, originalShowCommand);
	}

	boxedWindow = {};
	foreignWindow = {};
	windowContainer = {};
	layout()->removeWidget(container);
	delete container;
	delete window;
	picker->show();
	static_cast<QVBoxLayout*>(layout())->addStretch();
	RefreshWindows();
	return true;
}
