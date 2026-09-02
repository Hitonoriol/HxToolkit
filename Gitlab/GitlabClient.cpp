#include "GitlabClient.h"

#include "WorkItem.h"
#include "Util/Settings.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <algorithm>

GitlabClient::GitlabClient(QObject* parent)
	: QObject(parent)
{
}

void GitlabClient::getWorkItems()
{
	fetchedWorkItems = {};
	getWorkItemsPage(1);
}

void GitlabClient::getMergeRequests(int days)
{
	++mergeRequestRequestId;
	fetchedMergeRequests = {};
	mergeRequests.clear();
	getMergeRequestsPage(1, days, mergeRequestRequestId);
}

const std::vector<WorkItem>& GitlabClient::getCachedWorkItems() const
{
	return workItems;
}

const std::vector<MergeRequest>& GitlabClient::getMergeRequests() const
{
	return mergeRequests;
}

WorkItem* GitlabClient::getWorkItem(const QString& workItemId)
{
	auto it = std::find_if(workItems.begin(), workItems.end(), [&workItemId](const WorkItem& workItem) {
		return workItem.Id == workItemId;
	});

	if (it == workItems.end()) {
		return {};
	}

	return &*it;
}

QJsonArray GitlabClient::SaveState() const
{
	QJsonArray state;
	for (auto& workItem : workItems) {
		state.append(workItem.SaveState());
	}

	return state;
}

void GitlabClient::LoadState(const QJsonArray& state)
{
	workItems.clear();
	for (auto item : state) {
		WorkItem workItem;
		workItem.LoadState(item.toObject());
		workItems.push_back(workItem);
	}

	emit WorkItemsUpdated();
}

void GitlabClient::StartTracking(const QString& workItemId)
{
	auto workItem = getWorkItem(workItemId);
	if (!workItem || workItem->isTracking()) {
		return;
	}

	workItem->StartTracking();
	emit WorkItemUpdated(workItemId);
}

void GitlabClient::PauseTracking(const QString& workItemId)
{
	auto workItem = getWorkItem(workItemId);
	if (!workItem || !workItem->isTracking()) {
		return;
	}

	auto elapsedSeconds = workItem->PauseTracking();
	if (elapsedSeconds <= 0) {
		emit WorkItemUpdated(workItemId);
		return;
	}

	workItem->IsSubmittingElapsedTime = true;
	emit WorkItemUpdated(workItemId);
	AddElapsedTime(*workItem, elapsedSeconds);
}

void GitlabClient::DiscardTracking(const QString& workItemId)
{
	auto workItem = getWorkItem(workItemId);
	if (!workItem || !workItem->isTracking()) {
		return;
	}

	workItem->TrackingStartedAt = 0;
	emit WorkItemUpdated(workItemId);
}

void GitlabClient::getWorkItemsPage(int page)
{
	auto gitlabUrl = Settings::GetString(Option::GitlabUrl);
	auto token = Settings::GetString(Option::GitlabToken);

	if (gitlabUrl.isEmpty() || token.isEmpty()) {
		emit requestFailed("GitLab URL and token must be configured in Settings.");
		return;
	}

	QUrl url(gitlabUrl);
	if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
		emit requestFailed("The configured GitLab URL is invalid.");
		return;
	}

	auto apiPath = url.path();
	while (apiPath.endsWith('/')) {
		apiPath.chop(1);
	}

	if (!apiPath.endsWith("/api/v4")) {
		apiPath += "/api/v4";
	}
	url.setPath(apiPath + "/issues");

	QUrlQuery query;
	query.addQueryItem("scope", "assigned_to_me");
	query.addQueryItem("state", "opened");
	query.addQueryItem("per_page", "100");
	query.addQueryItem("page", QString::number(page));
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
	request.setRawHeader("Accept", "application/json");

	auto reply = networkManager.get(request);
	connect(reply, &QNetworkReply::finished, this, &GitlabClient::OnWorkItemsReplyFinished);
}

