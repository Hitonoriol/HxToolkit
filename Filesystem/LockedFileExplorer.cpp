#include "LockedFileExplorer.h"

#include "JSON/Serialization.h"
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileIconProvider>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QHash>
#include <QSet>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include <qt_windows.h>
#include <RestartManager.h>
#include <winternl.h>

#include <vector>

LockedFileExplorer::LockedFileExplorer(QWidget* parent)
	: Component(parent, ToolType::LockedFileExplorer)
{
	ui.setupUi(this);
	for (auto child : findChildren<QWidget*>()) {
		child->setAcceptDrops(false);
	}

	setAcceptDrops(true);

	connect(ui.BrowseButton, &QPushButton::clicked, this, &LockedFileExplorer::OnBrowsePressed);
	connect(ui.PathField, &QLineEdit::returnPressed, this, &LockedFileExplorer::OnPathSubmitted);
}

QJsonObject LockedFileExplorer::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.PathField, text);
	);

	return state;
}

void LockedFileExplorer::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.PathField, setText, toString);
	);

	if (!ui.PathField->text().isEmpty()) {
		OpenPath(ui.PathField->text(), false);
	}
}

void LockedFileExplorer::dragEnterEvent(QDragEnterEvent* event)
{
	const auto urls = event->mimeData()->urls();
	if (urls.size() == 1 && urls.first().isLocalFile() && QFileInfo::exists(urls.first().toLocalFile())) {
		event->acceptProposedAction();
	}
}

void LockedFileExplorer::dropEvent(QDropEvent* event)
{
	const auto urls = event->mimeData()->urls();
	if (urls.size() != 1 || !urls.first().isLocalFile()) {
		return;
	}

	OpenPath(urls.first().toLocalFile());
	event->acceptProposedAction();
}

void LockedFileExplorer::OnBrowsePressed()
{
	QMenu browseMenu(this);
	auto fileAction = browseMenu.addAction("Select file...");
	auto folderAction = browseMenu.addAction("Select folder...");
	auto selectedAction = browseMenu.exec(ui.BrowseButton->mapToGlobal(QPoint(0, ui.BrowseButton->height())));

	if (selectedAction == fileAction) {
		auto filePath = QFileDialog::getOpenFileName(this, "Select file", ui.PathField->text(), "All files (*)");

		if (!filePath.isEmpty()) {
			OpenPath(filePath);
		}
	}
	else if (selectedAction == folderAction) {
		auto folderPath = QFileDialog::getExistingDirectory(this, "Select folder", ui.PathField->text());

		if (!folderPath.isEmpty()) {
			OpenPath(folderPath);
		}
	}
}

void LockedFileExplorer::OnPathSubmitted()
{
	OpenPath(ui.PathField->text());
}

void LockedFileExplorer::OnKillPressed()
{
	auto button = qobject_cast<QPushButton*>(sender());
	if (!button) {
		return;
	}

	const auto processId = button->property("ProcessId").toUInt();
	auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
		FALSE, processId);

	if (!process) {
		QMessageBox::critical(this, "Unable to kill process",
			QString("Could not open process %1 for termination. Windows error: %2.")
				.arg(processId).arg(GetLastError()));
		return;
	}

	FILETIME creationTime{};
	FILETIME exitTime{};
	FILETIME kernelTime{};
	FILETIME userTime{};
	ULARGE_INTEGER creationTimeValue{};
	if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)) {
		creationTimeValue.LowPart = creationTime.dwLowDateTime;
		creationTimeValue.HighPart = creationTime.dwHighDateTime;
	}

	if (creationTimeValue.QuadPart != button->property("ProcessStartTime").toULongLong()) {
		CloseHandle(process);
		QMessageBox::warning(this, "Process changed",
			"The locking process has already exited. Refreshing the result.");
		OpenPath(ui.PathField->text(), false);
		return;
	}

	if (!TerminateProcess(process, 1)) {
		const auto error = GetLastError();
		CloseHandle(process);
		QMessageBox::critical(this, "Unable to kill process",
			QString("Could not terminate process %1. Windows error: %2.")
				.arg(processId).arg(error));
		return;
	}

	WaitForSingleObject(process, 3'000);
	CloseHandle(process);
	OpenPath(ui.PathField->text(), false);
}

void LockedFileExplorer::OnExecutableLinkActivated(const QString& link)
{
	auto executablePath = QUrl(link).toLocalFile();
	QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(executablePath)});
}

