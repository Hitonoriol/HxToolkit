#pragma once

#include <QDialog>

#include "System/KeystrokeListener.h"

class QVBoxLayout;
class KeystrokeField;

class KeystrokeEditorDialog : public QDialog
{
	Q_OBJECT

public:
	explicit KeystrokeEditorDialog(const KeystrokeListener::Chord& chord, QWidget* parent = nullptr);

	KeystrokeListener::Chord GetChord() const;
	static QString GetChordText(const KeystrokeListener::Chord& chord);

private slots:
	void AddStroke();
	void RemoveStroke();

private:
	void AddStroke(const KeystrokeListener::KeyStroke& stroke);

	QVBoxLayout* strokeLayout;
	QList<KeystrokeField*> fields;
};
