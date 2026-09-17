#include "Tab.h"
#include "ComponentContainer.h"

#include <QVBoxLayout>
#include <QJsonArray>
#include <QButtonGroup>

Tab::Tab(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	ui.RootLayout->setAlignment(ui.ToolbarLayout, Qt::AlignTop);

	auto splitButtonGroup = new QButtonGroup(this);
	splitButtonGroup->addButton(ui.HorizontalSplitBtn);
	splitButtonGroup->addButton(ui.VerticalSplitBtn);
	ui.VerticalSplitBtn->setChecked(true);
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

	if (!row || ui.VerticalSplitBtn->isChecked()) {
		row = new QWidget(this);
		auto rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(0);
		ui.WorkspaceLayout->addWidget(row, 1);
	}

	auto rowLayout = dynamic_cast<QBoxLayout*>(row->layout());
	assert(rowLayout);
	rowLayout->addWidget(container, 1);

	connect(component, &Component::Modified, this, &Tab::ComponentModified);
	connect(container, &ComponentContainer::CloseClicked, this, std::bind(&Tab::OnComponentClosed, this, container));

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
	}

	Modify();
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

void Tab::Modify()
{
	emit TabModified(this);
	modified = true;
}
