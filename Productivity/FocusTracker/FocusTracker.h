#pragma once

#include "UI/Component.h"
#include "ui_FocusTracker.h"

#include <QDateTime>
#include <QHash>
#include <QTimer>

class FocusTracker : public Component
{
	Q_OBJECT

public:
	FocusTracker(QWidget* parent = nullptr);
	~FocusTracker();

	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private slots:
	void TrackForegroundWindow();
	void UpdateTable();

private:
	struct FocusSession
	{
		QString ProcessName;
		QString Title;
		QString ExecutablePath;
		QDateTime StartTime;
		QDateTime EndTime;
	};

	struct ProcessActivity
	{
		QString ProcessName;
		QString ExecutablePath;
		qint64 DurationMs{};
		QHash<QString, qint64> TitleDurations;
	};

	void FinishCurrentSession(const QDateTime& endTime);
	QDateTime GetPeriodStart() const;
	qint64 GetSessionDurationInPeriod(const FocusSession& session, const QDateTime& periodStart, const QDateTime& now) const;
	QString GetDurationString(qint64 durationMs) const;

	Ui::FocusTrackerClass ui;
	QTimer trackingTimer;
	QList<FocusSession> sessions;
	quintptr currentWindowId{};
	QString currentProcessName;
	QString currentTitle;
	QString currentExecutablePath;
};