QList<LockedFileExplorer::ProcessInfo> LockedFileExplorer::FindLockingProcesses(
	const QString& filePath, QString& error)
{
	auto fileInfo = QFileInfo(filePath);

	if (fileInfo.isDir()) {
		return FindProcessesUsingDirectory(fileInfo.absoluteFilePath(), error);
	}

	DWORD sessionHandle{};
	wchar_t sessionKey[CCH_RM_SESSION_KEY + 1]{};
	auto result = RmStartSession(&sessionHandle, 0, sessionKey);

	if (result != ERROR_SUCCESS) {
		error = QString("Could not start a Restart Manager session. Windows error: %1.").arg(result);
		return {};
	}

	const auto nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath()).toStdWString();
	const wchar_t* resources[]{nativePath.c_str()};
	result = RmRegisterResources(sessionHandle, 1, resources, 0, nullptr, 0, nullptr);

	if (result != ERROR_SUCCESS) {
		RmEndSession(sessionHandle);
		error = QString("Could not inspect the file. Windows error: %1.").arg(result);
		return {};
	}

	UINT processCount{};
	UINT processCapacity{};
	DWORD rebootReasons{};
	result = RmGetList(sessionHandle, &processCapacity, &processCount, nullptr, &rebootReasons);

	if (result == ERROR_SUCCESS) {
		RmEndSession(sessionHandle);
		return {};
	}

	if (result != ERROR_MORE_DATA) {
		RmEndSession(sessionHandle);
		error = QString("Could not query processes using the file. Windows error: %1.").arg(result);
		return {};
	}

	std::vector<RM_PROCESS_INFO> processEntries(processCapacity);
	processCount = processCapacity;
	result = RmGetList(sessionHandle, &processCapacity, &processCount, processEntries.data(), &rebootReasons);

	if (result != ERROR_SUCCESS) {
		RmEndSession(sessionHandle);
		error = QString("Could not query processes using the file. Windows error: %1.").arg(result);
		return {};
	}

	QList<ProcessInfo> processes;

	for (UINT idx = 0; idx < processCount; ++idx) {
		const auto& entry = processEntries[idx];
		ULARGE_INTEGER startTime{};
		startTime.LowPart = entry.Process.ProcessStartTime.dwLowDateTime;
		startTime.HighPart = entry.Process.ProcessStartTime.dwHighDateTime;
		processes.append(GetProcessInfo(entry.Process.dwProcessId, startTime.QuadPart,
			QString::fromWCharArray(entry.strAppName)));
	}

	RmEndSession(sessionHandle);
	return processes;
}

