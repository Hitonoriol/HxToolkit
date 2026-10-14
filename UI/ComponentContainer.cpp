#include "ComponentContainer.h"
#include "ComponentPopoutWindow.h"

#include <QMenu>
#include <QInputDialog>
#include <QIcon>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

ComponentContainer::ComponentContainer(Component* component, QWidget* parent)
	: QWidget(parent)
	, component(component)
{
	ui.setupUi(this);
	component->container = this;

	ui.Content->layout()->addWidget(component);
	ui.Title->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	ui.CloseBtn->setIcon(QIcon(":/icons/tool-close.svg"));
	UpdateCollapseButton();
	ui.Title->setMaximumHeight(ui.Title->sizeHint().height());
	ui.Title->installEventFilter(this);
	ui.TitleLabel->installEventFilter(this);

	connect(ui.CloseBtn, &QToolButton::clicked, this, &ComponentContainer::OnCloseClicked);
	connect(ui.CollapseBtn, &QToolButton::clicked, this, &ComponentContainer::OnCollapseClicked);
	connect(ui.Title, &QWidget::customContextMenuRequested, this, &ComponentContainer::OnTitleContextMenuRequested);
}

ComponentContainer::~ComponentContainer()
{
}

QJsonObject ComponentContainer::SaveState()
{
	QJsonObject state;

	state["Title"] = getTitle();

	return state;
}

void ComponentContainer::LoadState(const QJsonObject& state)
{
	setTitle(state["Title"].toString());
}

void ComponentContainer::setTitle(const QString& title)
{
	ui.TitleLabel->setText(title);

	if (popoutWindow) {
		popoutWindow->setWindowTitle(title);
	}
}

QString ComponentContainer::getTitle()
{
	return ui.TitleLabel->text();
}

void ComponentContainer::setFillContainer(bool fill)
{
	auto lastItem = ui.ContentLayout->itemAt(ui.ContentLayout->count() - 1);
	const bool hasSpacer = lastItem && lastItem->spacerItem();

	if (fill == !hasSpacer) {
		return;
	}

	if (fill) {
		delete ui.ContentLayout->takeAt(ui.ContentLayout->count() - 1);
	} else {
		ui.ContentLayout->addStretch();
	}
}

bool ComponentContainer::getCollapsed() const
{
	return ui.Content->isHidden();
}

bool ComponentContainer::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == popoutWindow && event->type() == QEvent::Close) {
		RestoreToWorkspace();
		event->ignore();
		return true;
	}

	if (watched != ui.Title && watched != ui.TitleLabel) {
		return QWidget::eventFilter(watched, event);
	}

	auto mouseEvent = dynamic_cast<QMouseEvent*>(event);

	if (!mouseEvent) {
		return QWidget::eventFilter(watched, event);
	}

	if (event->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton) {
		titleDragging = false;

		if (!popoutWindow) {
			PopOut();
		} else if (popoutWindow->isMaximized()) {
			popoutWindow->showNormal();
		} else {
			popoutWindow->showMaximized();
		}

		return true;
	}

	if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton && popoutWindow) {
		titleDragging = !popoutWindow->isMaximized();
		titleDragOffset = mouseEvent->globalPosition().toPoint() - popoutWindow->pos();
		return true;
	}

	if (event->type() == QEvent::MouseMove && titleDragging && popoutWindow
		&& mouseEvent->buttons().testFlag(Qt::LeftButton)) {
		popoutWindow->move(mouseEvent->globalPosition().toPoint() - titleDragOffset);
		return true;
	}

	if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
		titleDragging = false;
		return true;
	}

	return QWidget::eventFilter(watched, event);
}
void ComponentContainer::PopOut()
{
	if (popoutWindow) {
		return;
	}

	workspaceRow = parentWidget();
	auto workspaceLayout = dynamic_cast<QBoxLayout*>(workspaceRow->layout());
	assert(workspaceLayout);
	workspacePosition = workspaceLayout->indexOf(this);
	auto windowPosition = mapToGlobal(QPoint{});
	auto windowSize = size();
	workspaceLayout->removeWidget(this);
	workspacePlaceholder = new QWidget(workspaceRow);
	workspacePlaceholder->hide();
	workspaceLayout->insertWidget(workspacePosition, workspacePlaceholder);

	auto window = new ComponentPopoutWindow(workspaceRow);
	popoutWindow = window;
	window->setWindowTitle(getTitle());
	window->installEventFilter(this);
	auto windowLayout = new QVBoxLayout(window);
	windowLayout->setContentsMargins(0, 0, 0, 0);
	windowLayout->addWidget(this);
	window->resize(windowSize);
	window->move(windowPosition);
	window->show();
	expandedWindowSize = {};

	emit WorkspaceLayoutChanged(workspaceRow, false);
}

