#pragma once

#include <QFrame>

class QLabel;
struct MergeRequest;

class MergeRequestEntry : public QFrame
{
	Q_OBJECT

public:
	MergeRequestEntry(const MergeRequest& mergeRequest, QWidget* parent = nullptr);

	void UpdateMergeRequest(const MergeRequest& mergeRequest);

private:
	QLabel* titleLabel;
	QLabel* metadataLabel;
	QLabel* statsLabel;
};
