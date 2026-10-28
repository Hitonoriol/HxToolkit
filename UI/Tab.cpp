#include "Tab.h"
#include "ComponentContainer.h"

#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHash>
#include <QJsonArray>
#include <QMimeData>
#include <QSplitter>
#include <QSplitterHandle>

#include <algorithm>

Tab::Tab(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	ui.RootLayout->setAlignment(ui.ToolbarLayout, Qt::AlignTop);
	workspaceSplitter = new QSplitter(Qt::Vertical, this);
	workspaceSplitter->setChildrenCollapsible(false);
	workspaceSplitter->setHandleWidth(5);
	workspaceSplitter->setStyleSheet(
		"QSplitter::handle { background: transparent; }"
		"QSplitter::handle:horizontal:hover { background: rgba(38, 140, 255, 100); margin: 0 1px; }"
		"QSplitter::handle:vertical:hover { background: rgba(38, 140, 255, 100); margin: 1px 0; }"
		"QSplitter::handle:horizontal:pressed { background: #268cff; margin: 0 1px; }"
		"QSplitter::handle:vertical:pressed { background: #268cff; margin: 1px 0; }");
	ui.WorkspaceLayout->addWidget(workspaceSplitter);
	connect(workspaceSplitter, &QSplitter::splitterMoved, this, [this] { Modify(); });
	setAcceptDrops(true);
	dropIndicator = new QWidget(this);
	dropIndicator->setStyleSheet("background: #268cff;");
	dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
	dropIndicator->hide();

	connect(ui.AddToolBtn, &QToolButton::clicked, this, &Tab::AddToolRequested);
}

Tab::~Tab()
{}

void Tab::ComponentModified(Component* component, bool undoable)
{
	modified = true;
	emit TabModified(this, component, undoable);
}

void Tab::SetSavePath(const QString& savePath)
{
	this->savePath = savePath;
}

QString Tab::GetSavePath() const
{
	return savePath;
}

void Tab::AddComponent(Component* component, const QString& title, bool fillContainer)
{
	auto container = new ComponentContainer(component, this);
	container->setTitle(title);
	container->setFillContainer(fillContainer);
	auto row = qobject_cast<QSplitter*>(GetLastRow());

	if (!row || verticalSplit) {
		row = CreateRow(workspaceSplitter->count());
	}

	row->addWidget(container);
	InstallSplitterHandleFilters(row);
	UpdateComponentMargins();
	UpdateRowStretch(row);

	connect(component, &Component::Modified, this, &Tab::ComponentModified);
	connect(container, &ComponentContainer::CloseClicked, this, std::bind(&Tab::OnComponentClosed, this, container));
	connect(container, &ComponentContainer::CollapseClicked, this, [this, container](bool collapse) {
		if (container->parentWidget()->window() == window()) {
			OnComponentCollapse(container, collapse);
		}
	});

	connect(container, &ComponentContainer::WorkspaceLayoutChanged, this, [this, container](QWidget* row, bool restored) {
		if (restored) {
			OnComponentCollapse(container, container->getCollapsed());
		} else {
			UpdateRowStretch(row);
		}

		UpdateComponentMargins();
	});

	emit ComponentAttached(this, component);
	emit ComponentAdded(this, container, GetComponentLocation(container));
}

void Tab::OnComponentClosed(ComponentContainer* container)
{
	auto location = DetachComponent(container);
	emit ComponentRemoved(this, container, location);
}

void Tab::dragEnterEvent(QDragEnterEvent* event)
{
	auto source = qobject_cast<ComponentContainer*>(event->source());
	if (source && event->mimeData()->hasFormat("application/x-hx-component")
		&& GetComponentLocation(source).Row >= 0) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		return;
	}

	event->ignore();
}

void Tab::dragMoveEvent(QDragMoveEvent* event)
{
	auto source = qobject_cast<ComponentContainer*>(event->source());
	if (source && UpdateDropIndicator(event->position().toPoint(), source)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		return;
	}

	dropIndicator->hide();
	event->ignore();
}

void Tab::dragLeaveEvent(QDragLeaveEvent* event)
{
	dropIndicator->hide();
	event->accept();
}

void Tab::dropEvent(QDropEvent* event)
{
	auto source = qobject_cast<ComponentContainer*>(event->source());
	if (source && UpdateDropIndicator(event->position().toPoint(), source)) {
		MoveComponent(source, dropLocation, dropCreatesRow);
		dropIndicator->hide();
		event->setDropAction(Qt::MoveAction);
		event->accept();
		return;
	}

	dropIndicator->hide();
	event->ignore();
}

