#include "DurationContextMenu.h"

#include <QActionGroup>

DurationContextMenu::DurationContextMenu(DurationDisplayMode currentMode, QWidget* parent)
	: QMenu(parent)
{
	auto displayModeGroup = new QActionGroup(this);
	displayModeGroup->setExclusive(true);

	auto defaultAction = addAction("Default");
	defaultAction->setCheckable(true);
	defaultAction->setChecked(currentMode == DurationDisplayMode::Default);
	displayModeGroup->addAction(defaultAction);
	connect(defaultAction, &QAction::triggered, this, [this] {
		emit DisplayModeSelected(DurationDisplayMode::Default);
	});

	auto hoursAction = addAction("Hours");
	hoursAction->setCheckable(true);
	hoursAction->setChecked(currentMode == DurationDisplayMode::Hours);
	displayModeGroup->addAction(hoursAction);
	connect(hoursAction, &QAction::triggered, this, [this] {
		emit DisplayModeSelected(DurationDisplayMode::Hours);
	});
}
