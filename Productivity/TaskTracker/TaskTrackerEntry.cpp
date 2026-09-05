#include "TaskTrackerEntry.h"

#include "TimeContextMenu.h"
#include "Util/Time.h"

#include <QSignalBlocker>
#include <QTimer>

TaskTrackerEntry::TaskTrackerEntry(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	ui.StartField->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.StartField, &QWidget::customContextMenuRequested, ui.StartField, [this](const QPoint& position) {
		TimeContextMenu menu(ui.StartField);
		menu.exec(ui.StartField->mapToGlobal(position));
	});

	ui.EndField->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.EndField, &QWidget::customContextMenuRequested, ui.EndField, [this](const QPoint& position) {
		TimeContextMenu menu(ui.EndField);
		menu.exec(ui.EndField->mapToGlobal(position));
	});

	ui.DurationField->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.DurationField, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
		DurationContextMenu menu(durationDisplayMode, ui.DurationField);
		connect(&menu, &DurationContextMenu::DisplayModeSelected, this, [this](DurationDisplayMode mode) {
			durationDisplayMode = mode;

			if (finished) {
				UpdateTime();
			}
			else {
				UpdateLiveTime();
			}
		});
		menu.exec(ui.DurationField->mapToGlobal(position));
	});

	auto liveTimeTimer = new QTimer(this);
	connect(liveTimeTimer, &QTimer::timeout, this, &TaskTrackerEntry::UpdateLiveTime);
	liveTimeTimer->start(1000);
	SetFinished(false);
}

TaskTrackerEntry::~TaskTrackerEntry()
{}

void TaskTrackerEntry::SetNumber(uint64_t number)
{
	ui.IndexLabel->setText(QString::number(number) + ".");
}

uint64_t TaskTrackerEntry::GetNumber()
{
	return ui.IndexLabel->text().removeLast().toULongLong();
}

void TaskTrackerEntry::SetStartTime(const QString& time)
{
	ui.StartField->setText(time);
	UpdateTime();
}

QString TaskTrackerEntry::GetStartTime() const
{
	return ui.StartField->text();
}

void TaskTrackerEntry::SetEndTime(const QString& time)
{
	ui.EndField->setText(time);
	UpdateTime();
}

QString TaskTrackerEntry::GetEndTime() const
{
	return ui.EndField->text();
}

QString TaskTrackerEntry::GetDescription() const
{
	return ui.DescriptionField->text();
}

void TaskTrackerEntry::SetDescription(const QString& description)
{
	ui.DescriptionField->setText(description);
}

int64_t TaskTrackerEntry::GetDuration()
{
	auto startTimeStr = ui.StartField->text();
	auto endTimeStr = ui.EndField->text();

	if (startTimeStr.isEmpty() || endTimeStr.isEmpty()) {
		return 0;
	}

	auto start = QDateTime::fromString(startTimeStr, "hh:mm").toMSecsSinceEpoch();
	auto end = QDateTime::fromString(endTimeStr, "hh:mm").toMSecsSinceEpoch();
	return end - start;
}

bool TaskTrackerEntry::IsFinished()
{
	return finished;
}

void TaskTrackerEntry::SetFinished(bool value)
{
	finished = value;
	ui.EndField->setStyleSheet(finished ? "" : "color: palette(mid);");
	ui.DurationField->setStyleSheet(finished ? "" : "color: palette(mid);");

	if (!finished) {
		UpdateLiveTime();
	}
}

QLineEdit* TaskTrackerEntry::GetEndField()
{
	return ui.EndField;
}

QPushButton* TaskTrackerEntry::GetEndButton()
{
	return ui.EndButton;
}

void TaskTrackerEntry::OnEndFieldModified(QString newTime)
{
	UpdateTime();
	emit EndFieldModified(newTime);
}

void TaskTrackerEntry::OnStartFieldModified(QString newTime)
{
	if (finished) {
		UpdateTime();
	}
	else {
		UpdateLiveTime();
	}
	emit StartFieldModified(newTime);
}

void TaskTrackerEntry::OnDescriptionFieldModified(QString newDescription)
{
	emit DescriptionFieldModified(newDescription);
}

void TaskTrackerEntry::UpdateTime(int64_t begin, int64_t end, bool updateEndField)
{
	auto diff = end - begin;
	float hours = ((diff / 1000) % 86400) / 3600.0f;
	if (diff <= 0) {
		return;
	}

	if (updateEndField) {
		ui.EndField->setText(QTime::fromMSecsSinceStartOfDay(end).toString("hh:mm"));
	}

	ui.DurationField->setText(durationDisplayMode == DurationDisplayMode::Hours
		? QString::number(hours, 'f', 2)
		: Time::GetTimeString(diff));
}

void TaskTrackerEntry::UpdateTime()
{
	try {
		auto startTimeStr = ui.StartField->text();
		auto endTimeStr = ui.EndField->text();
		if (startTimeStr.isEmpty() || endTimeStr.isEmpty()) {
			return;
		}

		auto start = QTime::fromString(startTimeStr, "hh:mm");
		auto end = QTime::fromString(endTimeStr, "hh:mm");

		UpdateTime(start.msecsSinceStartOfDay(), end.msecsSinceStartOfDay(), false);
	}
	catch (...) {
		ui.DurationField->setText("");
	}
}

void TaskTrackerEntry::OnEndButtonPress()
{
	auto startTime = QTime::fromString(ui.StartField->text(), "hh:mm");
	auto endTime = QTime::currentTime();
	endTime.setHMS(endTime.hour(), endTime.minute(), 0); // Ignore seconds

	SetFinished(true);
	UpdateTime(startTime.msecsSinceStartOfDay(), endTime.msecsSinceStartOfDay());
	ui.EndButton->setVisible(false);
	ui.EndField->setReadOnly(false);
	emit EndButtonPressed();
}

void TaskTrackerEntry::UpdateLiveTime()
{
	if (finished) {
		return;
	}

	auto now = QTime::currentTime();
	auto endFieldChangeBlocker = QSignalBlocker(ui.EndField);
	ui.EndField->setText(now.toString("hh:mm"));

	auto start = QTime::fromString(ui.StartField->text(), "hh:mm");
	if (!start.isValid()) {
		start = QTime::fromString(ui.StartField->text(), "h:mm");
	}

	if (start.isValid()) {
		UpdateTime(start.msecsSinceStartOfDay(), now.msecsSinceStartOfDay(), false);
	}
	else {
		ui.DurationField->clear();
	}
}
