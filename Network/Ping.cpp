#include "Ping.h"

#include <QDateTime>
#include <QSignalBlocker>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cmath>

Ping::Ping(QWidget* parent)
	: Component(parent, ToolType::Ping)
{
	ui.setupUi(this);
	ui.HostField->setText("1.1.1.1");

	connect(&pingTimer, &QTimer::timeout, this, &Ping::PingHost);
	connect(&pinger, &ICMPPinger::PingFinished, this, &Ping::OnPingFinished);
	connect(ui.HostField, &QLineEdit::editingFinished, this, &Ping::OnSettingsChanged);
	connect(ui.IntervalBox, qOverload<int>(&QSpinBox::valueChanged), this, &Ping::OnSettingsChanged);
	connect(ui.AvailabilityWindowBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &Ping::OnAvailabilityWindowChanged);
	ui.AvailabilityChartWidget->SetWindowSeconds(AvailabilityWindowSeconds());
	UpdateSchedule();
	PingHost();
}

QJsonObject Ping::SaveState()
{
	auto state = Component::SaveState();
	state["Host"] = ui.HostField->text();
	state["IntervalSeconds"] = ui.IntervalBox->value();
	state["AvailabilityWindowIndex"] = ui.AvailabilityWindowBox->currentIndex();
	return state;
}

void Ping::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	const QSignalBlocker hostBlocker(ui.HostField);
	const QSignalBlocker intervalBlocker(ui.IntervalBox);
	const QSignalBlocker availabilityBlocker(ui.AvailabilityWindowBox);
	ui.HostField->setText(state["Host"].toString("1.1.1.1"));
	ui.IntervalBox->setValue(state["IntervalSeconds"].toInt(1));
	ui.AvailabilityWindowBox->setCurrentIndex(state["AvailabilityWindowIndex"].toInt(0));
	ui.AvailabilityChartWidget->SetWindowSeconds(AvailabilityWindowSeconds());
	UpdateSchedule();
	PingHost();
}

void Ping::PingHost()
{
	const auto host = Host();
	if (host.isEmpty()) {
		ui.StatusLabel->setText("Status: Enter a host");
		return;
	}

	pinger.Ping(host);
}

void Ping::OnSettingsChanged()
{
	pingResults.clear();
	availabilityHistory.clear();
	RefreshAvailabilityChart();
	UpdateSchedule();
	PingHost();
	emit Modified(this);
}

void Ping::OnPingFinished(const ICMPPingResult& result)
{
	lastPingOnline = result.online;
	lastRoundTripTimeMs = result.roundTripTimeMs;
	const auto now = QDateTime::currentMSecsSinceEpoch();
	pingResults.enqueue({now, result.online});
	AddAvailabilitySample(now);
	UpdatePacketLoss();
	RefreshAvailabilityChart();
}

void Ping::OnAvailabilityWindowChanged(int)
{
	ui.AvailabilityChartWidget->SetWindowSeconds(AvailabilityWindowSeconds());
	RefreshAvailabilityChart();
	emit Modified(this);
}

QString Ping::Host() const
{
	const auto input = ui.HostField->text().trimmed();
	if (input.isEmpty()) {
		return {};
	}

	auto url = QUrl::fromUserInput(input);
	return url.host().isEmpty() ? input : url.host();
}

void Ping::UpdateSchedule()
{
	pingTimer.start(std::chrono::seconds{ui.IntervalBox->value()});
}

void Ping::UpdatePacketLoss()
{
	const auto cutoff = QDateTime::currentMSecsSinceEpoch() - 10'000;
	int total = 0;
	int lost = 0;
	for (const auto& result : pingResults) {
		if (result.first >= cutoff) {
			++total;
			lost += !result.second;
		}
	}

	if (total == 0) {
		ui.StatusLabel->setText("Status: Waiting…");
		ui.PacketLossLabel->setText("Packet loss (last 10 seconds): —");
		return;
	}

	const auto lossPercent = static_cast<int>(std::round(100.0 * lost / total));
	if (lost == total) {
		ui.StatusLabel->setText("Status: Offline");
	} else if (lastPingOnline) {
		ui.StatusLabel->setText(QString("Status: Online (%1 ms)").arg(lastRoundTripTimeMs));
	} else {
		ui.StatusLabel->setText("Status: Online");
	}
	ui.PacketLossLabel->setText(QString("Packet loss (last 10 seconds): %1%").arg(lossPercent));
}

void Ping::AddAvailabilitySample(qint64 timestamp)
{
	const auto historyCutoff = timestamp - 60 * 60 * 1'000;
	while (!pingResults.isEmpty() && pingResults.head().first < historyCutoff) {
		pingResults.dequeue();
	}
	while (!availabilityHistory.isEmpty() && availabilityHistory.head().first < historyCutoff) {
		availabilityHistory.dequeue();
	}

	const auto windowCutoff = timestamp - 10'000;
	int total = 0;
	int lost = 0;
	for (const auto& result : pingResults) {
		if (result.first >= windowCutoff) {
			++total;
			lost += !result.second;
		}
	}
	availabilityHistory.enqueue({timestamp, total == 0 ? 1.0 : 1.0 - static_cast<double>(lost) / total});
}

void Ping::RefreshAvailabilityChart()
{
	ui.AvailabilityChartWidget->SetSamples(availabilityHistory);
}

int Ping::AvailabilityWindowSeconds() const
{
	constexpr std::array windows{60, 5 * 60, 10 * 60, 30 * 60, 60 * 60};
	return windows.at(ui.AvailabilityWindowBox->currentIndex());
}
