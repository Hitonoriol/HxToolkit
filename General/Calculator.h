#pragma once

#include <QWidget>
#include "ui_Calculator.h"

#include "UI/Component.h"

class Calculator : public Component
{
	Q_OBJECT

public:
	Calculator(QWidget *parent = nullptr);
	~Calculator();

	void OnPopOut() override;
	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

public slots:
	void InputTextChanged(QString text);

private:
	Ui::CalculatorClass ui;
};
