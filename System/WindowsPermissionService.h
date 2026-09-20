#pragma once

#include <QString>

class WindowsPermissionService
{
public:
	enum class ElevationResult {
		AlreadyElevated,
		ElevationRequested,
		Cancelled,
		Denied
	};

	static bool IsElevated();
	static ElevationResult requestElevation(const QString& actionDescription = {});
};
