#include "DateCountdown.h"

#include <QDateTime>
#include <QSignalBlocker>
#include <QStringList>

DateCountdown::DateCountdown(QWidget* parent)
	: Component(parent, ToolType::DateCountdown)
{
	ui.setupUi(this);

	auto now = QDateTime::currentDateTime();
	ui.FromDateTime->setDateTime(now);
	ui.ToDateTime->setDateTime(now);

	connect(ui.FromDateTime, &QDateTimeEdit::dateTimeChanged, this, &DateCountdown::Update);
	connect(ui.ToDateTime, &QDateTimeEdit::dateTimeChanged, this, &DateCountdown::Update);
	connect(ui.FromLiveButton, &QToolButton::toggled, this, &DateCountdown::LiveToggled);
	connect(ui.ToLiveButton, &QToolButton::toggled, this, &DateCountdown::LiveToggled);
	connect(&updateTimer, &QTimer::timeout, this, &DateCountdown::UpdateCurrentTimes);
	updateTimer.setInterval(std::chrono::seconds{1});
	Update();
}

QJsonObject DateCountdown::SaveState()
{
	auto state = Component::SaveState();
	state["FromDateTime"] = ui.FromDateTime->dateTime().toMSecsSinceEpoch();
	state["ToDateTime"] = ui.ToDateTime->dateTime().toMSecsSinceEpoch();
	state["FromLive"] = ui.FromLiveButton->isChecked();
	state["ToLive"] = ui.ToLiveButton->isChecked();
	return state;
}

void DateCountdown::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	ui.FromDateTime->setDateTime(QDateTime::fromMSecsSinceEpoch(state["FromDateTime"].toInteger()));
	ui.ToDateTime->setDateTime(QDateTime::fromMSecsSinceEpoch(state["ToDateTime"].toInteger()));
	ui.FromLiveButton->setChecked(state["FromLive"].toBool());
	ui.ToLiveButton->setChecked(state["ToLive"].toBool());
	Update();
}

void DateCountdown::Update()
{
	UpdateResult();
	emit Modified(this);
}

void DateCountdown::UpdateResult()
{
	auto from = ui.FromDateTime->dateTime();
	auto to = ui.ToDateTime->dateTime();
	auto seconds = from.secsTo(to);
	auto cursor = seconds >= 0 ? from : to;
	auto end = seconds >= 0 ? to : from;
	int years = end.date().year() - cursor.date().year();

	if (cursor.addYears(years) > end) {
		--years;
	}

	cursor = cursor.addYears(years);
	int months = (end.date().year() - cursor.date().year()) * 12
		+ end.date().month() - cursor.date().month();

	if (cursor.addMonths(months) > end) {
		--months;
	}

	cursor = cursor.addMonths(months);
	int days = cursor.date().daysTo(end.date());

	if (cursor.addDays(days) > end) {
		--days;
	}

	cursor = cursor.addDays(days);
	auto remaining = cursor.secsTo(end);
	auto hours = remaining / 3'600;
	remaining %= 3'600;
	auto minutes = remaining / 60;
	remaining %= 60;

	QStringList units;

	if (years) {
		units.append(QString("%1 year%2").arg(years).arg(years == 1 ? "" : "s"));
	}

	if (months) {
		units.append(QString("%1 month%2").arg(months).arg(months == 1 ? "" : "s"));
	}

	if (days) {
		units.append(QString("%1 day%2").arg(days).arg(days == 1 ? "" : "s"));
	}

	if (hours) {
		units.append(QString("%1 hour%2").arg(hours).arg(hours == 1 ? "" : "s"));
	}

	if (minutes) {
		units.append(QString("%1 minute%2").arg(minutes).arg(minutes == 1 ? "" : "s"));
	}

	if (remaining || units.isEmpty()) {
		units.append(QString("%1 second%2").arg(remaining).arg(remaining == 1 ? "" : "s"));
	}

	QString result = units.join(", ");

	if (seconds > 0) {
		result += " passed";
	} else if (seconds < 0) {
		result += " remaining";
	}

	ui.ResultLabel->setText(result);
}

void DateCountdown::UpdateCurrentTimes()
{
	auto now = QDateTime::currentDateTime();

	if (ui.FromLiveButton->isChecked()) {
		QSignalBlocker blocker(ui.FromDateTime);
		ui.FromDateTime->setDateTime(now);
	}

	if (ui.ToLiveButton->isChecked()) {
		QSignalBlocker blocker(ui.ToDateTime);
		ui.ToDateTime->setDateTime(now);
	}

	UpdateResult();
}

void DateCountdown::LiveToggled()
{
	ui.FromDateTime->setEnabled(!ui.FromLiveButton->isChecked());
	ui.ToDateTime->setEnabled(!ui.ToLiveButton->isChecked());

	if (ui.FromLiveButton->isChecked() || ui.ToLiveButton->isChecked()) {
		updateTimer.start();
		UpdateCurrentTimes();
	} else {
		updateTimer.stop();
	}

	emit Modified(this);
}
