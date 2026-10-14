#include "ComponentPopoutWindow.h"

#include <QCursor>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

ComponentPopoutWindow::ComponentPopoutWindow(QWidget* parent)
	: QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
}

bool ComponentPopoutWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
	auto nativeMessage = static_cast<MSG*>(message);

	if (nativeMessage->message == WM_NCHITTEST && !isMaximized()) {
		auto position = mapFromGlobal(QCursor::pos());
		const bool horizontalResize = minimumWidth() != maximumWidth();
		const bool verticalResize = minimumHeight() != maximumHeight();
		const bool left = horizontalResize && position.x() < ResizeMargin;
		const bool right = horizontalResize && position.x() >= width() - ResizeMargin;
		const bool top = verticalResize && position.y() < ResizeMargin;
		const bool bottom = verticalResize && position.y() >= height() - ResizeMargin;

		if (top && left) {
			*result = HTTOPLEFT;
		} else if (top && right) {
			*result = HTTOPRIGHT;
		} else if (bottom && left) {
			*result = HTBOTTOMLEFT;
		} else if (bottom && right) {
			*result = HTBOTTOMRIGHT;
		} else if (left) {
			*result = HTLEFT;
		} else if (right) {
			*result = HTRIGHT;
		} else if (top) {
			*result = HTTOP;
		} else if (bottom) {
			*result = HTBOTTOM;
		} else {
			return QWidget::nativeEvent(eventType, message, result);
		}

		return true;
	}
#endif

	return QWidget::nativeEvent(eventType, message, result);
}
