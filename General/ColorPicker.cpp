#include "ColorPicker.h"

#include "General/ScreenColorPicker.h"

#include <QPointer>

ColorPicker::ColorPicker(QWidget* parent)
	: Component(parent, ToolType::ColorPicker)
{
	ui.setupUi(this);
	ui.PipetteButton->setIcon(QIcon(":/icons/pipette.svg"));
	SetColor(color);
}

ColorPicker::~ColorPicker()
{}

QJsonObject ColorPicker::SaveState()
{
	auto state = Component::SaveState();
	state["Color"] = color.name(QColor::NameFormat::HexArgb);
	return state;
}

void ColorPicker::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	SetColor(QColor(state["Color"].toString()));
}

void ColorPicker::PickScreenColor()
{
	auto picker = QPointer<ColorPicker>(this);
	ScreenColorPicker::Pick([picker](const QColor& pickedColor) {
		if (picker && pickedColor.isValid()) {
			picker->SetColor(pickedColor);
		}
	});
}

void ColorPicker::SetColor(const QColor& newColor)
{
	if (!newColor.isValid()) {
		return;
	}

	color = newColor;
	ui.ColorPreview->setStyleSheet(QString("background-color: %1;").arg(color.name(QColor::NameFormat::HexArgb)));
	ui.RgbField->setText(QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue()));
	ui.HexField->setText(color.name(QColor::NameFormat::HexRgb).toUpper());
	ui.CssRgbaField->setText(QString("rgba(%1, %2, %3, %4)")
		.arg(color.red())
		.arg(color.green())
		.arg(color.blue())
		.arg(color.alphaF(), 0, 'f', 3));
	emit Modified(this);
}