bool Tab::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		auto handle = qobject_cast<QSplitterHandle*>(watched);
		auto splitter = handle ? qobject_cast<QSplitter*>(handle->parentWidget()) : nullptr;
		if (splitter && splitter->count() > 1) {
			splitter->setSizes(QList<int>(splitter->count(), 1));
			Modify();
			return true;
		}
	}

	return QWidget::eventFilter(watched, event);
}

bool Tab::UpdateDropIndicator(const QPoint& position, ComponentContainer* source)
{
	if (!source || GetComponentLocation(source).Row < 0) {
		return false;
	}

	ComponentContainer* target{};
	QRect targetRect;
	int targetRow = -1;
	int targetColumn = -1;
	bool sourceContainsPosition = false;
	for (int rowIdx = 0; rowIdx < workspaceSplitter->count() && !target; ++rowIdx) {
		auto row = qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx));
		for (int columnIdx = 0; columnIdx < row->count(); ++columnIdx) {
			auto container = qobject_cast<ComponentContainer*>(row->widget(columnIdx));
			if (!container) {
				continue;
			}

			auto rect = QRect(container->mapTo(this, QPoint{}), container->size());
			if (container == source) {
				sourceContainsPosition = rect.contains(position);
				continue;
			}

			if (rect.contains(position)) {
				target = container;
				targetRect = rect;
				targetRow = rowIdx;
				targetColumn = columnIdx;
				break;
			}
		}
	}

	if (!target) {
		if (sourceContainsPosition) {
			return false;
		}

		auto workspaceRect = QRect(workspaceSplitter->mapTo(this, QPoint{}), workspaceSplitter->size());
		if (!workspaceRect.contains(position)) {
			return false;
		}

		bool insertAtTop = workspaceSplitter->count() > 0
			&& position.y() < workspaceSplitter->widget(0)->mapTo(this, QPoint{}).y()
				+ workspaceSplitter->widget(0)->height() / 2;
		dropLocation = {insertAtTop ? 0 : workspaceSplitter->count(), 0};
		dropCreatesRow = true;
		int indicatorY = insertAtTop ? workspaceRect.top() : workspaceRect.bottom() - DropIndicatorThickness + 1;
		dropIndicator->setGeometry(workspaceRect.left(), indicatorY, workspaceRect.width(), DropIndicatorThickness);
		dropIndicator->raise();
		dropIndicator->show();
		return true;
	}

	double leftDistance = static_cast<double>(position.x() - targetRect.left()) / targetRect.width();
	double rightDistance = static_cast<double>(targetRect.right() - position.x()) / targetRect.width();
	double topDistance = static_cast<double>(position.y() - targetRect.top()) / targetRect.height();
	double bottomDistance = static_cast<double>(targetRect.bottom() - position.y()) / targetRect.height();
	double closestDistance = std::min({leftDistance, rightDistance, topDistance, bottomDistance});
	if (closestDistance == leftDistance || closestDistance == rightDistance) {
		bool insertRight = closestDistance == rightDistance;
		dropLocation = {targetRow, targetColumn + (insertRight ? 1 : 0)};
		dropCreatesRow = false;
		int indicatorX = insertRight ? targetRect.right() - DropIndicatorThickness + 1 : targetRect.left();
		dropIndicator->setGeometry(indicatorX, targetRect.top(), DropIndicatorThickness, targetRect.height());
	} else {
		bool insertBelow = closestDistance == bottomDistance;
		dropLocation = {targetRow + (insertBelow ? 1 : 0), 0};
		dropCreatesRow = true;
		auto workspaceRect = QRect(workspaceSplitter->mapTo(this, QPoint{}), workspaceSplitter->size());
		int indicatorY = insertBelow ? targetRect.bottom() - DropIndicatorThickness + 1 : targetRect.top();
		dropIndicator->setGeometry(workspaceRect.left(), indicatorY, workspaceRect.width(), DropIndicatorThickness);
	}

	dropIndicator->raise();
	dropIndicator->show();
	return true;
}

void Tab::MoveComponent(ComponentContainer* container, ComponentLocation location, bool createRow)
{
	auto sourceLocation = GetComponentLocation(container);
	if (sourceLocation.Row < 0) {
		return;
	}

	auto sourceRow = container->parentWidget();
	auto sourceSplitter = qobject_cast<QSplitter*>(sourceRow);
	bool sourceRowRemoved = sourceSplitter->count() == 1;
	if (createRow) {
		if (sourceRowRemoved && sourceLocation.Row < location.Row) {
			--location.Row;
		}
	} else if (sourceLocation.Row == location.Row) {
		if (sourceLocation.Column < location.Column) {
			--location.Column;
		}

		if (sourceLocation.Column == location.Column) {
			return;
		}
	} else if (sourceRowRemoved && sourceLocation.Row < location.Row) {
		--location.Row;
	}

	DetachComponent(container);
	AttachComponent(container, location, createRow);
	Modify();
}

