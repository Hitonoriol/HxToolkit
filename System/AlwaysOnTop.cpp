#include "AlwaysOnTop.h"

#include "System/WindowsHelper.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

AlwaysOnTop::AlwaysOnTop(QWidget* parent)
	: Component(parent, ToolType::AlwaysOnTop)
	, windowTable(new QTableWidget(this))
{
	windowTable->setColumnCount(2);
	windowTable->setHorizontalHeaderLabels({"Window", "Always on top"});
	windowTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	windowTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	windowTable->verticalHeader()->hide();
	windowTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	windowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	windowTable->setIconSize({24, 24});

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(windowTable);

	connect(windowTable, &QTableWidget::itemChanged, this, &AlwaysOnTop::WindowItemChanged);

	auto refreshTimer = new QTimer(this);
	refreshTimer->setInterval(1'000);
	connect(refreshTimer, &QTimer::timeout, this, &AlwaysOnTop::RefreshWindows);
	refreshTimer->start();
	RefreshWindows();
}

void AlwaysOnTop::RefreshWindows()
{
	const auto scrollPosition = windowTable->verticalScrollBar()->value();
	const auto signalBlocker = QSignalBlocker(windowTable);
	windowTable->setUpdatesEnabled(false);

	const auto windows = WindowsHelper::GetWindows();
	windowTable->setRowCount(windows.size());
	for (int row = 0; row < windows.size(); ++row) {
		const auto& window = windows[row];
		auto windowItem = new QTableWidgetItem(window.Icon, window.Title);
		windowItem->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(window.Handle)));
		windowTable->setItem(row, 0, windowItem);

		auto alwaysOnTopItem = new QTableWidgetItem;
		alwaysOnTopItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
		alwaysOnTopItem->setCheckState(WindowsHelper::IsAlwaysOnTop(window.Handle) ? Qt::Checked : Qt::Unchecked);
		alwaysOnTopItem->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(window.Handle)));
		windowTable->setItem(row, 1, alwaysOnTopItem);
	}

	windowTable->setUpdatesEnabled(true);
	windowTable->verticalScrollBar()->setValue(scrollPosition);
}

void AlwaysOnTop::WindowItemChanged(QTableWidgetItem* item)
{
	if (item->column() != 1) {
		return;
	}

	auto window = reinterpret_cast<HWND>(item->data(Qt::UserRole).toULongLong());
	const bool alwaysOnTop = item->checkState() == Qt::Checked;
	if (WindowsHelper::SetAlwaysOnTop(window, alwaysOnTop)) {
		return;
	}

	const auto signalBlocker = QSignalBlocker(windowTable);
	item->setCheckState(alwaysOnTop ? Qt::Unchecked : Qt::Checked);
	QMessageBox::critical(this, "Always on top", "Unable to update the selected window.");
}
