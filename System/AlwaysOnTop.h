#pragma once

#include "UI/Component.h"

class QTableWidget;
class QTableWidgetItem;

class AlwaysOnTop : public Component
{
	Q_OBJECT

public:
	explicit AlwaysOnTop(QWidget* parent = nullptr);

private slots:
	void RefreshWindows();
	void WindowItemChanged(QTableWidgetItem* item);

private:
	QTableWidget* windowTable;
};