void GitlabClient::getMergeRequestsPage(int page, int days, int requestId)
{
	auto gitlabUrl = Settings::GetString(Option::GitlabUrl);
	auto token = Settings::GetString(Option::GitlabToken);
	if (gitlabUrl.isEmpty() || token.isEmpty()) {
		emit requestFailed("GitLab URL and token must be configured in Settings.");
		return;
	}

	QUrl url(gitlabUrl);
	if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
		emit requestFailed("The configured GitLab URL is invalid.");
		return;
	}

	auto apiPath = url.path();
	while (apiPath.endsWith('/')) {
		apiPath.chop(1);
	}

	if (!apiPath.endsWith("/api/v4")) {
		apiPath += "/api/v4";
	}

	url.setPath(apiPath + "/merge_requests");
	QUrlQuery query;
	query.addQueryItem("scope", "created_by_me");
	query.addQueryItem("state", "all");
	query.addQueryItem("order_by", "created_at");
	query.addQueryItem("sort", "desc");
	query.addQueryItem("per_page", "100");
	query.addQueryItem("page", QString::number(page));
	if (days > 0) {
		query.addQueryItem("created_after", QDateTime::currentDateTimeUtc().addDays(-days).toString(Qt::ISODate));
	}

	url.setQuery(query);
	QNetworkRequest request(url);
	request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
	request.setRawHeader("Accept", "application/json");
	auto reply = networkManager.get(request);
	reply->setProperty("mergeRequestRequestId", requestId);
	reply->setProperty("days", days);
	connect(reply, &QNetworkReply::finished, this, &GitlabClient::OnMergeRequestsReplyFinished);
}

void GitlabClient::getMergeRequestDiffStats(const MergeRequest& mergeRequest, int requestId)
{
	QUrl url(Settings::GetString(Option::GitlabUrl));
	auto apiPath = url.path();
	while (apiPath.endsWith('/')) {
		apiPath.chop(1);
	}

	if (!apiPath.endsWith("/api/v4")) {
		apiPath += "/api/v4";
	}

	url.setPath(QString("%1/projects/%2/merge_requests/%3/changes").arg(apiPath).arg(mergeRequest.ProjectId).arg(mergeRequest.Iid));
	QNetworkRequest request(url);
	request.setRawHeader("PRIVATE-TOKEN", Settings::GetString(Option::GitlabToken).toUtf8());
	request.setRawHeader("Accept", "application/json");
	auto reply = networkManager.get(request);
	reply->setProperty("mergeRequestRequestId", requestId);
	reply->setProperty("mergeRequestId", mergeRequest.Id);
	connect(reply, &QNetworkReply::finished, this, &GitlabClient::OnMergeRequestDiffStatsReplyFinished);
}

void GitlabClient::OnWorkItemsReplyFinished()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) {
		return;
	}

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (reply->error() != QNetworkReply::NoError) {
		emit requestFailed(QString("GitLab request failed (%1): %2").arg(statusCode).arg(reply->errorString()));
		reply->deleteLater();
		return;
	}

	auto document = QJsonDocument::fromJson(reply->readAll());
	if (!document.isArray()) {
		emit requestFailed("GitLab returned an unexpected response while retrieving work items.");
		reply->deleteLater();
		return;
	}

	auto items = document.array();
	for (auto item : items) {
		fetchedWorkItems.append(item);
	}

	auto nextPage = reply->rawHeader("X-Next-Page");
	reply->deleteLater();
	if (!nextPage.isEmpty()) {
		getWorkItemsPage(nextPage.toInt());
		return;
	}

	auto workItemsChanged = UpdateWorkItems(fetchedWorkItems);
	emit WorkItemsUpdated();
	if (workItemsChanged) {
		emit WorkItemsChanged();
	}
}

void GitlabClient::OnMergeRequestsReplyFinished()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) {
		return;
	}

	auto requestId = reply->property("mergeRequestRequestId").toInt();
	if (requestId != mergeRequestRequestId) {
		reply->deleteLater();
		return;
	}

	if (reply->error() != QNetworkReply::NoError) {
		emit requestFailed("GitLab request failed: " + reply->errorString());
		reply->deleteLater();
		return;
	}

	auto document = QJsonDocument::fromJson(reply->readAll());
	if (!document.isArray()) {
		emit requestFailed("GitLab returned an unexpected response while retrieving merge requests.");
		reply->deleteLater();
		return;
	}

	for (auto item : document.array()) {
		fetchedMergeRequests.append(item);
	}

	auto nextPage = reply->rawHeader("X-Next-Page");
	auto days = reply->property("days").toInt();
	reply->deleteLater();
	if (!nextPage.isEmpty()) {
		getMergeRequestsPage(nextPage.toInt(), days, requestId);
		return;
	}

	for (auto item : fetchedMergeRequests) {
		MergeRequest mergeRequest;
		mergeRequest.UpdateFromJson(item.toObject());
		mergeRequests.push_back(mergeRequest);
	}

	emit MergeRequestsUpdated();
	for (auto& mergeRequest : mergeRequests) {
		getMergeRequestDiffStats(mergeRequest, requestId);
	}
}

