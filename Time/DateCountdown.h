#pragma once

#include "UI/Component.h"
#include "ui_DateCountdown.h"

#include <QTimer>

class DateCountdown : public Component
{
	Q_OBJECT

public:
	DateCountdown(QWidget* parent = nullptr);

	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

private slots:
	void Update();
	void UpdateCurrentTimes();
	void LiveToggled();

private:
	void UpdateResult();

	Ui::DateCountdownClass ui;
	QTimer updateTimer;
};
