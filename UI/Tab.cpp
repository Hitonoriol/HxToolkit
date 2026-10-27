#include "Tab.h"
#include "ComponentContainer.h"

#include <QVBoxLayout>
#include <QJsonArray>

#include <algorithm>

Tab::Tab(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	ui.RootLayout->setAlignment(ui.ToolbarLayout, Qt::AlignTop);

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
	auto row = GetLastRow();

	if (!row || verticalSplit) {
		row = new QWidget(this);
		auto rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(0);
		ui.WorkspaceLayout->addWidget(row, 1);
	}

	auto rowLayout = dynamic_cast<QBoxLayout*>(row->layout());
	assert(rowLayout);
	rowLayout->addWidget(container, 1);
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
	});

	emit ComponentAdded(this, container, GetComponentLocation(container));
}

void Tab::OnComponentClosed(ComponentContainer* container)
{
	auto location = DetachComponent(container);
	emit ComponentRemoved(this, container, location);
}

void Tab::AttachComponent(ComponentContainer* container, const ComponentLocation& location)
{
	auto rowIdx = std::clamp(location.Row, 0, ui.WorkspaceLayout->count());
	auto row = rowIdx < ui.WorkspaceLayout->count() ? ui.WorkspaceLayout->itemAt(rowIdx)->widget() : nullptr;

	if (!row) {
		row = new QWidget(this);
		auto rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(0);
		ui.WorkspaceLayout->insertWidget(rowIdx, row, 1);
	}

	auto rowLayout = dynamic_cast<QBoxLayout*>(row->layout());
	assert(rowLayout);
	rowLayout->insertWidget(std::clamp(location.Column, 0, rowLayout->count()), container, 1);
	UpdateRowStretch(row);
}

Tab::ComponentLocation Tab::DetachComponent(ComponentContainer* container)
{
	auto location = GetComponentLocation(container);
	auto row = container->parentWidget();
	row->layout()->removeWidget(container);
	container->setParent(nullptr);

	if (row->layout()->isEmpty()) {
		ui.WorkspaceLayout->removeWidget(row);
		row->deleteLater();
	} else {
		UpdateRowStretch(row);
	}

	return location;
}

void Tab::OnComponentCollapse(ComponentContainer* container, bool collapse)
{
	auto row = container->parentWidget();
	auto rowLayout = dynamic_cast<QBoxLayout*>(row->layout());
	assert(rowLayout);
	auto containerSizePolicy = container->sizePolicy();
	containerSizePolicy.setVerticalPolicy(collapse ? QSizePolicy::Maximum : QSizePolicy::Preferred);
	container->setSizePolicy(containerSizePolicy);
	rowLayout->setAlignment(container, collapse ? Qt::AlignTop : Qt::Alignment{});
	UpdateRowStretch(row);
}

void Tab::UpdateRowStretch(QWidget* row)
{
	auto rowLayout = dynamic_cast<QBoxLayout*>(row->layout());
	assert(rowLayout);
	bool hasExpandedComponent = false;

	for (int itemIdx = 0; itemIdx < rowLayout->count(); ++itemIdx) {
		auto container = dynamic_cast<ComponentContainer*>(rowLayout->itemAt(itemIdx)->widget());

		if (container && !container->getCollapsed()) {
			hasExpandedComponent = true;
			break;
		}
	}

	ui.WorkspaceLayout->setStretchFactor(row, hasExpandedComponent ? 1 : 0);
	auto rowSizePolicy = row->sizePolicy();
	rowSizePolicy.setVerticalPolicy(hasExpandedComponent ? QSizePolicy::Preferred : QSizePolicy::Maximum);
	row->setSizePolicy(rowSizePolicy);
	row->updateGeometry();
}

QWidget* Tab::GetLastRow() const
{
	if (ui.WorkspaceLayout->isEmpty()) {
		return {};
	}

	return ui.WorkspaceLayout->itemAt(ui.WorkspaceLayout->count() - 1)->widget();
}

Tab::ComponentLocation Tab::GetComponentLocation(ComponentContainer* container) const
{
	auto row = container->parentWidget();
	auto rowLayout = row ? row->layout() : nullptr;
	return {ui.WorkspaceLayout->indexOf(row), rowLayout ? rowLayout->indexOf(container) : -1};
}

QJsonObject Tab::GetState() const
{
	QJsonObject state;
	QJsonArray arr;
	auto components = findChildren<Component*>();
	for (auto& component : components) {
		arr.append(component->SaveState());
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
		if (const auto componentType = ToolTypeFromName(componentObj["Type"].toString())) {
			emit LoadComponent(*componentType, componentObj);
		}
	}

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