void GitlabClient::OnMergeRequestDiffStatsReplyFinished()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) {
		return;
	}

	if (reply->property("mergeRequestRequestId").toInt() != mergeRequestRequestId || reply->error() != QNetworkReply::NoError) {
		reply->deleteLater();
		return;
	}

	auto document = QJsonDocument::fromJson(reply->readAll());
	auto mergeRequestId = reply->property("mergeRequestId").toString();
	auto it = std::find_if(mergeRequests.begin(), mergeRequests.end(), [&mergeRequestId](const MergeRequest& mergeRequest) {
		return mergeRequest.Id == mergeRequestId;
	});

	if (it == mergeRequests.end() || !document.isObject()) {
		reply->deleteLater();
		return;
	}

	for (auto change : document.object()["changes"].toArray()) {
		auto lines = change.toObject()["diff"].toString().split('\n');
		for (auto& line : lines) {
			if (line.startsWith('+') && !line.startsWith("+++")) {
				++it->Additions;
			}
			else if (line.startsWith('-') && !line.startsWith("---")) {
				++it->Deletions;
			}
		}
	}

	it->HasDiffStats = true;
	emit MergeRequestUpdated(mergeRequestId);
	reply->deleteLater();
}

void GitlabClient::AddElapsedTime(WorkItem& workItem, int64_t elapsedSeconds)
{
	if (elapsedSeconds <= 0) {
		return;
	}

	auto gitlabUrl = Settings::GetString(Option::GitlabUrl);
	auto token = Settings::GetString(Option::GitlabToken);
	QUrl url(gitlabUrl);
	auto apiPath = url.path();
	while (apiPath.endsWith('/')) {
		apiPath.chop(1);
	}

	if (!apiPath.endsWith("/api/v4")) {
		apiPath += "/api/v4";
	}
	url.setPath(QString("%1/projects/%2/issues/%3/add_spent_time").arg(apiPath).arg(workItem.ProjectId).arg(workItem.Iid));

	QUrlQuery query;
	query.addQueryItem("duration", QString::number(elapsedSeconds) + "s");
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
	request.setRawHeader("Accept", "application/json");

	auto reply = networkManager.post(request, QByteArray{});
	reply->setProperty("workItemId", workItem.Id);
	reply->setProperty("elapsedSeconds", elapsedSeconds);
	connect(reply, &QNetworkReply::finished, this, &GitlabClient::OnAddElapsedTimeReplyFinished);
}

bool GitlabClient::UpdateWorkItems(const QJsonArray& items)
{
	bool workItemsChanged = false;
	std::vector<QString> fetchedWorkItemIds;
	for (auto item : items) {
		auto json = item.toObject();
		auto workItemId = json["id"].toVariant().toString();
		fetchedWorkItemIds.push_back(workItemId);
		auto it = std::find_if(workItems.begin(), workItems.end(), [&workItemId](const WorkItem& workItem) {
			return workItem.Id == workItemId;
		});

		if (it == workItems.end()) {
			WorkItem workItem;
			workItem.UpdateFromJson(json);
			workItems.push_back(workItem);
			workItemsChanged = true;
		}
		else {
			auto oldState = it->SaveState();
			it->UpdateFromJson(json);
			workItemsChanged |= oldState != it->SaveState();
		}
	}

	auto oldCount = workItems.size();
	std::erase_if(workItems, [&fetchedWorkItemIds](const WorkItem& workItem) {
		auto it = std::find(fetchedWorkItemIds.begin(), fetchedWorkItemIds.end(), workItem.Id);
		return it == fetchedWorkItemIds.end() && !workItem.isTracking() && workItem.LocalElapsedSeconds == 0;
	});
	workItemsChanged |= workItems.size() != oldCount;
	return workItemsChanged;
}

void GitlabClient::OnAddElapsedTimeReplyFinished()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) {
		return;
	}

	auto workItemId = reply->property("workItemId").toString();
	auto elapsedSeconds = reply->property("elapsedSeconds").toLongLong();
	if (reply->error() != QNetworkReply::NoError) {
		auto workItem = getWorkItem(workItemId);
		if (workItem) {
			workItem->IsSubmittingElapsedTime = false;
			emit WorkItemUpdated(workItemId);
		}

		emit requestFailed(QString("Failed to add GitLab elapsed time: %1").arg(reply->errorString()));
		reply->deleteLater();
		return;
	}

	auto document = QJsonDocument::fromJson(reply->readAll());
	auto workItem = getWorkItem(workItemId);
	if (workItem) {
		if (document.isObject()) {
			workItem->ConfirmElapsedTime(elapsedSeconds, document.object()["total_time_spent"].toVariant().toLongLong());
		}

		workItem->IsSubmittingElapsedTime = false;
		emit WorkItemUpdated(workItemId);
	}

	emit ElapsedTimeAdded();

	reply->deleteLater();
}
