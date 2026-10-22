#pragma once

#include "UI/Component.h"

#include <QPointer>
#include <QRect>
#include <qt_windows.h>

class QComboBox;
class QEvent;
class QPushButton;
class QWindow;
class ToolConfigurationParameter;

class WindowBox : public Component
{
	Q_OBJECT

public:
	explicit WindowBox(QWidget* parent = nullptr);
	~WindowBox() override;

	QList<ToolConfigurationParameter*> GetConfigurationParameters() const override;

private slots:
	void BoxSelectedWindow();
	void UpdateBoxedWindowSize();

private:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void RefreshWindows();
	bool Unbox();

	QComboBox* windowCombo;
	QPushButton* boxButton;
	QWidget* picker;
	QPointer<QWidget> windowContainer;
	QPointer<QWindow> foreignWindow;
	ToolConfigurationParameter* unboxAction;
	HWND boxedWindow{};
	HWND originalParent{};
	LONG_PTR originalStyle{};
	LONG_PTR originalExtendedStyle{};
	QRect originalGeometry;
	int originalShowCommand{};
};