void ComponentContainer::RestoreToWorkspace()
{
	if (!popoutWindow || !workspaceRow) {
		return;
	}

	auto window = popoutWindow.data();
	window->removeEventFilter(this);
	window->layout()->removeWidget(this);
	popoutWindow = {};
	titleDragging = false;
	auto workspaceLayout = dynamic_cast<QBoxLayout*>(workspaceRow->layout());
	assert(workspaceLayout);
	workspacePosition = workspaceLayout->indexOf(workspacePlaceholder);
	workspaceLayout->removeWidget(workspacePlaceholder);
	workspacePlaceholder->deleteLater();
	workspacePlaceholder = {};
	workspaceLayout->insertWidget(workspacePosition, this, 1);
	show();
	window->hide();
	window->deleteLater();

	emit WorkspaceLayoutChanged(workspaceRow, true);
}

void ComponentContainer::OnCloseClicked()
{
	if (popoutWindow) {
		popoutWindow->close();
		return;
	}

	emit CloseClicked();
}

void ComponentContainer::OnCollapseClicked()
{
	const bool collapse = !ui.Content->isHidden();
	ui.Content->setVisible(!collapse);
	UpdateCollapseButton();
	UpdatePopoutSize(collapse);
	emit CollapseClicked(collapse);
}

void ComponentContainer::UpdatePopoutSize(bool collapsed)
{
	if (!popoutWindow) {
		return;
	}

	auto containerSizePolicy = sizePolicy();
	containerSizePolicy.setVerticalPolicy(collapsed ? QSizePolicy::Maximum : QSizePolicy::Preferred);
	setSizePolicy(containerSizePolicy);

	if (collapsed) {
		if (popoutWindow->isMaximized()) {
			popoutWindow->showNormal();
		}

		expandedWindowSize = popoutWindow->size();
		auto margins = layout()->contentsMargins();
		auto collapsedHeight = ui.Title->sizeHint().height() + margins.top() + margins.bottom();
		popoutWindow->setFixedHeight(collapsedHeight);
		popoutWindow->resize(popoutWindow->width(), collapsedHeight);
		auto window = popoutWindow;
		QTimer::singleShot(0, this, [this, window, collapsedHeight] {
			if (window && window == popoutWindow && getCollapsed()) {
				window->resize(window->width(), collapsedHeight);
			}
		});
	} else {
		auto expandedHeight = expandedWindowSize.isValid() ? expandedWindowSize.height() : sizeHint().height();
		popoutWindow->setMinimumHeight(0);
		popoutWindow->setMaximumHeight(QWIDGETSIZE_MAX);
		popoutWindow->resize(popoutWindow->width(), expandedHeight);
		expandedWindowSize = {};
	}
}

void ComponentContainer::UpdateCollapseButton()
{
	const bool expanded = !ui.Content->isHidden();
	ui.CollapseBtn->setIcon(QIcon(expanded ? ":/icons/tool-collapse.svg" : ":/icons/tool-expand.svg"));
	ui.CollapseBtn->setToolTip(expanded ? "Collapse tool" : "Expand tool");
}

void ComponentContainer::OnTitleContextMenuRequested(const QPoint& pos)
{
	QMenu menu(this);

	connect(menu.addAction("Rename"), &QAction::triggered, this, &ComponentContainer::OnRenameTriggered);

	if (popoutWindow) {
		auto alwaysOnTopAction = menu.addAction("Always on top");
		alwaysOnTopAction->setCheckable(true);
		alwaysOnTopAction->setChecked(popoutWindow->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
		connect(alwaysOnTopAction, &QAction::toggled, this, &ComponentContainer::OnAlwaysOnTopToggled);
	}

	menu.exec(ui.Title->mapToGlobal(pos));
}

void ComponentContainer::OnAlwaysOnTopToggled(bool alwaysOnTop)
{
	if (!popoutWindow) {
		return;
	}

	const bool maximized = popoutWindow->isMaximized();
	popoutWindow->setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);

	if (maximized) {
		popoutWindow->showMaximized();
	} else {
		popoutWindow->show();
	}
}

void ComponentContainer::OnRenameTriggered()
{
	bool entered = false;
	auto newTitle = QInputDialog::getText(this, "Rename component", "Component title:", QLineEdit::Normal, getTitle(), &entered);

	if (entered && !newTitle.isEmpty()) {
		setTitle(newTitle);
		emit component->Modified(component);
	}
}
