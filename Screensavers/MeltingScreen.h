#pragma once

#include "UI/Component.h"
#include "System/IdleService.h"
#include "ui_MeltingScreen.h"

#include <QObject>

class MeltingScreen : public Component
{
	Q_OBJECT

public:
	explicit MeltingScreen(QWidget* parent = nullptr);
	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private slots:
	void StartScreenSaver();
	void OnIdleTime(uint64_t seconds);
	void OnSettingsChanged();

private:
	uint64_t IdleDelaySeconds() const;

	Ui::MeltingScreenClass ui;
	IdleService idleService;
};
