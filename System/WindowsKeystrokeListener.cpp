#include "System/WindowsKeystrokeListener.h"

#ifdef Q_OS_WIN

#include "System/KeystrokeListener.h"

#include <QCoreApplication>
#include <QKeyEvent>

WindowsKeystrokeListener* WindowsKeystrokeListener::instance{};

WindowsKeystrokeListener::WindowsKeystrokeListener(KeystrokeListener* listener, QCoreApplication* application)
	: QObject(application)
	, listener(listener)
{
	instance = this;
	hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(nullptr), 0);
}

WindowsKeystrokeListener::~WindowsKeystrokeListener()
{
	if (hook) {
		UnhookWindowsHookEx(hook);
	}

	if (instance == this) {
		instance = nullptr;
	}
}

bool WindowsKeystrokeListener::IsInstalled() const
{
	return hook != nullptr;
}

bool WindowsKeystrokeListener::event(QEvent* event)
{
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		listener->eventFilter(listener, event);
		return true;
	}

	return QObject::event(event);
}

LRESULT CALLBACK WindowsKeystrokeListener::KeyboardProc(int code, WPARAM message, LPARAM data)
{
	if (code < 0 || !instance) {
		return CallNextHookEx(nullptr, code, message, data);
	}

	QEvent::Type eventType;
	switch (message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		eventType = QEvent::KeyPress;
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		eventType = QEvent::KeyRelease;
		break;

	default:
		return CallNextHookEx(nullptr, code, message, data);
	}

	auto keyData = reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
	auto key = GetQtKey(keyData->vkCode);
	if (key == Qt::Key_unknown) {
		return CallNextHookEx(nullptr, code, message, data);
	}

	auto modifiers = GetModifiers();
	if (keyData->vkCode >= VK_NUMPAD0 && keyData->vkCode <= VK_DIVIDE) {
		modifiers |= Qt::KeypadModifier;
	}

	QCoreApplication::postEvent(instance, new QKeyEvent(eventType, key, modifiers));
	return CallNextHookEx(nullptr, code, message, data);
}

Qt::Key WindowsKeystrokeListener::GetQtKey(DWORD virtualKey)
{
	if ((virtualKey >= '0' && virtualKey <= '9') || (virtualKey >= 'A' && virtualKey <= 'Z')) {
		return static_cast<Qt::Key>(virtualKey);
	}

	if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
		return static_cast<Qt::Key>(Qt::Key_F1 + virtualKey - VK_F1);
	}

	switch (virtualKey) {
	case VK_BACK: return Qt::Key_Backspace;
	case VK_TAB: return Qt::Key_Tab;
	case VK_CLEAR: return Qt::Key_Clear;
	case VK_RETURN: return Qt::Key_Return;
	case VK_SHIFT:
	case VK_LSHIFT:
	case VK_RSHIFT: return Qt::Key_Shift;
	case VK_CONTROL:
	case VK_LCONTROL:
	case VK_RCONTROL: return Qt::Key_Control;
	case VK_MENU:
	case VK_LMENU:
	case VK_RMENU: return Qt::Key_Alt;
	case VK_PAUSE: return Qt::Key_Pause;
	case VK_CAPITAL: return Qt::Key_CapsLock;
	case VK_ESCAPE: return Qt::Key_Escape;
	case VK_SPACE: return Qt::Key_Space;
	case VK_PRIOR: return Qt::Key_PageUp;
	case VK_NEXT: return Qt::Key_PageDown;
	case VK_END: return Qt::Key_End;
	case VK_HOME: return Qt::Key_Home;
	case VK_LEFT: return Qt::Key_Left;
	case VK_UP: return Qt::Key_Up;
	case VK_RIGHT: return Qt::Key_Right;
	case VK_DOWN: return Qt::Key_Down;
	case VK_SNAPSHOT: return Qt::Key_Print;
	case VK_INSERT: return Qt::Key_Insert;
	case VK_DELETE: return Qt::Key_Delete;
	case VK_LWIN:
	case VK_RWIN: return Qt::Key_Meta;
	case VK_NUMPAD0: return Qt::Key_0;
	case VK_NUMPAD1: return Qt::Key_1;
	case VK_NUMPAD2: return Qt::Key_2;
	case VK_NUMPAD3: return Qt::Key_3;
	case VK_NUMPAD4: return Qt::Key_4;
	case VK_NUMPAD5: return Qt::Key_5;
	case VK_NUMPAD6: return Qt::Key_6;
	case VK_NUMPAD7: return Qt::Key_7;
	case VK_NUMPAD8: return Qt::Key_8;
	case VK_NUMPAD9: return Qt::Key_9;
	case VK_NUMLOCK: return Qt::Key_NumLock;
	case VK_SCROLL: return Qt::Key_ScrollLock;
	case VK_MULTIPLY: return Qt::Key_Asterisk;
	case VK_ADD: return Qt::Key_Plus;
	case VK_SUBTRACT: return Qt::Key_Minus;
	case VK_DECIMAL: return Qt::Key_Period;
	case VK_DIVIDE: return Qt::Key_Slash;
	default: return Qt::Key_unknown;
	}
}

Qt::KeyboardModifiers WindowsKeystrokeListener::GetModifiers()
{
	Qt::KeyboardModifiers modifiers;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		modifiers |= Qt::ShiftModifier;
	}

	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
		modifiers |= Qt::ControlModifier;
	}

	if (GetAsyncKeyState(VK_MENU) & 0x8000) {
		modifiers |= Qt::AltModifier;
	}

	if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) {
		modifiers |= Qt::MetaModifier;
	}

	return modifiers;
}

#endif
