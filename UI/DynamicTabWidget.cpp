#include "DynamicTabWidget.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QStyleOptionTabBarBase>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

DynamicTabWidget::DynamicTabWidget(QWidget* parent)
	: QWidget(parent)
{
	auto rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(0);
	auto tabRow = new QWidget(this);
	auto tabLayout = new QHBoxLayout(tabRow);
	tabLayout->setContentsMargins(4, 0, 0, 0);
	tabLayout->setSpacing(1);
	tabBar = new QTabBar(tabRow);
	tabBar->setDocumentMode(true);
	tabBar->setElideMode(Qt::ElideLeft);
	tabBar->setExpanding(false);
	tabBar->setMovable(true);
	tabBar->setTabsClosable(true);
	auto addTabButton = new QToolButton(tabRow);
	addTabButton->setIcon(QIcon(":/icons/add.svg"));
	addTabButton->setIconSize({16, 16});
	addTabButton->setToolTip("New tab");
	addTabButton->setFocusPolicy(Qt::NoFocus);
	addTabButton->setAutoRaise(true);
	addTabButton->setFixedSize(30, 30);
	tabLayout->addWidget(tabBar);
	tabLayout->addWidget(addTabButton, 0, Qt::AlignVCenter);
	tabLayout->addStretch();
	pages = new QStackedWidget(this);
	rootLayout->addWidget(tabRow);
	rootLayout->addWidget(pages, 1);

	connect(addTabButton, &QToolButton::clicked, this, &DynamicTabWidget::AddTabRequested);
	connect(tabBar, &QTabBar::currentChanged, this, &DynamicTabWidget::OnCurrentChanged);
	connect(tabBar, &QTabBar::tabCloseRequested, this, &DynamicTabWidget::TabCloseRequested);
	connect(tabBar, &QTabBar::tabMoved, this, &DynamicTabWidget::OnTabMoved);
}

void DynamicTabWidget::paintEvent(QPaintEvent* event)
{
	QWidget::paintEvent(event);

	QStyleOptionTabBarBase option;
	option.initFrom(tabBar);
	option.rect = {0, 0, width(), tabBar->height()};
	option.shape = tabBar->shape();
	option.documentMode = tabBar->documentMode();
	QPainter painter(this);
	style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &option, &painter, tabBar);
}

int DynamicTabWidget::AddTab(QWidget* page, const QString& title)
{
	return InsertTab(GetCount(), page, title);
}

int DynamicTabWidget::InsertTab(int tabIdx, QWidget* page, const QString& title)
{
	tabIdx = std::clamp(tabIdx, 0, GetCount());
	pages->insertWidget(tabIdx, page);
	auto insertedTabIdx = tabBar->insertTab(tabIdx, title);
	UpdateTabBarMaximumWidth();
	return insertedTabIdx;
}

void DynamicTabWidget::RemoveTab(int tabIdx)
{
	if (tabIdx < 0 || tabIdx >= GetCount()) {
		return;
	}

	auto page = pages->widget(tabIdx);
	pages->removeWidget(page);
	tabBar->removeTab(tabIdx);
	UpdateTabBarMaximumWidth();
}

int DynamicTabWidget::GetCount() const
{
	return tabBar->count();
}

QWidget* DynamicTabWidget::GetWidget(int tabIdx) const
{
	return pages->widget(tabIdx);
}

int DynamicTabWidget::GetIndexOf(QWidget* page) const
{
	return pages->indexOf(page);
}

int DynamicTabWidget::GetCurrentIndex() const
{
	return tabBar->currentIndex();
}

QWidget* DynamicTabWidget::GetCurrentWidget() const
{
	return pages->currentWidget();
}

void DynamicTabWidget::SetCurrentIndex(int tabIdx)
{
	tabBar->setCurrentIndex(tabIdx);
}

void DynamicTabWidget::SetCurrentWidget(QWidget* page)
{
	SetCurrentIndex(GetIndexOf(page));
}

QString DynamicTabWidget::GetTabText(int tabIdx) const
{
	return tabBar->tabText(tabIdx);
}

void DynamicTabWidget::SetTabText(int tabIdx, const QString& title)
{
	tabBar->setTabText(tabIdx, title);
	UpdateTabBarMaximumWidth();
}

void DynamicTabWidget::SetTabIcon(int tabIdx, const QIcon& icon)
{
	tabBar->setTabIcon(tabIdx, icon);
	UpdateTabBarMaximumWidth();
}

QTabBar* DynamicTabWidget::GetTabBar() const
{
	return tabBar;
}

void DynamicTabWidget::OnCurrentChanged(int tabIdx)
{
	pages->setCurrentIndex(tabIdx);
	emit CurrentChanged(tabIdx);
}

void DynamicTabWidget::OnTabMoved(int from, int to)
{
	auto page = pages->widget(from);
	pages->removeWidget(page);
	pages->insertWidget(to, page);
	pages->setCurrentIndex(tabBar->currentIndex());
}

void DynamicTabWidget::UpdateTabBarMaximumWidth()
{
	tabBar->setMaximumWidth(tabBar->sizeHint().width());
}
