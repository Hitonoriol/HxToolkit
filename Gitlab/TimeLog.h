#pragma once

#include <QJsonObject>
#include <QString>

struct TimeLog
{
	QString Id;
	QString TimeSpent;
	QString SpentAt;
	QString Summary;

	void UpdateFromJson(const QJsonObject& json);
	QString getDisplayText() const;
};
