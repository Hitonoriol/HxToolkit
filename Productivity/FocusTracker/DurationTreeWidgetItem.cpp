#include "DurationTreeWidgetItem.h"

#include <QTreeWidget>

bool DurationTreeWidgetItem::operator<(const QTreeWidgetItem& other) const
{
	auto column = treeWidget()->sortColumn();
	if (column == 1) {
		return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
	}

	return QTreeWidgetItem::operator<(other);
}
