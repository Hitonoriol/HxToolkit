#include "ScreenColorPicker.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>

std::function<void(QColor)> ScreenColorPicker::callback;
std::vector<ScreenColorPicker*> ScreenColorPicker::activePickers;

void ScreenColorPicker::Pick(std::function<void(QColor)> callback)
{
	ScreenColorPicker::callback = std::move(callback);
	for (auto screen : QGuiApplication::screens()) {
		auto picker = new ScreenColorPicker(screen);
		activePickers.push_back(picker);
		picker->show();
		picker->raise();
	}

	activePickers.back()->activateWindow();
	activePickers.back()->grabKeyboard();
}

ScreenColorPicker::ScreenColorPicker(QScreen* screen)
	: QWidget(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
	setGeometry(screen->geometry());
	screenshot = screen->grabWindow(0);
	screenshotImage = screenshot.toImage();
	cursorPosition = mapFromGlobal(QCursor::pos());
	hasCursor = rect().contains(cursorPosition);
	setMouseTracking(true);
	setCursor(Qt::CrossCursor);
}

void ScreenColorPicker::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::MouseButton::LeftButton) {
		auto pixelPosition = event->position() * screenshot.devicePixelRatio();
		Finish(screenshotImage.pixelColor(pixelPosition.toPoint()));
		return;
	}

	Finish({});
}

void ScreenColorPicker::mouseMoveEvent(QMouseEvent* event)
{
	cursorPosition = event->position().toPoint();
	hasCursor = true;
	update();
}

void ScreenColorPicker::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape) {
		Finish({});
		return;
	}

	QWidget::keyPressEvent(event);
}

void ScreenColorPicker::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.drawPixmap(QPoint{}, screenshot);

	if (hasCursor) {
		DrawMagnifier(&painter);
	}
}

void ScreenColorPicker::DrawMagnifier(QPainter* painter)
{
	auto magnifierSize = MagnifierPixelCount * MagnifierPixelSize;
	auto targetPosition = cursorPosition + QPoint(24, 24);
	if (targetPosition.x() + magnifierSize > width()) {
		targetPosition.setX(cursorPosition.x() - magnifierSize - 24);
	}

	if (targetPosition.y() + magnifierSize > height()) {
		targetPosition.setY(cursorPosition.y() - magnifierSize - 24);
	}

	targetPosition.setX(std::max(0, targetPosition.x()));
	targetPosition.setY(std::max(0, targetPosition.y()));

	auto pixelPosition = (QPointF(cursorPosition) * screenshot.devicePixelRatio()).toPoint();
	auto sourcePosition = pixelPosition - QPoint(MagnifierPixelCount / 2, MagnifierPixelCount / 2);
	sourcePosition.setX(std::clamp(sourcePosition.x(), 0, screenshotImage.width() - MagnifierPixelCount));
	sourcePosition.setY(std::clamp(sourcePosition.y(), 0, screenshotImage.height() - MagnifierPixelCount));

	auto sourceRect = QRect(sourcePosition, QSize(MagnifierPixelCount, MagnifierPixelCount));
	auto targetRect = QRect(targetPosition, QSize(magnifierSize, magnifierSize));
	painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
	painter->drawImage(targetRect, screenshotImage, sourceRect);

	painter->setPen(QPen(QColor(0, 0, 0, 180)));
	for (int idx = 0; idx <= MagnifierPixelCount; ++idx) {
		auto offset = idx * MagnifierPixelSize;
		painter->drawLine(targetRect.left() + offset, targetRect.top(), targetRect.left() + offset, targetRect.bottom());
		painter->drawLine(targetRect.left(), targetRect.top() + offset, targetRect.right(), targetRect.top() + offset);
	}

	painter->setPen(QPen(Qt::white, 2));
	painter->drawRect(targetRect);

	auto highlightedPosition = pixelPosition - sourcePosition;
	auto highlightedRect = QRect(
		targetRect.topLeft() + highlightedPosition * MagnifierPixelSize,
		QSize(MagnifierPixelSize, MagnifierPixelSize));
	painter->setPen(QPen(Qt::red, 2));
	painter->drawRect(highlightedRect);
}

void ScreenColorPicker::Finish(const QColor& color)
{
	for (auto picker : activePickers) {
		picker->releaseMouse();
		picker->releaseKeyboard();
		picker->hide();
		picker->deleteLater();
	}

	activePickers.clear();
	auto colorCallback = std::move(callback);
	callback = {};
	colorCallback(color);
}
