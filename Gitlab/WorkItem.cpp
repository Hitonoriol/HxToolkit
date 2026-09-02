#include "WorkItem.h"

#include <QDateTime>

void WorkItem::UpdateFromJson(const QJsonObject& json)
{
	Id = json["id"].toVariant().toString();
	ProjectId = json["project_id"].toInt();
	Iid = json["iid"].toInt();
	WebUrl = json["web_url"].toString();
	Reference = json["references"].toObject()["full"].toString();
	if (Reference.isEmpty()) {
		Reference = "#" + QString::number(Iid);
	}
	Project = Reference.left(Reference.indexOf('#'));
	if (Project.isEmpty()) {
		Project = "Project " + QString::number(ProjectId);
	}

	Title = json["title"].toString();
	State = json["state"].toString();
	Description = json["description"].toString();
	GitlabElapsedSeconds = json["time_stats"].toObject()["total_time_spent"].toVariant().toLongLong();
}

QJsonObject WorkItem::SaveState() const
{
	QJsonObject state;
	state["Id"] = Id;
	state["ProjectId"] = ProjectId;
	state["Iid"] = Iid;
	state["Project"] = Project;
	state["Reference"] = Reference;
	state["WebUrl"] = WebUrl;
	state["Title"] = Title;
	state["State"] = State;
	state["Description"] = Description;
	state["GitlabElapsedSeconds"] = static_cast<qint64>(GitlabElapsedSeconds);
	state["LocalElapsedSeconds"] = static_cast<qint64>(LocalElapsedSeconds);
	state["TrackingStartedAt"] = static_cast<qint64>(TrackingStartedAt);
	return state;
}

void WorkItem::LoadState(const QJsonObject& state)
{
	Id = state["Id"].toString();
	ProjectId = state["ProjectId"].toInt();
	Iid = state["Iid"].toInt();
	Project = state["Project"].toString();
	Reference = state["Reference"].toString();
	WebUrl = state["WebUrl"].toString();
	Title = state["Title"].toString();
	State = state["State"].toString();
	Description = state["Description"].toString();
	GitlabElapsedSeconds = state["GitlabElapsedSeconds"].toVariant().toLongLong();
	LocalElapsedSeconds = state["LocalElapsedSeconds"].toVariant().toLongLong();
	TrackingStartedAt = state["TrackingStartedAt"].toVariant().toLongLong();
}

int64_t WorkItem::getElapsedSeconds() const
{
	auto elapsedSeconds = GitlabElapsedSeconds + LocalElapsedSeconds;
	if (isTracking()) {
		elapsedSeconds += QDateTime::currentSecsSinceEpoch() - TrackingStartedAt;
	}

	return elapsedSeconds;
}

QString WorkItem::getElapsedTimeString() const
{
	auto elapsedSeconds = getElapsedSeconds();
	auto hours = elapsedSeconds / 3'600;
	auto minutes = (elapsedSeconds / 60) % 60;
	auto seconds = elapsedSeconds % 60;
	return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

bool WorkItem::isTracking() const
{
	return TrackingStartedAt > 0;
}

bool WorkItem::isSubmittingElapsedTime() const
{
	return IsSubmittingElapsedTime;
}

void WorkItem::StartTracking()
{
	TrackingStartedAt = QDateTime::currentSecsSinceEpoch();
}

int64_t WorkItem::PauseTracking()
{
	if (!isTracking()) {
		return 0;
	}

	auto elapsedSeconds = QDateTime::currentSecsSinceEpoch() - TrackingStartedAt;
	LocalElapsedSeconds += elapsedSeconds;
	TrackingStartedAt = 0;
	return elapsedSeconds;
}

void WorkItem::ConfirmElapsedTime(int64_t elapsedSeconds, int64_t gitlabElapsedSeconds)
{
	GitlabElapsedSeconds = gitlabElapsedSeconds;
	LocalElapsedSeconds = qMax<int64_t>(0, LocalElapsedSeconds - elapsedSeconds);
}
