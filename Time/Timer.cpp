#include "Timer.h"

#include "Util/Time.h"
#include "JSON/Serialization.h"

Timer::Timer(QWidget *parent)
	: Component(parent, ToolType::Timer)
{
	ui.setupUi(this);
	connect(&updateTimer, &QTimer::timeout, this, &Timer::OnUpdateTimerTick);
}

Timer::~Timer()
{}

QJsonObject Timer::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.HourBox, value);
	    HX_SERIALIZE(ui.MinuteBox, value);
		HX_SERIALIZE(ui.SecondBox, value);
	);

	state["IsPaused"] = !updateTimer.isActive();
	state["StartTime"] = startTime;
	state["SavedTime"] = savedTime;
	state["Duration"] = duration.GetTimeMs();

	return state;
}

void Timer::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	updateTimer.stop();
	ui.TimeEdit->setEnabled(true);
	ui.StartPauseButton->setText("Start");

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.HourBox, setValue, toInt);
	    HX_DESERIALIZE(ui.MinuteBox, setValue, toInt);
		HX_DESERIALIZE(ui.SecondBox, setValue, toInt);
	);

	bool paused = state["IsPaused"].toBool();
	startTime = state["StartTime"].toInteger();
	savedTime = state["SavedTime"].toInteger();
	duration = Time(state["Duration"].toInteger());

	if (startTime || savedTime) {
		if (!paused) {
			updateTimer.start(std::chrono::milliseconds{100});
			ui.TimeEdit->setEnabled(false);
			ui.StartPauseButton->setText("Pause");
		}
		else {
			Update(true);
			ui.StartPauseButton->setText("Resume");
		}
	}
}

void Timer::OnStartButtonPress()
{
	if (!updateTimer.isActive()) {
		auto hours = ui.HourBox->value();
		auto minutes = ui.MinuteBox->value();
		auto seconds = ui.SecondBox->value();
		if (hours == 0 && minutes == 0 && seconds == 0) {
			return;
		}

		duration.SetTime(hours, minutes, seconds);
		startTime = Time::Now();
		updateTimer.start(std::chrono::milliseconds{100});
		ui.TimeEdit->setEnabled(false);
		ui.StartPauseButton->setText("Pause");
	} else {
		savedTime += qMax(0, Time::Now() - startTime);
		updateTimer.stop();
		ui.StartPauseButton->setText("Resume");
	}

	emit Modified(this);
}

void Timer::OnStopButtonPress()
{
	if (updateTimer.isActive()) {
		updateTimer.stop();
	}

	savedTime = 0;
	ui.TimeEdit->setEnabled(true);
	ui.StartPauseButton->setText("Start");
	ui.TimeLabel->setText(Time::GetTimeString(0));
	ui.TimeProgress->setValue(0);

	emit Modified(this);
}

void Timer::OnUpdateTimerTick()
{
	Update();
}

void Timer::Update(bool savedTimeOnly)
{
	auto elapsed = savedTimeOnly ? 0 : Time::Now() - startTime;
	auto durationMs = duration.GetTimeMs();
	auto remaining = qMax(0, durationMs - (elapsed + savedTime));
	auto progress = (int)(100.0f - (((float)remaining / (float)durationMs) * 100.0f));

	ui.TimeLabel->setText(Time::GetTimeString(remaining));
	ui.TimeProgress->setValue(progress);

	if (remaining == 0) {
		updateTimer.stop();
		savedTime = 0;
		ui.TimeEdit->setEnabled(true);
		ui.StartPauseButton->setText("Start");
		emit TimerCompleted(startTime, duration);
	}
}