void Tab::AttachComponent(ComponentContainer* container, const ComponentLocation& location, bool createRow)
{
	auto rowIdx = std::clamp(location.Row, 0, workspaceSplitter->count());
	auto row = !createRow && rowIdx < workspaceSplitter->count()
		? qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx)) : nullptr;

	if (!row) {
		row = CreateRow(rowIdx);
	}

	row->insertWidget(std::clamp(location.Column, 0, row->count()), container);
	InstallSplitterHandleFilters(row);
	UpdateComponentMargins();
	UpdateRowStretch(row);
	emit ComponentAttached(this, container->GetComponent());
}

Tab::ComponentLocation Tab::DetachComponent(ComponentContainer* container)
{
	auto location = GetComponentLocation(container);
	auto component = container->GetComponent();
	auto row = qobject_cast<QSplitter*>(container->parentWidget());
	container->setParent(nullptr);

	if (row->count() == 0) {
		row->setParent(nullptr);
		row->deleteLater();
	} else {
		UpdateRowStretch(row);
	}
	UpdateComponentMargins();

	emit ComponentDetached(component);
	return location;
}

void Tab::OnComponentCollapse(ComponentContainer* container, bool collapse)
{
	auto row = container->parentWidget();
	auto containerSizePolicy = container->sizePolicy();
	containerSizePolicy.setVerticalPolicy(collapse ? QSizePolicy::Maximum : QSizePolicy::Preferred);
	container->setSizePolicy(containerSizePolicy);
	UpdateRowStretch(row);
}

void Tab::UpdateRowStretch(QWidget* row)
{
	auto rowSplitter = qobject_cast<QSplitter*>(row);
	bool hasExpandedComponent = false;
	bool collapseEnabled = rowSplitter->count() == 1;

	for (int itemIdx = 0; itemIdx < rowSplitter->count(); ++itemIdx) {
		auto container = qobject_cast<ComponentContainer*>(rowSplitter->widget(itemIdx));
		if (container) {
			container->setCollapseEnabled(collapseEnabled);
		}

		if (container && !container->getCollapsed()) {
			hasExpandedComponent = true;
		}
	}

	workspaceSplitter->setStretchFactor(workspaceSplitter->indexOf(row), hasExpandedComponent ? 1 : 0);
	auto rowSizePolicy = row->sizePolicy();
	rowSizePolicy.setVerticalPolicy(hasExpandedComponent ? QSizePolicy::Preferred : QSizePolicy::Maximum);
	row->setSizePolicy(rowSizePolicy);
	row->setMaximumHeight(hasExpandedComponent ? QWIDGETSIZE_MAX : row->sizeHint().height());
	row->updateGeometry();
}

void Tab::UpdateComponentMargins()
{
	int firstRow = -1;
	int lastRow = -1;
	for (int rowIdx = 0; rowIdx < workspaceSplitter->count(); ++rowIdx) {
		auto row = qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx));
		for (int columnIdx = 0; columnIdx < row->count(); ++columnIdx) {
			if (qobject_cast<ComponentContainer*>(row->widget(columnIdx))) {
				if (firstRow < 0) {
					firstRow = rowIdx;
				}

				lastRow = rowIdx;
				break;
			}
		}
	}

	for (int rowIdx = firstRow; rowIdx >= 0 && rowIdx <= lastRow; ++rowIdx) {
		auto row = qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx));
		int firstColumn = -1;
		int lastColumn = -1;
		for (int columnIdx = 0; columnIdx < row->count(); ++columnIdx) {
			if (qobject_cast<ComponentContainer*>(row->widget(columnIdx))) {
				if (firstColumn < 0) {
					firstColumn = columnIdx;
				}

				lastColumn = columnIdx;
			}
		}

		for (int columnIdx = firstColumn; columnIdx >= 0 && columnIdx <= lastColumn; ++columnIdx) {
			auto container = qobject_cast<ComponentContainer*>(row->widget(columnIdx));
			if (container) {
				container->setFrameMargins(
					columnIdx == firstColumn ? WorkspaceMargin : 0,
					rowIdx == firstRow ? WorkspaceMargin : 0,
					columnIdx == lastColumn ? WorkspaceMargin : 0,
					rowIdx == lastRow ? WorkspaceMargin : 0);
			}
		}
	}
}

QSplitter* Tab::CreateRow(int rowIdx)
{
	auto row = new QSplitter(Qt::Horizontal);
	row->setChildrenCollapsible(false);
	row->setHandleWidth(5);
	connect(row, &QSplitter::splitterMoved, this, [this] { Modify(); });
	workspaceSplitter->insertWidget(std::clamp(rowIdx, 0, workspaceSplitter->count()), row);
	InstallSplitterHandleFilters(workspaceSplitter);
	return row;
}

