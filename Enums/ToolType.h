#pragma once

enum class ToolType
{
	None = 0,

	// General
	BaseConverter = 1,
	Calculator = 2,
	MarkdownEditor = 10,
	ColorPicker = 18,

	// Productivity
	Checklist = 3,
	TaskTracker = 4,
	GitlabTasks = 13,
	GitlabMergeRequests = 14,
	GitlabTimeStats = 15,

	// Time
	Stopwatch = 5,
	Timer = 6,
	DateCountdown = 17,

	// Random
	RandomNumber = 7,
	RandomString = 8,

	// Filesystem
	FileSearch = 9,
	SymlinkMover = 19,

	// Network
	Ping = 20,

	// System
	RamMonitor = 11,
	ClipboardManager = 12,
	SystemShortcuts = 16
};
