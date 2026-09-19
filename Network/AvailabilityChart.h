#pragma once

#include <QQueue>
#include <QWidget>

class AvailabilityChart : public QWidget
{
public:
	explicit AvailabilityChart(QWidget* parent = nullptr);

	void SetSamples(const QQueue<QPair<qint64, double>>& samples);
	void SetWindowSeconds(int seconds);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QQueue<QPair<qint64, double>> samples;
	int windowSeconds{60};
};