QList<LockedFileExplorer::ProcessInfo> LockedFileExplorer::FindProcessesUsingDirectory(
	const QString& directoryPath, QString& error)
{
	using NtQuerySystemInformation = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
	auto ntdll = GetModuleHandleW(L"ntdll.dll");
	auto querySystem = reinterpret_cast<NtQuerySystemInformation>(
		GetProcAddress(ntdll, "NtQuerySystemInformation"));

	if (!querySystem) {
		error = "Could not access the Windows handle list.";
		return {};
	}

	auto directoryPathNative = QDir::toNativeSeparators(QFileInfo(directoryPath).absoluteFilePath()).toStdWString();
	auto directoryHandle = CreateFileW(directoryPathNative.c_str(), 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (directoryHandle == INVALID_HANDLE_VALUE) {
		error = QString("Could not open the folder. Windows error: %1.").arg(GetLastError());
		return {};
	}

	QByteArray buffer(1'048'576, Qt::Uninitialized);
	ULONG requiredSize{};
	NTSTATUS status{};

	while (true) {
		status = querySystem(SystemExtendedHandleInformation, buffer.data(),
			static_cast<ULONG>(buffer.size()), &requiredSize);

		if (status != StatusInfoLengthMismatch) {
			break;
		}

		int newSize = buffer.size() * 2;

		if (requiredSize > static_cast<ULONG>(newSize)) {
			newSize = static_cast<int>(requiredSize);
		}

		buffer.resize(newSize);
	}

	if (status < 0) {
		CloseHandle(directoryHandle);
		error = QString("Could not query Windows handles. NT status: 0x%1.")
			.arg(static_cast<quint32>(status), 8, 16, QChar('0'));
		return {};
	}

	const auto handleInformation = reinterpret_cast<const SystemHandleInformation*>(buffer.constData());
	quint16 fileObjectType{};

	for (quintptr idx = 0; idx < handleInformation->HandleCount; ++idx) {
		const auto& entry = handleInformation->Handles[idx];

		if (entry.ProcessId == GetCurrentProcessId()
			&& entry.HandleValue == reinterpret_cast<quintptr>(directoryHandle)) {
			fileObjectType = entry.ObjectTypeIdx;
			break;
		}
	}

	CloseHandle(directoryHandle);

	if (!fileObjectType) {
		error = "Could not identify Windows file handles.";
		return {};
	}

	auto normalizedDirectory = QDir::cleanPath(QFileInfo(directoryPath).absoluteFilePath());
	auto directoryPrefix = normalizedDirectory + "/";
	auto currentProcess = GetCurrentProcess();
	QHash<quint32, HANDLE> processHandles;
	QSet<quint32> inaccessibleProcesses;
	QSet<quint32> matchedProcesses;
	QList<ProcessInfo> processes;

	for (quintptr idx = 0; idx < handleInformation->HandleCount; ++idx) {
		const auto& entry = handleInformation->Handles[idx];

		if (entry.ObjectTypeIdx != fileObjectType || entry.ProcessId > MAXDWORD) {
			continue;
		}

		const auto processId = static_cast<quint32>(entry.ProcessId);

		if (matchedProcesses.contains(processId) || inaccessibleProcesses.contains(processId)) {
			continue;
		}

		auto process = processHandles.value(processId, nullptr);

		if (!process) {
			process = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

			if (!process) {
				inaccessibleProcesses.insert(processId);
				continue;
			}

			processHandles.insert(processId, process);
		}

		HANDLE duplicateHandle{};
		auto sourceHandle = reinterpret_cast<HANDLE>(entry.HandleValue);

		if (!DuplicateHandle(process, sourceHandle, currentProcess, &duplicateHandle,
			0, FALSE, DUPLICATE_SAME_ACCESS)) {
			continue;
		}

		if (GetFileType(duplicateHandle) != FILE_TYPE_DISK) {
			CloseHandle(duplicateHandle);
			continue;
		}

		QString handlePath(32'768, Qt::Uninitialized);
		const auto pathLength = GetFinalPathNameByHandleW(duplicateHandle,
			reinterpret_cast<wchar_t*>(handlePath.data()), static_cast<DWORD>(handlePath.size()),
			FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		CloseHandle(duplicateHandle);

		if (!pathLength || pathLength >= static_cast<DWORD>(handlePath.size())) {
			continue;
		}

		handlePath.resize(pathLength);

		if (handlePath.startsWith("\\\\?\\UNC\\")) {
			handlePath.replace(0, 8, "\\\\");
		}
		else if (handlePath.startsWith("\\\\?\\")) {
			handlePath.remove(0, 4);
		}

		handlePath = QDir::cleanPath(QDir::fromNativeSeparators(handlePath));

		if (handlePath.compare(normalizedDirectory, Qt::CaseInsensitive) != 0
			&& !handlePath.startsWith(directoryPrefix, Qt::CaseInsensitive)) {
			continue;
		}

		FILETIME creationTime{};
		FILETIME exitTime{};
		FILETIME kernelTime{};
		FILETIME userTime{};
		ULARGE_INTEGER startTime{};

		if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)) {
			startTime.LowPart = creationTime.dwLowDateTime;
			startTime.HighPart = creationTime.dwHighDateTime;
		}

		matchedProcesses.insert(processId);
		processes.append(GetProcessInfo(processId, startTime.QuadPart, {}));
	}

	for (auto process : processHandles) {
		CloseHandle(process);
	}

	return processes;
}

LockedFileExplorer::ProcessInfo LockedFileExplorer::GetProcessInfo(
	quint32 processId, quint64 startTime, const QString& fallbackName)
{
	ProcessInfo processInfo{processId, startTime, fallbackName, {}, {}};
	auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

	if (!process) {
		processInfo.CommandLine = "Unavailable";
		return processInfo;
	}

	DWORD pathLength = 32'768;
	QString executablePath(pathLength, Qt::Uninitialized);

	if (QueryFullProcessImageNameW(process, 0, reinterpret_cast<wchar_t*>(executablePath.data()), &pathLength)) {
		executablePath.resize(pathLength);
		processInfo.ExecutablePath = executablePath;
		processInfo.Name = QFileInfo(executablePath).fileName();
	}

	processInfo.CommandLine = GetProcessCommandLine(process);
	CloseHandle(process);
	return processInfo;
}

QString LockedFileExplorer::GetProcessCommandLine(void* processHandle)
{
	using NtQueryInformationProcess = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
	auto ntdll = GetModuleHandleW(L"ntdll.dll");
	auto queryProcess = reinterpret_cast<NtQueryInformationProcess>(GetProcAddress(ntdll, "NtQueryInformationProcess"));

	if (!queryProcess) {
		return "Unavailable";
	}

	ULONG bufferSize{};
	constexpr ULONG processCommandLineInformation = 60;
	queryProcess(processHandle, processCommandLineInformation, nullptr, 0, &bufferSize);

	if (!bufferSize) {
		return "Unavailable";
	}

	QByteArray buffer(bufferSize, Qt::Uninitialized);
	if (queryProcess(processHandle, processCommandLineInformation, buffer.data(), bufferSize, &bufferSize) < 0) {
		return "Unavailable";
	}

	const auto commandLine = reinterpret_cast<const UNICODE_STRING*>(buffer.constData());
	return QString::fromWCharArray(commandLine->Buffer, commandLine->Length / sizeof(wchar_t));
}

void LockedFileExplorer::ClearProcesses()
{
	while (auto item = ui.ProcessListLayout->takeAt(0)) {
		if (item->widget()) {
			item->widget()->deleteLater();
		}

		delete item;
	}
}

void LockedFileExplorer::OpenPath(const QString& path, bool notifyModification)
{
	auto fileInfo = QFileInfo(path);

	if (!fileInfo.exists()) {
		ui.StatusLabel->setText("Select an existing file or folder.");
		ClearProcesses();
		return;
	}

	ui.PathField->setText(fileInfo.absoluteFilePath());
	QString error;
	auto processes = FindLockingProcesses(fileInfo.absoluteFilePath(), error);

	if (!error.isEmpty()) {
		ui.StatusLabel->setText(error);
		ClearProcesses();
		return;
	}

	ShowProcesses(processes, fileInfo.isDir());

	if (notifyModification) {
		emit Modified(this);
	}
}

void LockedFileExplorer::ShowProcesses(const QList<ProcessInfo>& processes, bool directory)
{
	ClearProcesses();
	const auto resourceType = directory ? "folder" : "file";

	if (processes.isEmpty()) {
		ui.StatusLabel->setText(QString("The %1 is not locked by any process.").arg(resourceType));
	}
	else {
		ui.StatusLabel->setText(QString("The %1 is locked by %2 process(es).")
			.arg(resourceType).arg(processes.size()));
	}

	for (const auto& process : processes) {
		auto entry = new QFrame(ui.ProcessList);
		entry->setObjectName("ProcessEntry");
		entry->setAutoFillBackground(true);
		entry->setFrameShape(QFrame::StyledPanel);
		entry->setStyleSheet("#ProcessEntry { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }");
		auto entryLayout = new QHBoxLayout(entry);
		entryLayout->setContentsMargins(12, 12, 12, 12);
		entryLayout->setSpacing(12);

		auto iconLabel = new QLabel(entry);
		auto icon = process.ExecutablePath.isEmpty()
			? style()->standardIcon(QStyle::SP_ComputerIcon)
			: QFileIconProvider{}.icon(QFileInfo(process.ExecutablePath));
		iconLabel->setPixmap(icon.pixmap(40, 40));
		iconLabel->setFixedSize(40, 40);
		iconLabel->setAlignment(Qt::AlignCenter);
		entryLayout->addWidget(iconLabel, 0, Qt::AlignTop);

		auto detailsLayout = new QVBoxLayout;
		detailsLayout->setContentsMargins(0, 0, 0, 0);
		detailsLayout->setSpacing(5);
		auto headerLayout = new QHBoxLayout;
		headerLayout->setContentsMargins(0, 0, 0, 0);

		auto nameLabel = new QLabel(process.Name.isEmpty() ? "Unknown process" : process.Name, entry);
		auto nameFont = nameLabel->font();
		nameFont.setBold(true);
		nameFont.setPointSize(nameFont.pointSize() + 1);
		nameLabel->setFont(nameFont);
		headerLayout->addWidget(nameLabel);
		headerLayout->addStretch();

		auto killButton = new QPushButton("Kill", entry);
		killButton->setProperty("ProcessId", process.Id);
		killButton->setProperty("ProcessStartTime", process.StartTime);
		connect(killButton, &QPushButton::clicked, this, &LockedFileExplorer::OnKillPressed);
		headerLayout->addWidget(killButton);
		detailsLayout->addLayout(headerLayout);

		if (!process.ExecutablePath.isEmpty()) {
			auto pathLink = new QLabel(entry);
			pathLink->setText(QString("<a href=\"%1\">%2</a>")
				.arg(QUrl::fromLocalFile(process.ExecutablePath).toString().toHtmlEscaped(),
					process.ExecutablePath.toHtmlEscaped()));
			pathLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
			pathLink->setToolTip(process.ExecutablePath);
			connect(pathLink, &QLabel::linkActivated, this, &LockedFileExplorer::OnExecutableLinkActivated);
			detailsLayout->addWidget(pathLink);
		}
		else {
			auto pathLabel = new QLabel("Unavailable", entry);
			detailsLayout->addWidget(pathLabel);
		}

		auto commandLineLabel = new QLabel(QString("Command line: %1").arg(process.CommandLine), entry);
		commandLineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		commandLineLabel->setToolTip(process.CommandLine);
		commandLineLabel->setWordWrap(true);
		detailsLayout->addWidget(commandLineLabel);
		entryLayout->addLayout(detailsLayout, 1);

		ui.ProcessListLayout->addWidget(entry);
	}
}
