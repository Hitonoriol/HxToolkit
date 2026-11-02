#pragma once

#include "UI/Component.h"
#include "ui_LockedFileExplorer.h"

#include <QList>

class QDragEnterEvent;
class QDropEvent;

class LockedFileExplorer : public Component
{
	Q_OBJECT

public:
	LockedFileExplorer(QWidget* parent = nullptr);

	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private slots:
	void OnBrowsePressed();
	void OnPathSubmitted();
	void OnKillPressed();
	void OnExecutableLinkActivated(const QString& link);

private:
	struct ProcessInfo
	{
		quint32 Id;
		quint64 StartTime;
		QString Name;
		QString ExecutablePath;
		QString CommandLine;
	};
	struct SystemHandleEntry
	{
		quintptr Object;
		quintptr ProcessId;
		quintptr HandleValue;
		quint32 GrantedAccess;
		quint16 CreatorBackTraceIdx;
		quint16 ObjectTypeIdx;
		quint32 HandleAttributes;
		quint32 Reserved;
	};
	struct SystemHandleInformation
	{
		quintptr HandleCount;
		quintptr Reserved;
		SystemHandleEntry Handles[1];
	};

	static constexpr unsigned long SystemExtendedHandleInformation = 64;
	static constexpr long StatusInfoLengthMismatch = -1'073'741'820;

	static QList<ProcessInfo> FindLockingProcesses(const QString& filePath, QString& error);
	static QList<ProcessInfo> FindProcessesUsingDirectory(const QString& directoryPath, QString& error);
	static ProcessInfo GetProcessInfo(quint32 processId, quint64 startTime, const QString& fallbackName);
	static QString GetProcessCommandLine(void* processHandle);
	void ClearProcesses();
	void OpenPath(const QString& path, bool notifyModification = true);
	void ShowProcesses(const QList<ProcessInfo>& processes, bool directory);

	Ui::LockedFileExplorerClass ui;
};
