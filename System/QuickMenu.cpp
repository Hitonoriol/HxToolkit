#include "QuickMenu.h"

#include "App/Workspace.h"
#include "HxNxToolkit.h"
#include "System/KeystrokeEditorDialog.h"
#include "UI/ComponentContainer.h"
#include "UI/ComponentFactory.h"
#include "UI/ToolConfigurationParameter.h"

#ifdef Q_OS_WIN
#include "System/WindowsHelper.h"
#include <qt_windows.h>
#endif

#include <QCursor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>
#include <algorithm>

QuickMenu::QuickMenu(Workspace* workspace, QWidget* parent)
	: Component(parent, ToolType::QuickMenu)
	, workspace(workspace)
	, shortcutConfiguration(new ToolConfigurationParameter("Shortcut", [this](QWidget* parent) { return EditShortcut(parent); }, this))
{
	auto layout = new QVBoxLayout(this);
	auto description = new QLabel("Choose which workspace components appear in the quick menu.", this);
	description->setWordWrap(true);
	layout->addWidget(description);
	componentTable = new QTableWidget(this);
	componentTable->setColumnCount(1);
	componentTable->setHorizontalHeaderLabels({"Component"});
	componentTable->horizontalHeader()->setStretchLastSection(true);
	componentTable->verticalHeader()->hide();
	componentTable->setSelectionMode(QAbstractItemView::NoSelection);
	componentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	layout->addWidget(componentTable);

	popup = new QWidget(nullptr);
	popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	auto popupLayout = new QVBoxLayout(popup);
	popupLayout->setContentsMargins(0, 0, 0, 0);
	popupLayout->setSpacing(4);
	searchField = new QLineEdit(popup);
	searchField->setPlaceholderText("Search components...");
	popupLayout->addWidget(searchField);
	popupList = new QListWidget(popup);
	popupList->setIconSize({20, 20});
	popupLayout->addWidget(popupList);
	searchField->installEventFilter(this);
	popupList->installEventFilter(this);

	connect(componentTable, &QTableWidget::itemChanged, this, &QuickMenu::ComponentCheckStateChanged);
	connect(workspace, &Workspace::ComponentsChanged, this, &QuickMenu::RefreshComponents);
	connect(searchField, &QLineEdit::textChanged, this, &QuickMenu::FilterItems);
	connect(popupList, &QListWidget::itemActivated, this, &QuickMenu::ActivateItem);
	RefreshComponents();
}

QuickMenu::~QuickMenu()
{
	if (auto listener = KeystrokeListener::Get()) {
		listener->UnregisterShortcut(shortcutId);
	}

	delete popup;
}

QJsonObject QuickMenu::SaveState()
{
	auto state = Component::SaveState();
	QJsonArray strokes;
	for (const auto& stroke : chord) {
		QJsonArray keys;
		for (auto key : stroke) {
			keys.append(static_cast<int>(key));
		}

		strokes.append(keys);
	}

	state["Chord"] = strokes;

	QJsonArray exclusions;
	for (const auto& id : excludedComponentIds) {
		exclusions.append(id);
	}

	state["ExcludedComponents"] = exclusions;
	state["ShowToolkitExcluded"] = showToolkitExcluded;
	return state;
}

void QuickMenu::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	chord.clear();
	for (const auto& strokeValue : state["Chord"].toArray()) {
		KeystrokeListener::KeyStroke stroke;
		for (const auto& keyValue : strokeValue.toArray()) {
			stroke.insert(static_cast<Qt::Key>(keyValue.toInt()));
		}

		if (!stroke.isEmpty()) {
			chord.append(stroke);
		}
	}

	excludedComponentIds.clear();
	for (const auto& id : state["ExcludedComponents"].toArray()) {
		excludedComponentIds.insert(id.toString());
	}

	showToolkitExcluded = state["ShowToolkitExcluded"].toBool();
	RegisterShortcut();
	RefreshComponents();
}

QList<ToolConfigurationParameter*> QuickMenu::GetConfigurationParameters() const
{
	return {shortcutConfiguration};
}

bool QuickMenu::eventFilter(QObject* watched, QEvent* event)
{
	if ((watched == searchField || watched == popupList) && event->type() == QEvent::KeyPress) {
		auto keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Escape) {
			popup->hide();
			return true;
		}

		if (watched == searchField
			&& (keyEvent->key() == Qt::Key_Up
				|| keyEvent->key() == Qt::Key_Down
				|| keyEvent->key() == Qt::Key_Return
				|| keyEvent->key() == Qt::Key_Enter)) {
			QCoreApplication::sendEvent(popupList, event);
			return true;
		}
	}

	return Component::eventFilter(watched, event);
}

