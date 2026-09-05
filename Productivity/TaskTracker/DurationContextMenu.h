#pragma once

#include <QMenu>

enum class DurationDisplayMode
{
	Default,
	Hours
};

class DurationContextMenu : public QMenu
{
	Q_OBJECT

public:
	explicit DurationContextMenu(DurationDisplayMode currentMode, QWidget* parent = nullptr);

signals:
	void DisplayModeSelected(DurationDisplayMode mode);
};
