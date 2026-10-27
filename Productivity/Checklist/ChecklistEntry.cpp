#include "ChecklistEntry.h"

ChecklistEntry::ChecklistEntry(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
}

ChecklistEntry::~ChecklistEntry()
{}

void ChecklistEntry::SetChecked(bool checked)
{
	ui.CheckBox->setChecked(checked);
}

bool ChecklistEntry::IsChecked() const
{
	return ui.CheckBox->isChecked();
}

void ChecklistEntry::SetDescription(const QString& description)
{
	ui.DescriptionField->setText(description);
}

QString ChecklistEntry::GetDescription() const
{
	return ui.DescriptionField->text();
}

void ChecklistEntry::OnDeletePressed()
{
	setParent(nullptr);
	emit Deleted();
	deleteLater();
}

void ChecklistEntry::OnCheckboxValueChange(bool value)
{
	ui.DescriptionField->setEnabled(!value);
	emit CheckboxValueChanged(value);
}

void ChecklistEntry::OnDescriptionChange(QString text)
{
	emit DescriptionChanged(text);
}
