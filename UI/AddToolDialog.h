#pragma once

#include "UI/ComponentFactory.h"
#include "ui_AddToolDialog.h"

#include <QDialog>
#include <QIcon>

class QVBoxLayout;

class AddToolDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AddToolDialog(const QList<ToolInfo>& tools, bool verticalSplit, QWidget* parent = nullptr);

signals:
	void ToolSelected(ToolType toolType, bool verticalSplit);

protected:
	void showEvent(QShowEvent* event) override;

private:
	void UpdateResults(const QString& query);
	void AddToolButton(QVBoxLayout* layout, const ToolInfo& tool, bool showCategory = false);
	bool MatchesFuzzy(const QString& name, const QString& query) const;
	QIcon ToolIcon(ToolType toolType) const;

	QList<ToolInfo> tools;
	Ui::AddToolDialog ui;
};
