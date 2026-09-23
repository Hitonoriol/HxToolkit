#pragma once

#include <QMetaEnum>
#include <QString>

#include <optional>

class ToolTypeMeta
{
	Q_GADGET

public:
	enum class Value
	{
		None = 0,
		BaseConverter = 1, Calculator = 2, MarkdownEditor = 10, ColorPicker = 18,
		Checklist = 3, TaskTracker = 4, GitlabTasks = 13, GitlabMergeRequests = 14, GitlabTimeStats = 15,
		Stopwatch = 5, Timer = 6, DateCountdown = 17,
		RandomNumber = 7, RandomString = 8,
		FileSearch = 9, SymlinkMover = 19,
		Ping = 20, NetworkInterfaces = 22,
		MeltingScreen = 21,
		RamMonitor = 11, ClipboardManager = 12, SystemShortcuts = 16,
		FocusTracker = 23
	};
	Q_ENUM(Value)
};

using ToolType = ToolTypeMeta::Value;

inline QString ToolTypeName(ToolType type)
{
	const auto metaEnum = QMetaEnum::fromType<ToolType>();
	const auto* key = metaEnum.valueToKey(static_cast<int>(type));
	return key ? QString::fromLatin1(key) : QString{};
}

inline std::optional<ToolType> ToolTypeFromName(const QString& name)
{
	const auto value = QMetaEnum::fromType<ToolType>().keyToValue(name.toLatin1().constData());
	if (value < 0) return std::nullopt;
	return static_cast<ToolType>(value);
}
