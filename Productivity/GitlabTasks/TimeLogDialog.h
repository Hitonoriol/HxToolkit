#pragma once

#include <QDialog>

#include <vector>

class QListWidget;
struct TimeLog;

class TimeLogDialog : public QDialog
{
	Q_OBJECT

public:
	TimeLogDialog(const std::vector<TimeLog>& timeLogs, QWidget* parent = nullptr);

	std::vector<QString> getDeletedTimeLogIds() const;

private slots:
	void DeleteSelected();

private:
	QListWidget* timeLogList;
	std::vector<QString> deletedTimeLogIds;
};
