#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>

#include "WorkItem.h"
#include "MergeRequest.h"
#include "TimeLog.h"

#include <cstdint>
#include <vector>

class GitlabClient : public QObject
{
	Q_OBJECT

public:
	explicit GitlabClient(QObject* parent = nullptr);

	void getWorkItems();
	void getMergeRequests(int days);
	const std::vector<WorkItem>& getCachedWorkItems() const;
	const std::vector<MergeRequest>& getMergeRequests() const;
	WorkItem* getWorkItem(const QString& workItemId);
	QJsonArray SaveState() const;
	void LoadState(const QJsonArray& state);
	void StartTracking(const QString& workItemId);
	void PauseTracking(const QString& workItemId);
	void DiscardTracking(const QString& workItemId);
	void AddSpentTime(const QString& workItemId, const QString& duration);
	void getTimeLogs(const QString& workItemId);
	const std::vector<TimeLog>& getTimeLogs() const;
	void DeleteTimeLogs(const QString& workItemId, const std::vector<QString>& timeLogIds);

signals:
	void WorkItemsUpdated();
	void WorkItemsChanged();
	void WorkItemUpdated(const QString& workItemId);
	void ElapsedTimeAdded();
	void MergeRequestsUpdated();
	void MergeRequestUpdated(const QString& mergeRequestId);
	void TimeLogsUpdated(const QString& workItemId);
	void requestFailed(const QString& errorMessage);

private:
	void getWorkItemsPage(int page);
	void getMergeRequestsPage(int page, int days, int requestId);
	void getMergeRequestDiffStats(const MergeRequest& mergeRequest, int requestId);
	void AddElapsedTime(WorkItem& workItem, const QString& duration);
	bool UpdateWorkItems(const QJsonArray& items);

private slots:
	void OnWorkItemsReplyFinished();
	void OnAddElapsedTimeReplyFinished();
	void OnMergeRequestsReplyFinished();
	void OnMergeRequestDiffStatsReplyFinished();
	void OnTimeLogsReplyFinished();
	void OnDeleteTimeLogReplyFinished();

private:
	QNetworkAccessManager networkManager;
	QJsonArray fetchedWorkItems;
	QJsonArray fetchedMergeRequests;
	std::vector<WorkItem> workItems;
	std::vector<MergeRequest> mergeRequests;
	std::vector<TimeLog> timeLogs;
	int mergeRequestRequestId = 0;
	int pendingTimeLogDeletions = 0;
};