bool QuickMenu::EditShortcut(QWidget* parent)
{
	KeystrokeEditorDialog dialog(chord, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	auto newChord = dialog.GetChord();
	if (newChord == chord) {
		return false;
	}

	chord = newChord;
	RegisterShortcut();
	return true;
}

void QuickMenu::RefreshComponents()
{
	refreshingComponents = true;
	componentTable->setRowCount(0);
	auto showToolkitItem = new QTableWidgetItem(QIcon(":/icons/icon-app.ico"), "Show HxToolkit");
	showToolkitItem->setFlags(showToolkitItem->flags() | Qt::ItemIsUserCheckable);
	showToolkitItem->setCheckState(showToolkitExcluded ? Qt::Unchecked : Qt::Checked);
	componentTable->insertRow(0);
	componentTable->setItem(0, 0, showToolkitItem);

	for (auto component : workspace->GetComponents()) {
		auto tool = ComponentFactory::GetToolInfo(component->GetType());
		QString title = component->GetTitle();
		if (title.isEmpty() && tool) {
			title = tool->ToolName;
		}

		auto row = componentTable->rowCount();
		componentTable->insertRow(row);
		auto item = new QTableWidgetItem(tool ? QIcon(tool->IconPath) : QIcon{}, title);
		item->setData(Qt::UserRole, component->GetId());
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(excludedComponentIds.contains(component->GetId()) ? Qt::Unchecked : Qt::Checked);
		componentTable->setItem(row, 0, item);
	}

	refreshingComponents = false;
}

void QuickMenu::ComponentCheckStateChanged(QTableWidgetItem* item)
{
	if (refreshingComponents || !item) {
		return;
	}

	auto id = item->data(Qt::UserRole).toString();
	if (id.isEmpty()) {
		showToolkitExcluded = item->checkState() != Qt::Checked;
		emit Modified(this);
		return;
	}

	if (item->checkState() == Qt::Checked) {
		excludedComponentIds.remove(id);
	} else {
		excludedComponentIds.insert(id);
	}

	emit Modified(this);
}

void QuickMenu::FilterItems(const QString& query)
{
	int firstVisibleRow = -1;
	for (int row = 0; row < popupList->count(); ++row) {
		auto item = popupList->item(row);
		bool visible = item->text().contains(query, Qt::CaseInsensitive);
		item->setHidden(!visible);
		if (visible && firstVisibleRow < 0) {
			firstVisibleRow = row;
		}
	}

	popupList->setCurrentRow(firstVisibleRow);
}

void QuickMenu::ShowPopup()
{
	if (!workspace->GetTab(this)) {
		return;
	}

	popupList->clear();
	if (!showToolkitExcluded) {
		auto item = new QListWidgetItem(QIcon(":/icons/icon-app.ico"), "Show HxToolkit", popupList);
		item->setData(Qt::UserRole, QString{});
	}

	for (auto component : workspace->GetComponents()) {
		if (excludedComponentIds.contains(component->GetId())) {
			continue;
		}

		auto tool = ComponentFactory::GetToolInfo(component->GetType());
		QString title = component->GetTitle();
		if (title.isEmpty() && tool) {
			title = tool->ToolName;
		}

		auto item = new QListWidgetItem(tool ? QIcon(tool->IconPath) : QIcon{}, title, popupList);
		item->setData(Qt::UserRole, component->GetId());
	}

	if (popupList->count() == 0) {
		return;
	}

	searchField->clear();
	popupList->setCurrentRow(0);
	int rowHeight = popupList->sizeHintForRow(0);
	if (rowHeight <= 0) {
		rowHeight = 30;
	}

	int popupHeight = searchField->sizeHint().height() + rowHeight * std::min(popupList->count(), 10)
		+ popupList->frameWidth() * 2 + 4;
	popup->setFixedSize(320, popupHeight);
	auto position = GetPopupPosition();
	popupAnchor = position;
	auto screen = QGuiApplication::screenAt(position);
	if (!screen) {
		screen = QGuiApplication::primaryScreen();
	}

	if (screen) {
		auto available = screen->availableGeometry();
		position.setX(std::clamp(position.x(), available.left(), available.right() - popup->width() + 1));
		if (position.y() + popup->height() > available.bottom() + 1) {
			position.setY(position.y() - popup->height());
		}
		position.setY(std::max(position.y(), available.top()));
	}

	popup->move(position);
	popup->show();
	popup->raise();
	popup->activateWindow();
	searchField->setFocus();
#ifdef Q_OS_WIN
	SetForegroundWindow(reinterpret_cast<HWND>(popup->winId()));
#endif
}

void QuickMenu::ActivateItem(QListWidgetItem* item)
{
	if (!item) {
		return;
	}

	auto id = item->data(Qt::UserRole).toString();
	popup->hide();
	if (id.isEmpty()) {
		auto toolkit = qobject_cast<HxNxToolkit*>(workspace->parent());
		if (toolkit) {
			toolkit->show();
			toolkit->setWindowState(toolkit->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
			toolkit->raise();
			toolkit->activateWindow();
		}

		return;
	}

	auto component = workspace->FindComponent(id);
	if (component && component->GetContainer()) {
		component->GetContainer()->ShowPopoutAt(popupAnchor);
	}
}

void QuickMenu::RegisterShortcut()
{
	auto listener = KeystrokeListener::Get();
	if (!listener) {
		return;
	}

	listener->UnregisterShortcut(shortcutId);
	shortcutId = chord.isEmpty() ? 0
		: listener->RegisterShortcut(chord, this, std::bind(&QuickMenu::ShowPopup, this));
}

QPoint QuickMenu::GetPopupPosition() const
{
#ifdef Q_OS_WIN
	if (auto position = WindowsHelper::GetTextCursorPosition()) {
		return *position;
	}
#endif

	return QCursor::pos();
}
