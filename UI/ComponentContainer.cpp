#include "ComponentContainer.h"
#include "ComponentPopoutWindow.h"
#include "ToolConfigurationParameter.h"

#include <QMenu>
#include <QApplication>
#include <QDrag>
#include <QInputDialog>
#include <QIcon>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QMimeData>
#include <QScreen>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

ComponentContainer::ComponentContainer(Component* component, QWidget* parent)
	: QWidget(parent)
	, component(component)
{
	ui.setupUi(this);
	component->container = this;

	ui.Content->layout()->addWidget(component);
	ui.ConfigurationBtn->setIcon(QIcon(":/icons/tool-options.svg"));
	ui.ConfigurationBtn->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0px; }");
	ui.Title->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	ui.CloseBtn->setIcon(QIcon(":/icons/tool-close.svg"));
	PopulateConfigurationMenu();
	UpdateCollapseButton();
	ui.Title->setMaximumHeight(ui.Title->sizeHint().height());
	ui.Title->installEventFilter(this);
	ui.TitleLabel->installEventFilter(this);

	connect(ui.CloseBtn, &QToolButton::clicked, this, &ComponentContainer::OnCloseClicked);
	connect(ui.CollapseBtn, &QToolButton::clicked, this, &ComponentContainer::OnCollapseClicked);
	connect(ui.Title, &QWidget::customContextMenuRequested, this, &ComponentContainer::OnTitleContextMenuRequested);
}

void ComponentContainer::PopulateConfigurationMenu()
{
	const auto parameters = component->GetConfigurationParameters();
	ui.ConfigurationBtn->setVisible(!parameters.isEmpty());

	auto* menu = new QMenu(ui.ConfigurationBtn);
	for (auto* parameter : parameters) {
		if (!parameter) {
			continue;
		}

		auto* action = menu->addAction(parameter->GetName());
		connect(action, &QAction::triggered, parameter, [this, parameter] {
			parameter->Edit(this);
		});
		connect(parameter, &ToolConfigurationParameter::Changed, component, [this] {
			emit component->Modified(component);
		});
	}
	ui.ConfigurationBtn->setMenu(menu);
}

ComponentContainer::~ComponentContainer()
{
}

QJsonObject ComponentContainer::SaveState()
{
	QJsonObject state;

	state["Title"] = getTitle();
	auto currentPopoutSize = GetCurrentPopoutSize();
	if (currentPopoutSize.isValid()) {
		state["PopoutWidth"] = currentPopoutSize.width();
		state["PopoutHeight"] = currentPopoutSize.height();
	}

	return state;
}

