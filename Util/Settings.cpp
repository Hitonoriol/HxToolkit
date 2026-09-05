#include "Settings.h"

#include <nameof/nameof.hpp>
#include <QStandardPaths>

QSettings& Settings::Instance()
{
	static auto configPath = GetPath() / "hxnxtk_config.ini";
	static QSettings instance(QString::fromStdWString(configPath.wstring()), QSettings::IniFormat);
	return instance;
}

std::filesystem::path Settings::GetPath()
{
	std::filesystem::path configDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toStdString());
	configDir /= "HxNxToolkit";

	if (!std::filesystem::exists(configDir)) {
		std::filesystem::create_directories(configDir);
	}

	return configDir;
}

bool Settings::Contains(Option key)
{
	return Instance().contains(nameof::nameof_enum(key));
}

void Settings::Set(Option key, const QVariant& value)
{
	Instance().setValue(nameof::nameof_enum(key), value);
	Instance().sync();
}

void Settings::SetDefault(Option key, const QVariant& value)
{
	auto& instance = Instance();
	auto name = nameof::nameof_enum(key);
	if (!instance.contains(name)) {
		instance.setValue(name, value);
	}
}

QString Settings::GetString(Option key)
{
	return Instance().value(nameof::nameof_enum(key), QString{}).toString();
}

int Settings::GetInt(Option key)
{
	return Instance().value(nameof::nameof_enum(key), 0).toInt();
}

bool Settings::GetBool(Option key)
{
	return Instance().value(nameof::nameof_enum(key), false).toBool();
}

QSize Settings::GetSize(Option key) {
	return Instance().value(nameof::nameof_enum(key), QSize{}).toSize();
}

QPoint Settings::GetPoint(Option key)
{
	return Instance().value(nameof::nameof_enum(key), QPoint{}).toPoint();
}

QByteArray Settings::GetByteArray(Option key)
{
	return Instance().value(nameof::nameof_enum(key), QByteArray{}).toByteArray();
}
