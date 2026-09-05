#pragma once

enum class ToolType
{
	None = 0,

	// General
	BaseConverter = 1,
	Calculator = 2,
	MarkdownEditor = 10,

	// Productivity
	Checklist = 3,
	TaskTracker = 4,

	// Time
	Stopwatch = 5,
	Timer = 6,

	// Random
	RandomNumber = 7,
	RandomString = 8,

	// Filesystem
	FileSearch = 9,
	SymlinkMover = 13,

	// System
	RamMonitor = 11,
	ClipboardManager = 12
};