#include "Calculator.h"
#include "JSON/Serialization.h"

#include <tinyexpr/tinyexpr.h>

Calculator::Calculator(QWidget *parent)
	: Component(parent, ToolType::Calculator)
{
	ui.setupUi(this);
}

Calculator::~Calculator()
{}

void Calculator::OnPopOut()
{
	ui.InputField->setFocus();
}

QJsonObject Calculator::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.InputField, text);
	    HX_SERIALIZE(ui.OutputField, text);
	)
	return state;
}

void Calculator::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.InputField, setText, toString);
	    HX_DESERIALIZE(ui.OutputField, setText, toString);
	)
}

void Calculator::InputTextChanged(QString text)
{
	int error{};
	auto result = te_interp(text.toStdString().c_str(), &error);
	auto resultStr = !error ? QString::number(result, 'f', 12) : QString("");
	for (int64_t i = resultStr.size() - 1; i >= 0; --i) {
		if (resultStr[i] != '0') {
			break;
		}
		resultStr.removeAt(i);
	}

	if (!resultStr.isEmpty()) {
		auto lastIdx = resultStr.size() - 1;
		if (resultStr[lastIdx] == '.') {
			resultStr.removeAt(lastIdx);
		}
	}

	ui.OutputField->setText(resultStr);
	emit Modified(this);
}
