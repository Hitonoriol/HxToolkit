#pragma once

#include <QSet>
#include <QWidget>

class QIcon;
class QPaintEvent;
class QStackedWidget;
class QTabBar;

class DynamicTabWidget : public QWidget
{
	Q_OBJECT

public:
	explicit DynamicTabWidget(QWidget* parent = nullptr);

	int AddTab(QWidget* page, const QString& title);
	int InsertTab(int tabIdx, QWidget* page, const QString& title);
	void RemoveTab(int tabIdx);
	int GetCount() const;
	QWidget* GetWidget(int tabIdx) const;
	int GetIndexOf(QWidget* page) const;
	int GetCurrentIndex() const;
	QWidget* GetCurrentWidget() const;
	void SetCurrentIndex(int tabIdx);
	void SetCurrentWidget(QWidget* page);
	QString GetTabText(int tabIdx) const;
	void SetTabText(int tabIdx, const QString& title);
	void SetTabIcon(int tabIdx, const QIcon& icon);
	bool GetTabPinned(int tabIdx) const;
	void SetTabPinned(int tabIdx, bool pinned);
	QTabBar* GetTabBar() const;

signals:
	void CurrentChanged(int tabIdx);
	void TabCloseRequested(int tabIdx);
	void AddTabRequested();

protected:
	void paintEvent(QPaintEvent* event) override;

private slots:
	void OnCurrentChanged(int tabIdx);
	void OnTabCloseRequested(int tabIdx);
	void OnTabMoved(int from, int to);

private:
	void UpdateCloseButtons();
	void UpdateTabBarMaximumWidth();

	QTabBar* tabBar;
	QStackedWidget* pages;
	QSet<QWidget*> pinnedPages;
};
