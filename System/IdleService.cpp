#include "IdleService.h"

#include <chrono>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

IdleService::IdleService(QObject* parent)
	: QObject(parent)
{
	connect(&timer, &QTimer::timeout, this, &IdleService::CheckIdleTime);
	timer.start(std::chrono::seconds{1});
	CheckIdleTime();
}

void IdleService::CheckIdleTime()
{
#ifdef Q_OS_WIN
	LASTINPUTINFO inputInfo{};
	inputInfo.cbSize = sizeof(LASTINPUTINFO);
	if (!GetLastInputInfo(&inputInfo)) {
		return;
	}

	const DWORD idleMilliseconds = GetTickCount() - inputInfo.dwTime;
	emit idleTime(static_cast<uint64_t>(idleMilliseconds) / 1'000);
#else
	emit idleTime(0);
#endif
}
