#pragma once

#include <QWidget>
#include "ui_TaskTrackerEntry.h"

#include "DurationContextMenu.h"

#include <QPushButton>
#include <QLineEdit>

class TaskTrackerEntry : public QWidget
{
	Q_OBJECT

public:
	TaskTrackerEntry(QWidget *parent = nullptr);
	~TaskTrackerEntry();

	void SetNumber(uint64_t number);
	uint64_t GetNumber();

	void SetStartTime(const QString& time);
	QString GetStartTime() const;

	void SetEndTime(const QString& time);
	QString GetEndTime() const;

	QString GetDescription() const;
	void SetDescription(const QString& description);

	int64_t GetDuration();

	bool IsFinished();
	void SetFinished(bool value);

	QLineEdit* GetEndField();
	QPushButton* GetEndButton();

signals:
	void EndButtonPressed();
	void EndFieldModified(QString newTime);
	void StartFieldModified(QString newTime);
	void DescriptionFieldModified(QString newDescription);

private slots:
	void OnEndButtonPress();
	void OnEndFieldModified(QString newTime);
	void OnStartFieldModified(QString newTime);
	void OnDescriptionFieldModified(QString newDescription);

private:
	void UpdateTime(int64_t begin, int64_t end, bool updateEndField = true);
	void UpdateTime();
	void UpdateLiveTime();

	Ui::TaskTrackerEntryClass ui;
	DurationDisplayMode durationDisplayMode = DurationDisplayMode::Default;
	bool finished = false;
};
