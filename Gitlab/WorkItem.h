#pragma once

#include <QJsonObject>
#include <QString>

#include <cstdint>

struct WorkItem
{
	QString Id;
	int ProjectId{};
	int Iid{};
	QString Project;
	QString Reference;
	QString WebUrl;
	QString Title;
	QString State;
	QString Description;
	int64_t GitlabElapsedSeconds{};
	int64_t LocalElapsedSeconds{};
	int64_t TrackingStartedAt{};
	bool IsSubmittingElapsedTime = false;

	void UpdateFromJson(const QJsonObject& json);
	QJsonObject SaveState() const;
	void LoadState(const QJsonObject& state);
	int64_t getElapsedSeconds() const;
	QString getElapsedTimeString() const;
	bool isTracking() const;
	bool isSubmittingElapsedTime() const;
	void StartTracking();
	int64_t PauseTracking();
	void ConfirmElapsedTime(int64_t elapsedSeconds, int64_t gitlabElapsedSeconds);
};
