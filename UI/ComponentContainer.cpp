#include "ComponentContainer.h"

#include <QMenu>
#include <QInputDialog>
#include <QIcon>
#include <QToolButton>

#include <oclero/qlementine/widgets/Expander.hpp>

ComponentContainer::ComponentContainer(Component* component, QWidget* parent)
	: QWidget(parent)
	, component(component)
{
	ui.setupUi(this);
	component->container = this;

	auto contentExpander = new oclero::qlementine::Expander(this);
	contentExpander->setExpanded(true);
	ui.RootLayout->replaceWidget(ui.Content, contentExpander);
	contentExpander->setContent(ui.Content);
	expander = contentExpander;

	ui.Content->layout()->addWidget(component);
	ui.Title->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	ui.UpBtn->setIcon(QIcon(":/icons/tool-move-up.svg"));
	ui.DownBtn->setIcon(QIcon(":/icons/tool-move-down.svg"));
	ui.CloseBtn->setIcon(QIcon(":/icons/tool-close.svg"));
	UpdateCollapseButton();

	connect(ui.CloseBtn, &QToolButton::clicked, this, &ComponentContainer::CloseClicked);
	connect(ui.UpBtn, &QToolButton::clicked, this, &ComponentContainer::UpClicked);
	connect(ui.DownBtn, &QToolButton::clicked, this, &ComponentContainer::DownClicked);
	connect(ui.CollapseBtn, &QToolButton::clicked, this, &ComponentContainer::OnCollapseClicked);
	connect(ui.Title, &QWidget::customContextMenuRequested, this, &ComponentContainer::OnTitleContextMenuRequested);
	connect(expander, &oclero::qlementine::Expander::expandedChanged, this, [this] {
		UpdateCollapseButton();
	});
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

void ComponentContainer::OnCollapseClicked()
{
	expander->toggleExpanded();
	emit CollapseClicked(!expander->expanded());
}

void ComponentContainer::UpdateCollapseButton()
{
	const bool expanded = expander->expanded();
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
