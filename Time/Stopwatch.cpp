#include "Stopwatch.h"

#include "Util/Time.h"
#include "JSON/Serialization.h"

#include <QDateTime>
#include <QTimeZone>

Stopwatch::Stopwatch(QWidget *parent)
	: Component(parent, ToolType::Stopwatch)
{
	ui.setupUi(this);
	connect(&updateTimer, &QTimer::timeout, this, &Stopwatch::UpdateTimerTick);
}

Stopwatch::~Stopwatch()
{}

QJsonObject Stopwatch::SaveState()
{
	auto state = Component::SaveState();
	bool hasState = startTime || savedTime;

	state["HasState"] = hasState;
	
	if (!hasState) {
		return state;
	}

	state["IsPaused"] = !updateTimer.isActive();
	state["StartTime"] = startTime;
	state["SavedTime"] = savedTime;

	QJsonArray timePoints;

	for (auto rowIdx = 0; rowIdx < ui.TimeTable->rowCount(); ++rowIdx) {
		auto timeItem = ui.TimeTable->item(rowIdx, 0);
		auto diffItem = ui.TimeTable->item(rowIdx, 1);

		QJsonObject rowJson;
		rowJson["Time"] = timeItem->text();
		rowJson["Diff"] = diffItem->text();
		timePoints.append(rowJson);
	}

	state["TimePoints"] = timePoints;

	return state;
}

void Stopwatch::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	updateTimer.stop();
	startTime = 0;
	savedTime = 0;
	lastPoint = 0;
	ui.TimeLabel->setText(Time::GetTimeString(0));
	ui.TimeTable->setRowCount(0);
	ui.StartPauseButton->setText("Start");
	ui.TimePointButton->setEnabled(false);

	auto hasState = state["HasState"].toBool();

	if (!hasState) {
		return;
	}

	auto isPaused = state["IsPaused"].toBool();
	startTime = state["StartTime"].toInteger();
	savedTime = state["SavedTime"].toInteger();

	if (isPaused) {
		Pause(false);
		ui.TimeLabel->setText(Time::GetTimeString(savedTime));
	} else {
		Start(false);
	}

	auto timePoints = state["TimePoints"].toArray();

	for (auto rowJsonRef : timePoints) {
		auto rowJson = rowJsonRef.toObject();
		auto time = rowJson["Time"].toString();
		auto diff = rowJson["Diff"].toString();

		AddTimePoint(time, diff);
	}
}

void Stopwatch::OnStartPausePress()
{
	if (!updateTimer.isActive()) {
		Start(true);
	} else {
		Pause(true);
	}

	emit Modified(this);
}

void Stopwatch::Start(bool updateState)
{
	if (updateState) {
		startTime = Time::Now();
	}

	updateTimer.start(std::chrono::milliseconds{100});
	ui.StartPauseButton->setText("Pause");
	ui.TimePointButton->setEnabled(true);
}

void Stopwatch::Pause(bool updateState)
{
	if (updateState) {
		savedTime += Elapsed();
	}

	updateTimer.stop();
	ui.StartPauseButton->setText("Resume");
	ui.TimePointButton->setEnabled(false);
}

void Stopwatch::OnStopPress()
{
	if (updateTimer.isActive()) {
		OnStartPausePress();
	}
	ui.StartPauseButton->setText("Start");

	savedTime = 0;
	ui.TimeLabel->setText(Time::GetTimeString(0));
	ui.TimeTable->setRowCount(0);
	ui.TimePointButton->setEnabled(false);

	emit Modified(this);
}

void Stopwatch::OnTimePointPress()
{
	auto now = savedTime + Elapsed();
	auto diff = now - lastPoint;
	lastPoint = now;

	AddTimePoint(now, diff);

	emit Modified(this);
}

void Stopwatch::AddTimePoint(const QString& time, const QString& diff)
{
	auto newIdx = ui.TimeTable->rowCount();
	ui.TimeTable->insertRow(newIdx);
	ui.TimeTable->setItem(newIdx, 0, new QTableWidgetItem(time));
	ui.TimeTable->setItem(newIdx, 1, new QTableWidgetItem(diff));
}

void Stopwatch::AddTimePoint(int64_t time, int64_t diff)
{
	auto timeStr = Time::GetTimeString(time);
	auto diffStr = "+" + Time::GetTimeString(diff);

	AddTimePoint(timeStr, diffStr);
}

void Stopwatch::UpdateTimerTick()
{
	auto timeStr = Time::GetTimeString(savedTime + Elapsed());
	ui.TimeLabel->setText(timeStr);
}

int64_t Stopwatch::Elapsed()
{
	return Time::Now() - startTime;
}
