#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>

std::vector<ScreenCapture> ScreenCaptures::CaptureAll()
{
	std::vector<ScreenCapture> captures;
	for (auto* screen : QGuiApplication::screens()) {
		captures.push_back({screen, screen->geometry(), screen->grabWindow(0)});
	}
	return captures;
}
