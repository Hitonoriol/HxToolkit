#include "Tab.h"
#include "ComponentContainer.h"

#include <QVBoxLayout>
#include <QJsonArray>

Tab::Tab(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	ui.RootLayout->setAlignment(ui.ToolbarLayout, Qt::AlignTop);

	connect(ui.AddToolBtn, &QToolButton::clicked, this, &Tab::AddToolRequested);
}

Tab::~Tab()
{}

void Tab::ComponentModified(Component* component)
{
	emit TabModified(this);
	modified = true;
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
		OnComponentCollapse(container, collapse);
	});

	emit ComponentModified(nullptr);
}

void Tab::OnComponentClosed(ComponentContainer* container)
{
	auto row = container->parentWidget();
	row->layout()->removeWidget(container);
	container->deleteLater();

	if (row->layout()->isEmpty()) {
		ui.WorkspaceLayout->removeWidget(row);
		row->deleteLater();
	} else {
		UpdateRowStretch(row);
	}

	Modify();
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

QJsonObject Tab::SaveState()
{
	QJsonObject state;
	QJsonArray arr;
	auto components = findChildren<Component*>();
	for (auto& component : components) {
		arr.append(component->SaveState());
	}
	state["Components"] = arr;

	emit TabSaved(this);
	modified = false;

	return state;
}

void Tab::LoadState(const QJsonObject& state)
{
	auto componentArr = state["Components"].toArray();
	for (auto componentRef : componentArr) {
		auto componentObj = componentRef.toObject();
		auto componentType = static_cast<ToolType>(componentObj["Type"].toInt());
		emit LoadComponent(componentType, componentObj);
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
	emit TabModified(this);
	modified = true;
}
