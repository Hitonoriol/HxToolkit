#include "WindowBox.h"

#include "UI/ToolConfigurationParameter.h"
#include "System/WindowsHelper.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

WindowBox::WindowBox(QWidget* parent)
	: Component(parent, ToolType::WindowBox)
	, windowCombo(new QComboBox(this))
	, boxButton(new QPushButton("Box", this))
	, picker(new QWidget(this))
	, unboxAction(new ToolConfigurationParameter("Unbox", [this](QWidget*) { return Unbox(); }, this))
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto pickerLayout = new QHBoxLayout(picker);
	pickerLayout->setContentsMargins(0, 0, 0, 0);
	pickerLayout->addWidget(windowCombo, 1);
	pickerLayout->addWidget(boxButton);
	layout->addWidget(picker);
	layout->addStretch();

	windowCombo->installEventFilter(this);
	connect(boxButton, &QPushButton::clicked, this, &WindowBox::BoxSelectedWindow);

	auto windowTimer = new QTimer(this);
	windowTimer->setInterval(500);
	connect(windowTimer, &QTimer::timeout, this, [this] {
		if (boxedWindow && !IsWindow(boxedWindow)) {
			boxedWindow = {};
			Unbox();
		}
	});
	windowTimer->start();

	RefreshWindows();
}

WindowBox::~WindowBox()
{
	Unbox();
}

QList<ToolConfigurationParameter*> WindowBox::GetConfigurationParameters() const
{
	return {unboxAction};
}

bool WindowBox::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == windowCombo && event->type() == QEvent::MouseButtonPress) {
		RefreshWindows();
	}

	return Component::eventFilter(watched, event);
}

void WindowBox::RefreshWindows()
{
	const auto selectedWindow = windowCombo->currentData().toULongLong();
	windowCombo->clear();
	for (const auto& window : WindowsHelper::GetWindows()) {
		windowCombo->addItem(window.Icon, window.Title,
			QVariant::fromValue(reinterpret_cast<qulonglong>(window.Handle)));
	}

	const auto selectedIdx = windowCombo->findData(selectedWindow);
	if (selectedIdx >= 0) {
		windowCombo->setCurrentIndex(selectedIdx);
	}

	boxButton->setEnabled(windowCombo->count() > 0);
}

void WindowBox::BoxSelectedWindow()
{
	auto window = reinterpret_cast<HWND>(windowCombo->currentData().toULongLong());
	if (!WindowsHelper::IsSelectableWindow(window)) {
		RefreshWindows();
		return;
	}

	WINDOWPLACEMENT placement{};
	placement.length = sizeof(placement);
	GetWindowPlacement(window, &placement);

	boxedWindow = window;
	originalParent = GetParent(window);
	originalStyle = GetWindowLongPtrW(window, GWL_STYLE);
	originalExtendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
	originalGeometry = QRect(placement.rcNormalPosition.left, placement.rcNormalPosition.top,
		placement.rcNormalPosition.right - placement.rcNormalPosition.left,
		placement.rcNormalPosition.bottom - placement.rcNormalPosition.top);
	originalShowCommand = placement.showCmd;

	ShowWindow(window, SW_RESTORE);
	foreignWindow = QWindow::fromWinId(reinterpret_cast<WId>(window));
	windowContainer = QWidget::createWindowContainer(foreignWindow, this);
	delete layout()->takeAt(1);
	layout()->addWidget(windowContainer);
	picker->hide();
	QTimer::singleShot(0, this, &WindowBox::UpdateBoxedWindowSize);
}

void WindowBox::UpdateBoxedWindowSize()
{
	if (!boxedWindow || !IsWindow(boxedWindow)) {
		return;
	}

	RECT clientRect{};
	if (!GetClientRect(boxedWindow, &clientRect)) {
		return;
	}

	DWORD_PTR result{};
	SendMessageTimeoutW(boxedWindow, WM_SIZE, SIZE_RESTORED,
		MAKELPARAM(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top),
		SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result);
}

bool WindowBox::Unbox()
{
	if (!windowContainer) {
		return false;
	}

	auto container = windowContainer.data();
	auto window = foreignWindow.data();
	if (foreignWindow) {
		foreignWindow->setParent(nullptr);
	}

	if (boxedWindow && IsWindow(boxedWindow)) {
		SetWindowLongPtrW(boxedWindow, GWL_STYLE, originalStyle);
		SetWindowLongPtrW(boxedWindow, GWL_EXSTYLE, originalExtendedStyle);
		SetParent(boxedWindow, originalParent);

		WINDOWPLACEMENT placement{};
		placement.length = sizeof(placement);
		placement.showCmd = originalShowCommand;
		placement.rcNormalPosition = {
			originalGeometry.left(), originalGeometry.top(),
			originalGeometry.right() + 1, originalGeometry.bottom() + 1
		};
		SetWindowPlacement(boxedWindow, &placement);
		SetWindowPos(boxedWindow, nullptr, 0, 0, 0, 0,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		ShowWindow(boxedWindow, originalShowCommand);
	}

	boxedWindow = {};
	foreignWindow = {};
	windowContainer = {};
	layout()->removeWidget(container);
	delete container;
	delete window;
	picker->show();
	static_cast<QVBoxLayout*>(layout())->addStretch();
	RefreshWindows();
	return true;
}
