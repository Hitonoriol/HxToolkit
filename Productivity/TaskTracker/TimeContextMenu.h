#pragma once

#include <QMenu>

class QLineEdit;

class TimeContextMenu : public QMenu
{
public:
	explicit TimeContextMenu(QLineEdit* timeField);
};
