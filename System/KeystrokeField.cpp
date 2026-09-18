#include "System/KeystrokeField.h"

#include <QKeyEvent>
#include <QKeySequence>

#include <algorithm>

KeystrokeField::KeystrokeField(QWidget* parent)
	: QLineEdit(parent)
{
	setReadOnly(true);
}

KeystrokeListener::KeyStroke KeystrokeField::GetStroke() const
{
	return stroke;
}

void KeystrokeField::SetStroke(const KeystrokeListener::KeyStroke& newStroke)
{
	stroke = newStroke;
	setText(GetStrokeText(stroke));
}

QString KeystrokeField::GetStrokeText(const KeystrokeListener::KeyStroke& stroke)
{
	QList<int> keys;
	for (auto key : stroke) {
		keys.append(static_cast<int>(key));
	}

	std::sort(keys.begin(), keys.end());
	QStringList names;
	for (auto key : keys) {
		names.append(QKeySequence(key).toString(QKeySequence::NativeText));
	}

	return names.join(" + ");
}

void KeystrokeField::keyPressEvent(QKeyEvent* event)
{
	if (event->isAutoRepeat()) {
		return;
	}

	pressedKeys.insert(static_cast<Qt::Key>(event->key()));
	stroke = pressedKeys;
	setText(GetStrokeText(stroke));
}

void KeystrokeField::keyReleaseEvent(QKeyEvent* event)
{
	if (event->isAutoRepeat()) {
		return;
	}

	pressedKeys.remove(static_cast<Qt::Key>(event->key()));
}
