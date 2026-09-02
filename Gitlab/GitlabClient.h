#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>

#include "WorkItem.h"
#include "MergeRequest.h"

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

signals:
	void WorkItemsUpdated();
	void WorkItemsChanged();
	void WorkItemUpdated(const QString& workItemId);
	void ElapsedTimeAdded();
	void MergeRequestsUpdated();
	void MergeRequestUpdated(const QString& mergeRequestId);
	void requestFailed(const QString& errorMessage);

private:
	void getWorkItemsPage(int page);
	void getMergeRequestsPage(int page, int days, int requestId);
	void getMergeRequestDiffStats(const MergeRequest& mergeRequest, int requestId);
	void AddElapsedTime(WorkItem& workItem, int64_t elapsedSeconds);
	bool UpdateWorkItems(const QJsonArray& items);

private slots:
	void OnWorkItemsReplyFinished();
	void OnAddElapsedTimeReplyFinished();
	void OnMergeRequestsReplyFinished();
	void OnMergeRequestDiffStatsReplyFinished();

private:
	QNetworkAccessManager networkManager;
	QJsonArray fetchedWorkItems;
	QJsonArray fetchedMergeRequests;
	std::vector<WorkItem> workItems;
	std::vector<MergeRequest> mergeRequests;
	int mergeRequestRequestId = 0;
};
