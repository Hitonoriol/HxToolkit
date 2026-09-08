#include "GitlabTimeStats.h"

#include "Gitlab/GitlabClient.h"
#include "Gitlab/WorkItem.h"

#include <QComboBox>
#include <QDateTime>
#include <QFontMetrics>
#include <QLabel>
#include <QProgressBar>
#include <QTimeZone>

GitlabTimeStats::GitlabTimeStats(QWidget* parent)
	: Component(parent, ToolType::GitlabTimeStats)
{
	ui.setupUi(this);
	gitlabClient = new GitlabClient(this);
	connect(ui.PeriodComboBox, &QComboBox::currentIndexChanged, this, &GitlabTimeStats::RefreshTimeStats);
	connect(ui.GroupComboBox, &QComboBox::currentIndexChanged, this, &GitlabTimeStats::OnTimeStatsUpdated);
	connect(gitlabClient, &GitlabClient::TimeStatsUpdated, this, &GitlabTimeStats::OnTimeStatsUpdated);
	connect(gitlabClient, &GitlabClient::requestFailed, this, &GitlabTimeStats::OnGitlabRequestFailed);
	RefreshTimeStats();
}

QSize GitlabTimeStats::sizeHint() const
{
	return QSize(600, 450);
}

QJsonObject GitlabTimeStats::SaveState()
{
	auto state = Component::SaveState();
	state["Period"] = ui.PeriodComboBox->currentIndex();
	state["Grouping"] = ui.GroupComboBox->currentIndex();
	return state;
}

void GitlabTimeStats::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	ui.PeriodComboBox->setCurrentIndex(state["Period"].toInt());
	ui.GroupComboBox->setCurrentIndex(state["Grouping"].toInt());
}

void GitlabTimeStats::RefreshTimeStats()
{
	ui.StatusLabel->setText("Retrieving time statistics...");
	ui.TotalLabel->setText({});
	gitlabClient->getTimeStats(getPeriodStart(), getPeriodEnd());
}

void GitlabTimeStats::OnTimeStatsUpdated()
{
	std::map<QString, int64_t> stats;
	int64_t totalSeconds{};
	for (auto& workItem : gitlabClient->getTimeStats()) {
		auto seconds = workItem.GitlabElapsedSeconds;
		if (seconds <= 0) {
			continue;
		}

		auto name = ui.GroupComboBox->currentIndex() == 0 ? workItem.Project : workItem.Reference + " " + workItem.Title;
		stats[name] += seconds;
		totalSeconds += seconds;
	}

	ui.StatusLabel->setText(QString("%1 work items modified in this period").arg(gitlabClient->getTimeStats().size()));
	ui.TotalLabel->setText("Total tracked: " + GetDurationText(totalSeconds));
	UpdateChart(stats, totalSeconds);
}

void GitlabTimeStats::OnGitlabRequestFailed(const QString& errorMessage)
{
	ui.StatusLabel->setText("Refresh failed: " + errorMessage);
}

QDateTime GitlabTimeStats::getPeriodStart() const
{
	auto now = QDateTime::currentDateTime();
	switch (ui.PeriodComboBox->currentIndex()) {
	case 0:
		return QDateTime(now.date().addDays(1 - now.date().dayOfWeek()), QTime(), QTimeZone::systemTimeZone());

	case 1:
		return QDateTime(now.date().addDays(1 - now.date().dayOfWeek() - 7), QTime(), QTimeZone::systemTimeZone());

	default:
		return now.addMonths(-1);
	}
}

QDateTime GitlabTimeStats::getPeriodEnd() const
{
	auto now = QDateTime::currentDateTime();
	if (ui.PeriodComboBox->currentIndex() == 1) {
		return QDateTime(now.date().addDays(1 - now.date().dayOfWeek()), QTime(), QTimeZone::systemTimeZone());
	}

	return now;
}

void GitlabTimeStats::UpdateChart(const std::map<QString, int64_t>& stats, int64_t totalSeconds)
{
	while (auto item = ui.ChartLayout->takeAt(0)) {
		if (auto widget = item->widget()) {
			delete widget;
		}
		else if (auto layout = item->layout()) {
			while (auto child = layout->takeAt(0)) {
				delete child->widget();
				delete child;
			}
		}

		delete item;
	}

	for (auto& [name, seconds] : stats) {
		auto label = new QLabel(ui.ChartContents);
		label->setFixedWidth(220);
		label->setText(QFontMetrics(label->font()).elidedText(name, Qt::ElideRight, label->width()));
		label->setToolTip(name);
		auto bar = new QProgressBar(ui.ChartContents);
		bar->setRange(0, 1000);
		bar->setValue(totalSeconds > 0 ? static_cast<int>(seconds * 1000 / totalSeconds) : 0);
		bar->setTextVisible(false);
		auto duration = new QLabel(GetDurationText(seconds), ui.ChartContents);
		duration->setMinimumWidth(75);
		auto row = new QWidget(ui.ChartContents);
		auto rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->addWidget(label);
		rowLayout->addWidget(bar);
		rowLayout->addWidget(duration);
		ui.ChartLayout->addWidget(row);
	}

	ui.ChartLayout->addStretch();
}

QString GitlabTimeStats::GetDurationText(int64_t seconds)
{
	auto hours = seconds / 3'600;
	auto minutes = (seconds / 60) % 60;
	return QString("%1h %2m").arg(hours).arg(minutes, 2, 10, QChar('0'));
}
