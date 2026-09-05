#include "MergeRequestEntry.h"

#include "Gitlab/MergeRequest.h"

#include <QHBoxLayout>
#include <QLabel>

MergeRequestEntry::MergeRequestEntry(const MergeRequest& mergeRequest, QWidget* parent)
	: QFrame(parent)
{
	setFrameShape(QFrame::StyledPanel);
	setStyleSheet("MergeRequestEntry { border: 1px solid palette(mid); border-radius: 5px; background: palette(base); }");
	auto layout = new QHBoxLayout(this);
	titleLabel = new QLabel(this);
	titleLabel->setOpenExternalLinks(true);
	titleLabel->setWordWrap(true);
	metadataLabel = new QLabel(this);
	statsLabel = new QLabel(this);
	auto titleLayout = new QVBoxLayout;
	titleLayout->addWidget(titleLabel);
	titleLayout->addWidget(metadataLabel);
	layout->addLayout(titleLayout, 1);
	layout->addWidget(statsLabel);
	UpdateMergeRequest(mergeRequest);
}

void MergeRequestEntry::UpdateMergeRequest(const MergeRequest& mergeRequest)
{
	titleLabel->setText(QString("<a href=\"%1\">%2</a> — <b>%3</b>").arg(mergeRequest.WebUrl.toHtmlEscaped(), mergeRequest.Reference.toHtmlEscaped(), mergeRequest.Title.toHtmlEscaped()));
	auto state = mergeRequest.State;
	if (state == "opened") {
		state = "Open";
	}
	else if (state == "merged") {
		state = "Merged";
	}
	else if (state == "closed") {
		state = "Closed";
	}

	metadataLabel->setText(QString("%1 · Created %2").arg(state, mergeRequest.CreatedAt.toLocalTime().toString("yyyy-MM-dd")));
	statsLabel->setText(mergeRequest.HasDiffStats ? QString("<span style=\"color: #3daee9\">+%1</span> <span style=\"color: #d65d5d\">-%2</span>").arg(mergeRequest.Additions).arg(mergeRequest.Deletions) : "Loading…");
}
