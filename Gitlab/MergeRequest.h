#pragma once

#include <QJsonObject>
#include <QString>
#include <QDateTime>

struct MergeRequest
{
	QString Id;
	int ProjectId{};
	int Iid{};
	QString Reference;
	QString WebUrl;
	QString Title;
	QString State;
	QDateTime CreatedAt;
	int Additions{};
	int Deletions{};
	bool HasDiffStats = false;

	void UpdateFromJson(const QJsonObject& json);
};
