#pragma once

#include <QLineEdit>

#include "System/KeystrokeListener.h"

class KeystrokeField : public QLineEdit
{
	Q_OBJECT

public:
	explicit KeystrokeField(QWidget* parent = nullptr);

	KeystrokeListener::KeyStroke GetStroke() const;
	void SetStroke(const KeystrokeListener::KeyStroke& stroke);

	static QString GetStrokeText(const KeystrokeListener::KeyStroke& stroke);

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

private:
	KeystrokeListener::KeyStroke stroke;
	KeystrokeListener::KeyStroke pressedKeys;
};
