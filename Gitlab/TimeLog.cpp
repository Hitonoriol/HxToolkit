#include "TimeLog.h"

#include <QDateTime>

void TimeLog::UpdateFromJson(const QJsonObject& json)
{
	Id = json["id"].toString();
	TimeSpent = json["timeSpent"].toVariant().toString();
	SpentAt = json["spentAt"].toString();
	Summary = json["summary"].toString();
}

QString TimeLog::getDisplayText() const
{
	bool isSeconds = false;
	auto seconds = TimeSpent.toLongLong(&isSeconds);
	auto duration = TimeSpent;
	if (isSeconds) {
		auto hours = seconds / 3'600;
		auto minutes = (seconds / 60) % 60;
		duration = hours > 0 ? QString("%1h %2m").arg(hours).arg(minutes) : QString("%1m").arg(minutes);
	}

	auto date = QDateTime::fromString(SpentAt, Qt::ISODate).toLocalTime();
	auto day = date.date().day();
	auto suffix = QString("th");
	if (day % 100 < 11 || day % 100 > 13) {
		switch (day % 10) {
		case 1:
			suffix = "st";
			break;

		case 2:
			suffix = "nd";
			break;

		case 3:
			suffix = "rd";
			break;

		default:
			break;
		}
	}

	auto spentAt = QString("%1 %2%3 %4").arg(date.toString("dddd, MMMM"), QString::number(day), suffix, QString::number(date.date().year()));
	return Summary.isEmpty() ? QString("%1 — %2").arg(spentAt, duration) : QString("%1 — %2 (%3)").arg(spentAt, duration, Summary);
}
