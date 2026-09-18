#include "System/KeystrokeEditorDialog.h"

#include "System/KeystrokeField.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

KeystrokeEditorDialog::KeystrokeEditorDialog(const KeystrokeListener::Chord& chord, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Set panic shortcut");
	auto layout = new QVBoxLayout(this);
	strokeLayout = new QVBoxLayout;
	layout->addLayout(strokeLayout);

	for (const auto& stroke : chord) {
		AddStroke(stroke);
	}

	if (fields.isEmpty()) {
		AddStroke();
	}

	auto addButton = new QPushButton("Add chord stroke", this);
	connect(addButton, &QPushButton::clicked, this, static_cast<void (KeystrokeEditorDialog::*)()>(&KeystrokeEditorDialog::AddStroke));
	layout->addWidget(addButton);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
	connect(buttons->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

KeystrokeListener::Chord KeystrokeEditorDialog::GetChord() const
{
	KeystrokeListener::Chord chord;
	for (auto field : fields) {
		auto stroke = field->GetStroke();
		if (stroke.isEmpty()) {
			return {};
		}

		chord.append(stroke);
	}

	return chord;
}

QString KeystrokeEditorDialog::GetChordText(const KeystrokeListener::Chord& chord)
{
	QStringList strokes;
	for (const auto& stroke : chord) {
		strokes.append("[" + KeystrokeField::GetStrokeText(stroke) + "]");
	}

	return strokes.join(" + ");
}

void KeystrokeEditorDialog::AddStroke()
{
	AddStroke({});
}

void KeystrokeEditorDialog::RemoveStroke()
{
	if (fields.size() == 1) {
		return;
	}

	auto button = qobject_cast<QPushButton*>(sender());
	auto row = button->parentWidget();
	auto field = row->findChild<KeystrokeField*>();
	fields.removeOne(field);
	strokeLayout->removeWidget(row);
	row->deleteLater();
}

void KeystrokeEditorDialog::AddStroke(const KeystrokeListener::KeyStroke& stroke)
{
	auto row = new QWidget(this);
	auto layout = new QHBoxLayout(row);
	layout->setContentsMargins({});
	auto field = new KeystrokeField(row);
	field->SetStroke(stroke);
	layout->addWidget(field);
	auto removeButton = new QPushButton("Remove", row);
	connect(removeButton, &QPushButton::clicked, this, &KeystrokeEditorDialog::RemoveStroke);
	layout->addWidget(removeButton);
	strokeLayout->addWidget(row);
	fields.append(field);
}
