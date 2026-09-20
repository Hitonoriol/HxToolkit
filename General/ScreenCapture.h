#pragma once

#include <QPixmap>
#include <QRect>

#include <vector>

class QScreen;

struct ScreenCapture
{
	QScreen* screen{};
	QRect geometry;
	QPixmap image;
};

class ScreenCaptures
{
public:
	// Captures every display before any fullscreen overlay is shown.
	static std::vector<ScreenCapture> CaptureAll();
};
