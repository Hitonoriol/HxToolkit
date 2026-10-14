#pragma once

#include "UI/Component.h"

class ToolConfigurationParameter;
class WebView2Widget;

class Webview : public Component
{
	Q_OBJECT

public:
	explicit Webview(QWidget* parent = nullptr);

	QList<ToolConfigurationParameter*> GetConfigurationParameters() const override;
	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private:
	bool EditUrl(QWidget* parent);
	bool EditUserAgent(QWidget* parent);

	WebView2Widget* webview;
	QString url;
	QString userAgent;
	QList<ToolConfigurationParameter*> configurationParameters;
};
