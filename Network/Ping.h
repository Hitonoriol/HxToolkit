#pragma once

#include "Network/ICMPPinger.h"
#include "UI/Component.h"
#include "ui_Ping.h"

#include <QQueue>
#include <QTimer>

class Ping : public Component
{
	Q_OBJECT

public:
	explicit Ping(QWidget* parent = nullptr);

	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private slots:
	void PingHost();
	void OnSettingsChanged();
	void OnPingFinished(const ICMPPingResult& result);
	void OnAvailabilityWindowChanged(int index);

private:
	QString Host() const;
	void UpdateSchedule();
	void UpdatePacketLoss();
	void AddAvailabilitySample(qint64 timestamp);
	void RefreshAvailabilityChart();
	int AvailabilityWindowSeconds() const;

	Ui::PingClass ui;
	ICMPPinger pinger;
	QTimer pingTimer;
	QQueue<QPair<qint64, bool>> pingResults;
	QQueue<QPair<qint64, double>> availabilityHistory;
	bool lastPingOnline{};
	int lastRoundTripTimeMs{};
};
