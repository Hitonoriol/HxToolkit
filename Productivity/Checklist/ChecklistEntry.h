#pragma once

#include <QWidget>
#include "ui_ChecklistEntry.h"

class ChecklistEntry : public QWidget
{
	Q_OBJECT

public:
	ChecklistEntry(QWidget *parent = nullptr);
	~ChecklistEntry();

	void SetChecked(bool checked);
	bool IsChecked() const;

	void SetDescription(const QString& description);
	QString GetDescription() const;

signals:
	void CheckboxValueChanged(bool value);
	void DescriptionChanged(QString text);
	void Deleted();

private slots:
	void OnCheckboxValueChange(bool value);
	void OnDescriptionChange(QString text);

	void OnDeletePressed();

private:
	Ui::ChecklistEntryClass ui;
};
