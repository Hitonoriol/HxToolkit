#include "TimeContextMenu.h"

#include <QLineEdit>
#include <QTime>

TimeContextMenu::TimeContextMenu(QLineEdit* timeField)
	: QMenu(timeField)
{
	auto currentTimeAction = addAction("Current Time");
	connect(currentTimeAction, &QAction::triggered, timeField, [timeField] {
		timeField->setText(QTime::currentTime().toString("hh:mm"));
	});
}