void ComponentContainer::LoadState(const QJsonObject& state)
{
	setTitle(state["Title"].toString());
	int popoutWidth = state["PopoutWidth"].toInt();
	int popoutHeight = state["PopoutHeight"].toInt();
	if (popoutWidth > 0 && popoutHeight > 0) {
		popoutSize = QSize(popoutWidth, popoutHeight);
	}
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

void ComponentContainer::setFrameMargins(int left, int top, int right, int bottom)
{
	ui.verticalLayout->setContentsMargins(left, top, right, bottom);
}

void ComponentContainer::setCollapseEnabled(bool enabled)
{
	ui.CollapseBtn->setEnabled(enabled);
	if (!enabled && getCollapsed()) {
		ui.Content->show();
		UpdateCollapseButton();
		UpdatePopoutSize(false);
		emit CollapseClicked(false);
	}
}

bool ComponentContainer::getCollapsed() const
{
	return ui.Content->isHidden();
}

Component* ComponentContainer::GetComponent() const
{
	return component;
}

void ComponentContainer::ShowPopoutAt(const QPoint& position)
{
	if (!popoutWindow) {
		PopOut(true);
	}

	if (popoutWindow) {
		auto screen = QGuiApplication::screenAt(position);
		if (!screen) {
			screen = QGuiApplication::primaryScreen();
		}

		auto windowSize = popoutWindow->size();
		if (screen) {
			auto available = screen->availableGeometry();
			windowSize.setWidth(std::min(windowSize.width(), available.width()));
			windowSize.setHeight(std::min(windowSize.height(), available.height()));
			if (windowSize != popoutWindow->size()) {
				popoutWindow->resize(windowSize);
			}

			auto windowPosition = position - QPoint(windowSize.width() / 2, windowSize.height() / 2);
			int maximumX = available.right() - windowSize.width() + 1;
			int maximumY = available.bottom() - windowSize.height() + 1;
			windowPosition.setX(std::clamp(windowPosition.x(), available.left(), maximumX));
			windowPosition.setY(std::clamp(windowPosition.y(), available.top(), maximumY));
			popoutWindow->move(windowPosition);
		} else {
			popoutWindow->move(position - QPoint(windowSize.width() / 2, windowSize.height() / 2));
		}

		popoutWindow->show();
		popoutWindow->raise();
		popoutWindow->activateWindow();
	}
}

bool ComponentContainer::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == popoutWindow && event->type() == QEvent::Close) {
		RestoreToWorkspace();
		event->ignore();
		return true;
	}

	if (watched == popoutWindow && event->type() == QEvent::Resize
		&& !getCollapsed() && !popoutWindow->isMaximized() && popoutSize != popoutWindow->size()) {
		popoutSize = popoutWindow->size();
		emit component->Modified(component, false);
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

	if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
		componentDragStart = mouseEvent->position().toPoint();
		componentDragPending = true;
		return true;
	}

	if (event->type() == QEvent::MouseMove && componentDragPending
		&& mouseEvent->buttons().testFlag(Qt::LeftButton)
		&& (mouseEvent->position().toPoint() - componentDragStart).manhattanLength() >= QApplication::startDragDistance()) {
		componentDragPending = false;
		auto drag = new QDrag(this);
		auto mimeData = new QMimeData;
		mimeData->setData("application/x-hx-component", {});
		drag->setMimeData(mimeData);
		drag->exec(Qt::MoveAction);
		return true;
	}

	if (event->type() == QEvent::MouseMove && titleDragging && popoutWindow
		&& mouseEvent->buttons().testFlag(Qt::LeftButton)) {
		popoutWindow->move(mouseEvent->globalPosition().toPoint() - titleDragOffset);
		return true;
	}

	if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
		titleDragging = false;
		componentDragPending = false;
		return true;
	}

	return QWidget::eventFilter(watched, event);
}
void ComponentContainer::PopOut(bool independent)
{
	if (popoutWindow) {
		return;
	}

	workspaceRow = parentWidget();
	setFrameMargins(5, 5, 5, 5);
	auto workspaceLayout = dynamic_cast<QBoxLayout*>(workspaceRow->layout());
	auto workspaceSplitter = qobject_cast<QSplitter*>(workspaceRow);
	assert(workspaceLayout || workspaceSplitter);
	workspacePosition = workspaceLayout ? workspaceLayout->indexOf(this) : workspaceSplitter->indexOf(this);
	auto windowPosition = mapToGlobal(QPoint{});
	auto windowSize = popoutSize;
	if (!windowSize.isValid()) {
		windowSize = sizeHint();
		if (!windowSize.isValid()) {
			windowSize = minimumSizeHint();
		}

		if (!windowSize.isValid()) {
			windowSize = size();
		}

		popoutSize = windowSize;
	}

	if (workspaceLayout) {
		workspaceLayout->removeWidget(this);
	}

	workspacePlaceholder = new QWidget;
	workspacePlaceholder->hide();
	if (workspaceLayout) {
		workspaceLayout->insertWidget(workspacePosition, workspacePlaceholder);
	} else {
		workspaceSplitter->insertWidget(workspacePosition, workspacePlaceholder);
	}

	auto window = new ComponentPopoutWindow(independent ? nullptr : workspaceRow.data());
	popoutWindow = window;
	if (independent) {
		connect(workspaceRow, &QObject::destroyed, window, &QObject::deleteLater);
	}
	window->setWindowTitle(getTitle());
	auto windowLayout = new QVBoxLayout(window);
	windowLayout->setContentsMargins(0, 0, 0, 0);
	windowLayout->addWidget(this);
	window->resize(windowSize);
	window->move(windowPosition);
	window->show();
	window->installEventFilter(this);
	expandedWindowSize = {};
	component->OnPopOut();

	emit WorkspaceLayoutChanged(workspaceRow, false);
}

void ComponentContainer::RestoreToWorkspace()
{
	if (!popoutWindow || !workspaceRow) {
		return;
	}

	auto currentPopoutSize = GetCurrentPopoutSize();
	bool popoutSizeChanged = currentPopoutSize.isValid() && currentPopoutSize != popoutSize;
	if (currentPopoutSize.isValid()) {
		popoutSize = currentPopoutSize;
	}

	auto window = popoutWindow.data();
	window->removeEventFilter(this);
	window->layout()->removeWidget(this);
	popoutWindow = {};
	titleDragging = false;
	auto workspaceLayout = dynamic_cast<QBoxLayout*>(workspaceRow->layout());
	auto workspaceSplitter = qobject_cast<QSplitter*>(workspaceRow);
	assert(workspaceLayout || workspaceSplitter);
	workspacePosition = workspaceLayout ? workspaceLayout->indexOf(workspacePlaceholder)
		: workspaceSplitter->indexOf(workspacePlaceholder);
	if (workspaceLayout) {
		workspaceLayout->removeWidget(workspacePlaceholder);
	}

	workspacePlaceholder->setParent(nullptr);
	workspacePlaceholder->deleteLater();
	workspacePlaceholder = {};
	if (workspaceLayout) {
		workspaceLayout->insertWidget(workspacePosition, this, 1);
	} else {
		workspaceSplitter->insertWidget(workspacePosition, this);
	}
	show();
	window->hide();
	window->deleteLater();

	emit WorkspaceLayoutChanged(workspaceRow, true);
	if (popoutSizeChanged) {
		emit component->Modified(component, false);
	}
}

QSize ComponentContainer::GetCurrentPopoutSize() const
{
	if (!popoutWindow) {
		return popoutSize;
	}

	if (getCollapsed() && expandedWindowSize.isValid()) {
		return QSize(popoutWindow->width(), expandedWindowSize.height());
	}

	if (popoutWindow->isMaximized() && popoutWindow->normalGeometry().isValid()) {
		return popoutWindow->normalGeometry().size();
	}

	return popoutWindow->size();
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
