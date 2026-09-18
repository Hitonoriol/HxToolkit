#pragma once

#include <QColor>

#include "UI/Component.h"
#include "ui_ColorPicker.h"

class ColorPicker : public Component
{
	Q_OBJECT

public:
	ColorPicker(QWidget* parent = nullptr);
	~ColorPicker();

	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

private slots:
	void PickScreenColor();

private:
	void SetColor(const QColor& color);

	QColor color{Qt::black};
	Ui::ColorPickerClass ui;
};
