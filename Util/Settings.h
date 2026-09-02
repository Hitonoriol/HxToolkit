#pragma once
#include <QSettings>
#include <QSize>
#include <QPoint>
#include <QByteArray>
#include <filesystem>

enum class Option
{
	LastSaveDir,
	LastSavedTabPath,
	AlwaysOnTop,
	AutosaveInterval,
	HideWhenMinimized,
	HideWhenClosed,
	RestorePreviousSession,
	GitlabUrl,
	GitlabToken,

	// Main window settings
	WindowMaximized,
	WindowPos,
	WindowSize
};

class Settings
{
public:
	static bool Contains(Option key);

	static void Set(Option key, const QVariant& value);
	static void SetDefault(Option key, const QVariant& value);

	static QString GetString(Option key);
	static int GetInt(Option key);
	static bool GetBool(Option key);
	static QSize GetSize(Option key);
	static QPoint GetPoint(Option key);
	static QByteArray GetByteArray(Option key);

private:
	static QSettings& Instance();
	static std::filesystem::path GetPath();
};

