#include "WindowsPermissionService.h"

#include <QCoreApplication>
#include <QAbstractButton>
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>

#include <windows.h>
#include <shellapi.h>

namespace {
QString QuoteWindowsArgument(const QString& argument)
{
	QString result(QLatin1Char('"'));
	int backslashCount = 0;
	for (const auto character : argument) {
		if (character == '\\') {
			++backslashCount;
			continue;
		}
		if (character == '"') result.append(QString(backslashCount * 2 + 1, '\\'));
		else result.append(QString(backslashCount, '\\'));
		backslashCount = 0;
		result.append(character);
	}
	result.append(QString(backslashCount * 2, '\\'));
	return result + '"';
}
}

bool WindowsPermissionService::IsElevated()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
	TOKEN_ELEVATION elevation{};
	DWORD size = 0;
	const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
	CloseHandle(token);
	return elevated;
}

WindowsPermissionService::ElevationResult WindowsPermissionService::requestElevation(const QString& actionDescription)
{
	if (IsElevated()) return ElevationResult::AlreadyElevated;
	QMessageBox confirmation(QApplication::activeWindow());
	confirmation.setIcon(QMessageBox::Information);
	confirmation.setWindowTitle("Administrator permission required");
	confirmation.setWindowModality(Qt::ApplicationModal);
	confirmation.setWindowFlag(Qt::WindowStaysOnTopHint);
	confirmation.setText(actionDescription.isEmpty()
		? "This operation requires administrator permission."
		: QString("%1 requires administrator permission.").arg(actionDescription));
	confirmation.setInformativeText("HxNxToolkit will restart as administrator. The current window will close after you approve the Windows UAC prompt.");
	confirmation.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
	confirmation.setDefaultButton(QMessageBox::Ok);
	confirmation.button(QMessageBox::Ok)->setText("Restart as administrator");
	if (confirmation.exec() != QMessageBox::Ok) return ElevationResult::Cancelled;

	QStringList quotedArguments;
	for (const auto& argument : QCoreApplication::arguments().sliced(1)) quotedArguments.append(QuoteWindowsArgument(argument));
	const auto executable = QCoreApplication::applicationFilePath();
	const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
		nullptr,
		L"runas",
		reinterpret_cast<LPCWSTR>(executable.utf16()),
		reinterpret_cast<LPCWSTR>(quotedArguments.join(' ').utf16()),
		reinterpret_cast<LPCWSTR>(QFileInfo(executable).absolutePath().utf16()),
		SW_SHOWNORMAL));
	if (result <= 32) return ElevationResult::Denied;
	QTimer::singleShot(0, [] { QCoreApplication::exit(0); });
	return ElevationResult::ElevationRequested;
}
