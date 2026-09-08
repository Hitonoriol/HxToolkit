#pragma once

#include "UI/Component.h"
#include "ui_GitlabTimeStats.h"

#include <QDateTime>

#include <cstdint>
#include <map>

class GitlabClient;

class GitlabTimeStats : public Component
{
	Q_OBJECT

public:
	GitlabTimeStats(QWidget* parent = nullptr);

	virtual QSize sizeHint() const override;
	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

private slots:
	void RefreshTimeStats();
	void OnTimeStatsUpdated();
	void OnGitlabRequestFailed(const QString& errorMessage);

private:
	QDateTime getPeriodStart() const;
	QDateTime getPeriodEnd() const;
	void UpdateChart(const std::map<QString, int64_t>& stats, int64_t totalSeconds);
	static QString GetDurationText(int64_t seconds);

private:
	Ui::GitlabTimeStatsClass ui;
	GitlabClient* gitlabClient;
};
