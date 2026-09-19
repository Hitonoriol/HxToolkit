#include "AvailabilityChart.h"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

AvailabilityChart::AvailabilityChart(QWidget* parent)
	: QWidget(parent)
{
	setMinimumHeight(160);
}

void AvailabilityChart::SetSamples(const QQueue<QPair<qint64, double>>& samples)
{
	this->samples = samples;
	update();
}

void AvailabilityChart::SetWindowSeconds(int seconds)
{
	if (windowSeconds != seconds) {
		windowSeconds = seconds;
		update();
	}
}

void AvailabilityChart::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const auto textColor = palette().color(QPalette::Text);
	const auto gridColor = palette().color(QPalette::Mid);
	const auto lineColor = palette().color(QPalette::Highlight);
	const auto plot = rect().adjusted(42, 10, -12, -28);
	if (plot.width() <= 0 || plot.height() <= 0) {
		return;
	}

	auto drawLabel = [&](int y, const QString& text) {
		painter.setPen(textColor);
		painter.drawText(QRect(0, y - 9, 36, 18), Qt::AlignRight | Qt::AlignVCenter, text);
	};
	for (const auto [value, label] : { std::pair{1.0, "100%"}, {0.5, "50%"}, {0.0, "0%"} }) {
		const auto y = plot.bottom() - static_cast<int>(value * plot.height());
		painter.setPen(QPen(gridColor, 1));
		painter.drawLine(plot.left(), y, plot.right(), y);
		drawLabel(y, label);
	}

	const auto now = QDateTime::currentMSecsSinceEpoch();
	const auto start = now - static_cast<qint64>(windowSeconds) * 1'000;
	auto xFor = [&](qint64 time) {
		return plot.left() + static_cast<int>(std::clamp((time - start) / static_cast<double>(now - start), 0.0, 1.0) * plot.width());
	};
	auto yFor = [&](double availability) {
		return plot.bottom() - static_cast<int>(std::clamp(availability, 0.0, 1.0) * plot.height());
	};

	double availability = 1.0;
	for (const auto& sample : samples) {
		if (sample.first <= start) {
			availability = sample.second;
		} else {
			break;
		}
	}

	QPainterPath path;
	path.moveTo(plot.left(), yFor(availability));
	for (const auto& sample : samples) {
		if (sample.first < start || sample.first > now) {
			continue;
		}
		const auto x = xFor(sample.first);
		path.lineTo(x, yFor(availability));
		availability = sample.second;
		path.lineTo(x, yFor(availability));
	}
	path.lineTo(plot.right(), yFor(availability));
	painter.setPen(QPen(lineColor, 2));
	painter.drawPath(path);

	painter.setPen(textColor);
	painter.drawText(QRect(plot.left(), plot.bottom() + 5, 70, 18), Qt::AlignLeft | Qt::AlignVCenter,
		QString("-%1 min").arg(windowSeconds / 60));
	painter.drawText(QRect(plot.right() - 42, plot.bottom() + 5, 42, 18), Qt::AlignRight | Qt::AlignVCenter, "Now");
}