void Tab::InstallSplitterHandleFilters(QSplitter* splitter)
{
	for (int handleIdx = 1; handleIdx < splitter->count(); ++handleIdx) {
		splitter->handle(handleIdx)->installEventFilter(this);
	}
}

QWidget* Tab::GetLastRow() const
{
	if (workspaceSplitter->count() == 0) {
		return {};
	}

	return workspaceSplitter->widget(workspaceSplitter->count() - 1);
}

Tab::ComponentLocation Tab::GetComponentLocation(ComponentContainer* container) const
{
	auto row = container->parentWidget();
	auto rowSplitter = qobject_cast<QSplitter*>(row);
	return {workspaceSplitter->indexOf(row), rowSplitter ? rowSplitter->indexOf(container) : -1};
}

QJsonObject Tab::GetState() const
{
	QJsonObject state;
	QJsonArray arr;
	QJsonArray rowHeights;
	for (auto height : workspaceSplitter->sizes()) {
		rowHeights.append(height);
	}
	state["RowHeights"] = rowHeights;

	for (int rowIdx = 0; rowIdx < workspaceSplitter->count(); ++rowIdx) {
		auto row = qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx));
		auto columnWidths = row->sizes();
		for (int columnIdx = 0; columnIdx < row->count(); ++columnIdx) {
			auto container = qobject_cast<ComponentContainer*>(row->widget(columnIdx));
			if (container) {
				auto componentState = container->GetComponent()->SaveState();
				componentState["LayoutRow"] = rowIdx;
				componentState["LayoutWidth"] = columnWidths.value(columnIdx);
				arr.append(componentState);
			}
		}
	}
	state["Components"] = arr;
	return state;
}

QJsonObject Tab::SaveState()
{
	auto state = GetState();
	MarkSaved();
	return state;
}

void Tab::MarkSaved()
{
	emit TabSaved(this);
	modified = false;
}

void Tab::MarkModified()
{
	modified = true;
}

void Tab::LoadState(const QJsonObject& state)
{
	auto componentArr = state["Components"].toArray();
	for (auto componentRef : componentArr) {
		auto componentObj = componentRef.toObject();
		verticalSplit = true;
		if (const auto componentType = ToolTypeFromName(componentObj["Type"].toString())) {
			emit LoadComponent(*componentType, componentObj);
		}
	}

	QList<QPair<ComponentContainer*, int>> componentLayout;
	QHash<ComponentContainer*, int> componentWidths;
	auto components = findChildren<Component*>();
	int fallbackRow = 0;
	for (auto componentRef : componentArr) {
		auto componentObj = componentRef.toObject();
		for (auto component : components) {
			if (component->GetId() == componentObj["Id"].toString()) {
				int row = componentObj.contains("LayoutRow") ? componentObj["LayoutRow"].toInt() : fallbackRow;
				componentLayout.append({component->GetContainer(), row});
				componentWidths[component->GetContainer()] = componentObj["LayoutWidth"].toInt();
				break;
			}
		}

		++fallbackRow;
	}

	for (const auto& entry : componentLayout) {
		DetachComponent(entry.first);
	}

	int restoredRow = -1;
	int savedRow = -1;
	int column = 0;
	for (const auto& [container, row] : componentLayout) {
		bool createRow = row != savedRow;
		if (createRow) {
			++restoredRow;
			column = 0;
		}

		AttachComponent(container, {restoredRow, column}, createRow);
		savedRow = row;
		++column;
	}

	for (int rowIdx = 0; rowIdx < workspaceSplitter->count(); ++rowIdx) {
		auto row = qobject_cast<QSplitter*>(workspaceSplitter->widget(rowIdx));
		QList<int> widths;
		for (int columnIdx = 0; columnIdx < row->count(); ++columnIdx) {
			auto container = qobject_cast<ComponentContainer*>(row->widget(columnIdx));
			widths.append(componentWidths.value(container));
		}

		if (std::ranges::all_of(widths, [](int width) { return width > 0; })) {
			row->setSizes(widths);
		}
	}

	QList<int> rowHeights;
	for (auto height : state["RowHeights"].toArray()) {
		rowHeights.append(height.toInt());
	}
	if (rowHeights.size() == workspaceSplitter->count()
		&& std::ranges::all_of(rowHeights, [](int height) { return height > 0; })) {
		workspaceSplitter->setSizes(rowHeights);
	}
	verticalSplit = true;

	emit TabSaved(this);
	modified = false;
}

bool Tab::IsModified()
{
	return modified;
}

bool Tab::IsVerticalSplit() const
{
	return verticalSplit;
}

void Tab::SetVerticalSplit(bool vertical)
{
	verticalSplit = vertical;
}

void Tab::Modify()
{
	modified = true;
	emit TabModified(this, nullptr, false);
}
