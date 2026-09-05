#include "RandomNumber.h"
#include "JSON/Serialization.h"

#include <QMetaEnum>
#include <cfloat>

RandomNumber::RandomNumber(QWidget *parent)
	: Component(parent, ToolType::RandomNumber)
{
	ui.setupUi(this);
	ui.MinIntBox->setRange(INT_MIN, INT_MAX);
	ui.MaxIntBox->setRange(INT_MIN, INT_MAX);
	ui.MinDoubleBox->setRange(DBL_MIN, DBL_MAX);
	ui.MaxDoubleBox->setRange(DBL_MIN, DBL_MAX);

	auto numberEnum = QMetaEnum::fromType<RandomNumber::NumberType>();
	for (size_t i = 0; i < numberEnum.keyCount(); ++i) {
		ui.NumberTypeBox->addItem(numberEnum.key(i));
	}

	OnNumberTypeBoxChange(0);
}

RandomNumber::~RandomNumber()
{}

QJsonObject RandomNumber::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
	    HX_SERIALIZE(ui.MinIntBox, value);
	    HX_SERIALIZE(ui.MaxIntBox, value);
		HX_SERIALIZE(ui.MinDoubleBox, value);
		HX_SERIALIZE(ui.MaxDoubleBox, value);
		HX_SERIALIZE(ui.NumberCountBox, value);
		HX_SERIALIZE(ui.NumberTypeBox, currentIndex);
		HX_SERIALIZE(ui.OutputField, toPlainText);
	)
	return state;
}

void RandomNumber::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
	    HX_DESERIALIZE(ui.MinIntBox, setValue, toInt);
	    HX_DESERIALIZE(ui.MaxIntBox, setValue, toInt);
	    HX_DESERIALIZE(ui.MinDoubleBox, setValue, toDouble);
	    HX_DESERIALIZE(ui.MaxDoubleBox, setValue, toDouble);
	    HX_DESERIALIZE(ui.NumberCountBox, setValue, toInt);
	    HX_DESERIALIZE(ui.NumberTypeBox, setCurrentIndex, toInt);
	    HX_DESERIALIZE(ui.OutputField, setPlainText, toString);
	)
}

RandomNumber::NumberType RandomNumber::GetSelectedNumberType()
{
	return static_cast<RandomNumber::NumberType>(ui.NumberTypeBox->currentIndex());
}

void RandomNumber::OnNumberTypeBoxChange(int newIndex)
{
	auto type = static_cast<RandomNumber::NumberType>(newIndex);
	switch (type) {
	case RandomNumber::NumberType::Integer:
		ui.DoubleWidget->setVisible(false);
		ui.IntegerWidget->setVisible(true);
		break;

	case RandomNumber::NumberType::Double:
		ui.DoubleWidget->setVisible(true);
		ui.IntegerWidget->setVisible(false);
		break;

	default:
		break;
	}

	emit Modified(this);
}

void RandomNumber::OnSpinBoxChange()
{
	emit Modified(this);
}

void RandomNumber::OnGenerateButtonPress()
{
	ui.OutputField->clear();

	auto nums = ui.NumberCountBox->value();
	switch (GetSelectedNumberType()) {
	case RandomNumber::NumberType::Integer:
	{
		auto min = ui.MinIntBox->value();
		auto max = ui.MaxIntBox->value();
		if (min >= max) {
			return;
		}

		std::uniform_int_distribution distr(min, max);
		for (size_t i = 0; i < nums; ++i) {
			auto num = distr(rng);
			ui.OutputField->appendPlainText(QString::number(num));
		}
		break;
	}

	case RandomNumber::NumberType::Double:
	{
		auto min = ui.MinDoubleBox->value();
		auto max = ui.MaxDoubleBox->value();
		if (min >= max) {
			return;
		}

		std::uniform_real_distribution distr(min, max);
		for (size_t i = 0; i < nums; ++i) {
			auto num = distr(rng);
			ui.OutputField->appendPlainText(QString::number(num));
		}
		break;
	}

	default:
		break;
	}

	emit Modified(this);
}
