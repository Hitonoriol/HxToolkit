#include "MergeRequest.h"

void MergeRequest::UpdateFromJson(const QJsonObject& json)
{
	Id = json["id"].toVariant().toString();
	ProjectId = json["project_id"].toInt();
	Iid = json["iid"].toInt();
	Reference = json["references"].toObject()["full"].toString();
	if (Reference.isEmpty()) {
		Reference = "!" + QString::number(Iid);
	}

	WebUrl = json["web_url"].toString();
	Title = json["title"].toString();
	State = json["state"].toString();
	CreatedAt = QDateTime::fromString(json["created_at"].toString(), Qt::ISODate);
}
