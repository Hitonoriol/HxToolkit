#pragma once

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include <functional>
#include <vector>

class QScreen;
class QPainter;

class ScreenColorPicker : public QWidget
{
public:
	static void Pick(std::function<void(QColor)> callback);

protected:
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;

private:
	ScreenColorPicker(QScreen* screen);

	void DrawMagnifier(QPainter* painter);
	void Finish(const QColor& color);

	QPixmap screenshot;
	QImage screenshotImage;
	QPoint cursorPosition;
	bool hasCursor = false;

	static constexpr int MagnifierPixelCount = 15;
	static constexpr int MagnifierPixelSize = 12;

	static std::function<void(QColor)> callback;
	static std::vector<ScreenColorPicker*> activePickers;
};
