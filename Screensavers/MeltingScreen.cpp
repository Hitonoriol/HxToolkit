#include "MeltingScreen.h"

#include "Screensavers/MeltingScreenSaver.h"

#include <QSignalBlocker>
#include <QTime>

#include <algorithm>

MeltingScreen::MeltingScreen(QWidget* parent)
	: Component(parent, ToolType::MeltingScreen)
{
	ui.setupUi(this);
	ui.IdleDelayEdit->setMinimumTime(QTime(0, 0, 1));
	ui.IdleDelayEdit->setMaximumTime(QTime(0, 59, 59));
	ui.IdleDelayEdit->setTime(QTime(0, 0, 5));
	connect(&idleService, &IdleService::idleTime, this, &MeltingScreen::OnIdleTime);
	connect(ui.IdleEnabledSwitch, &oclero::qlementine::Switch::toggled, this, &MeltingScreen::OnSettingsChanged);
	connect(ui.IdleDelayEdit, &QTimeEdit::timeChanged, this, &MeltingScreen::OnSettingsChanged);
}

QJsonObject MeltingScreen::SaveState()
{
	auto state = Component::SaveState();
	state["IdleEnabled"] = ui.IdleEnabledSwitch->isChecked();
	state["IdleDelaySeconds"] = static_cast<qint64>(IdleDelaySeconds());
	return state;
}

void MeltingScreen::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	const QSignalBlocker switchBlocker(ui.IdleEnabledSwitch);
	const QSignalBlocker delayBlocker(ui.IdleDelayEdit);
	ui.IdleEnabledSwitch->setChecked(state["IdleEnabled"].toBool(false));
	const auto delaySeconds = state["IdleDelaySeconds"].toInt(5);
	ui.IdleDelayEdit->setTime(QTime(0, 0).addSecs(std::clamp(delaySeconds, 1, 3'599)));
}

void MeltingScreen::StartScreenSaver()
{
	MeltingScreenSaver::Start();
}

void MeltingScreen::OnIdleTime(uint64_t seconds)
{
	if (ui.IdleEnabledSwitch->isChecked() && seconds >= IdleDelaySeconds()) {
		MeltingScreenSaver::Start();
	}
}

void MeltingScreen::OnSettingsChanged()
{
	emit Modified(this);
}

uint64_t MeltingScreen::IdleDelaySeconds() const
{
	return static_cast<uint64_t>(QTime(0, 0).secsTo(ui.IdleDelayEdit->time()));
}
