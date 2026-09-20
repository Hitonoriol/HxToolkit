#include "MeltingScreenSaver.h"

#include "General/ScreenCapture.h"

#include <QKeyEvent>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

std::vector<MeltingScreenSaver*> MeltingScreenSaver::activeSavers;

void MeltingScreenSaver::Start()
{
	if (!activeSavers.empty()) {
		return;
	}

	for (auto&& capture : ScreenCaptures::CaptureAll()) {
		auto* saver = new MeltingScreenSaver(capture.geometry, std::move(capture.image));
		activeSavers.push_back(saver);
		saver->showFullScreen();
		saver->raise();
	}

	if (!activeSavers.empty()) {
		auto* activeSaver = activeSavers.back();
		activeSaver->activateWindow();
		activeSaver->grabKeyboard();
	}
}

MeltingScreenSaver::MeltingScreenSaver(const QRect& geometry, QPixmap capturedScreenshot)
	: QWidget(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
	, screenshot(std::move(capturedScreenshot))
{
	setGeometry(geometry);
	setFocusPolicy(Qt::StrongFocus);
	const auto stripCount = std::max(1, width() / 3);
	drips.resize(stripCount);
	for (auto& drip : drips) {
		drip.acceleration = QRandomGenerator::global()->bounded(8, 38) / 100.0;
		drip.phase = QRandomGenerator::global()->generateDouble() * 6.283185307;
	}
	connect(&meltTimer, &QTimer::timeout, this, [this] { AdvanceMelt(); });
	meltTimer.start(33);
}

void MeltingScreenSaver::keyPressEvent(QKeyEvent*)
{
	Finish();
}

void MeltingScreenSaver::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), Qt::black);
	if (screenshot.isNull()) {
		return;
	}

	constexpr int bandHeight = 32;
	const int stripWidth = std::max(1, width() / static_cast<int>(drips.size()));
	for (int index = 0; index < static_cast<int>(drips.size()); ++index) {
		const auto x = index * stripWidth;
		const auto sourceWidth = std::min(stripWidth, width() - x);
		const auto& drip = drips[index];
		for (int sourceTop = 0; sourceTop < height(); sourceTop += bandHeight) {
			const int sourceBottom = std::min(sourceTop + bandHeight, height());
			const auto positionFactor = [](int y, int fullHeight) {
				return std::pow(static_cast<qreal>(y) / fullHeight, 1.65);
			};
			const auto stretch = [drip](int y) {
				return 1.0 + 0.18 * std::sin(y * 0.035 + drip.phase);
			};
			const auto targetTop = sourceTop + drip.flow * positionFactor(sourceTop, height()) * stretch(sourceTop);
			const auto targetBottom = sourceBottom + drip.flow * positionFactor(sourceBottom, height()) * stretch(sourceBottom);
			const QRect sourceRect(x, sourceTop, sourceWidth, sourceBottom - sourceTop);
			const QRect targetRect(x, static_cast<int>(targetTop), sourceWidth,
				std::max(1, static_cast<int>(std::ceil(targetBottom - targetTop))));
			if (!sourceRect.isEmpty() && !targetRect.isEmpty()) {
				painter.drawPixmap(targetRect, screenshot, sourceRect);
			}
		}
	}
}

void MeltingScreenSaver::AdvanceMelt()
{
	for (auto& drip : drips) {
		drip.acceleration = std::clamp(drip.acceleration + QRandomGenerator::global()->bounded(-3, 4) / 100.0,
			0.04, 0.55);
		drip.flow = std::min<qreal>(height() * 1.5, drip.flow + drip.acceleration);
		drip.phase += 0.018;
	}
	for (int index = 1; index + 1 < static_cast<int>(drips.size()); ++index) {
		if (QRandomGenerator::global()->bounded(100) < 3) {
			drips[index].flow = (drips[index - 1].flow + drips[index].flow + drips[index + 1].flow) / 3.0;
		}
	}
	update();
}

void MeltingScreenSaver::Finish()
{
	for (auto* saver : activeSavers) {
		saver->meltTimer.stop();
		saver->releaseKeyboard();
		saver->hide();
		saver->deleteLater();
	}
	activeSavers.clear();
}
