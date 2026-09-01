#include "BaseConverter.h"
#include "JSON/Serialization.h"

#include <QMetaEnum>
#include <string>
#include <sstream>
#include <windows.h>

BaseConverter::BaseConverter(QWidget *parent)
	: Component(parent, ToolType::BaseConverter)
{
	ui.setupUi(this);

	auto baseEnum = QMetaEnum::fromType<BaseConverter::Base>();
	for (size_t i = 0; i < baseEnum.keyCount(); ++i) {
		ui.InputBaseSelector->addItem(baseEnum.key(i));
		ui.OutputBaseSelector->addItem(baseEnum.key(i));
	}
}

BaseConverter::~BaseConverter()
{}

QJsonObject BaseConverter::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.InputBaseSelector, currentIndex);
	    HX_SERIALIZE(ui.OutputBaseSelector, currentIndex);
		HX_SERIALIZE(ui.InputField, text);
		HX_SERIALIZE(ui.OutputField, text);
	)
	return state;
}

void BaseConverter::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.InputBaseSelector, setCurrentIndex, toInt);
	    HX_DESERIALIZE(ui.OutputBaseSelector, setCurrentIndex, toInt);
	    HX_DESERIALIZE(ui.InputField, setText, toString);
	    HX_DESERIALIZE(ui.OutputField, setText, toString);
	)
}

void BaseConverter::InputTextChanged(QString text)
{
	auto baseEnum = QMetaEnum::fromType<BaseConverter::Base>();
	auto srcBase = static_cast<Base>(baseEnum.value(ui.InputBaseSelector->currentIndex()));
	auto dstBase = static_cast<Base>(baseEnum.value(ui.OutputBaseSelector->currentIndex()));

	auto result = Convert(ui.InputField->text(), srcBase, dstBase);
	ui.OutputField->setText(result);

	bool ok{};
	HRESULT hResult = result.toInt(&ok, static_cast<int>(dstBase));
	if (!ok) {
		ui.HROutputField->setText("");
		return;
	}

	wchar_t* message{};
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		hResult,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<wchar_t*>(&message),
		0,
		nullptr);

	auto errorText = message
		? QString::fromWCharArray(message).trimmed()
		: QString("HRESULT 0x%1").arg(static_cast<quint32>(hResult), 8, 16, QChar('0'));
	LocalFree(message);
	ui.HROutputField->setText(errorText);

	emit Modified(this);
}

void BaseConverter::InputBaseChanged(int)
{
	InputTextChanged(ui.InputField->text());
}

void BaseConverter::OutputBaseChanged(int)
{
	InputTextChanged(ui.InputField->text());
}

QString BaseConverter::Convert(const QString& value, Base src, Base dst)
{
	uint64_t in{};
	try {
		in = std::stoull(value.toStdString(), nullptr, static_cast<int>(src));
	}
	catch (...) {
		return {};
	}

	auto result = QString::number(in, static_cast<int>(dst));
	switch (dst) {
	case Base::Hex:
		result = "0x" + result;
		break;

	case Base::Bin:
		result = "0b" + result;
		break;
	}

	return result;
}
