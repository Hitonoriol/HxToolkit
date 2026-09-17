#include "ComponentContainer.h"

#include <QMenu>
#include <QInputDialog>
#include <QIcon>
#include <QToolButton>

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

	connect(ui.CloseBtn, &QToolButton::clicked, this, &ComponentContainer::CloseClicked);
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

void ComponentContainer::OnCollapseClicked()
{
	const bool collapse = !ui.Content->isHidden();
	ui.Content->setVisible(!collapse);
	UpdateCollapseButton();
	emit CollapseClicked(collapse);
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

	menu.exec(ui.Title->mapToGlobal(pos));
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
