#include "TimeLogDialog.h"

#include "Gitlab/TimeLog.h"

#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

TimeLogDialog::TimeLogDialog(const std::vector<TimeLog>& timeLogs, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Delete logged time");
	auto layout = new QVBoxLayout(this);
	timeLogList = new QListWidget(this);
	for (auto& timeLog : timeLogs) {
		auto item = new QListWidgetItem(timeLog.getDisplayText(), timeLogList);
		item->setData(Qt::UserRole, timeLog.Id);
	}

	auto deleteButton = new QPushButton("Delete selected", this);
	auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(timeLogList);
	layout->addWidget(deleteButton);
	layout->addWidget(buttonBox);
	connect(deleteButton, &QPushButton::clicked, this, &TimeLogDialog::DeleteSelected);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

std::vector<QString> TimeLogDialog::getDeletedTimeLogIds() const
{
	return deletedTimeLogIds;
}

void TimeLogDialog::DeleteSelected()
{
	auto selectedItems = timeLogList->selectedItems();
	for (auto item : selectedItems) {
		deletedTimeLogIds.push_back(item->data(Qt::UserRole).toString());
		delete item;
	}
}
