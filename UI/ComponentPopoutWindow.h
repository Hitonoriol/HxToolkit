#pragma once

#include <QWidget>

class ComponentPopoutWindow : public QWidget
{
public:
	ComponentPopoutWindow(QWidget* parent = nullptr);

protected:
	bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
	static constexpr int ResizeMargin = 6;
};
