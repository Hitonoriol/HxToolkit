#pragma once

#include <QTreeWidgetItem>

class DurationTreeWidgetItem : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;

	bool operator<(const QTreeWidgetItem& other) const override;
};
