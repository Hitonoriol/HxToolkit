#include "Webview.h"

#ifdef _WIN32

#include "WebView2Widget.h"
#include "UI/ToolConfigurationParameter.h"

#include <QInputDialog>
#include <QJsonObject>
#include <QLineEdit>
#include <QUrl>
#include <QVBoxLayout>

Webview::Webview(QWidget* parent)
	: Component(parent, ToolType::Webview)
	, webview(new WebView2Widget(this))
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(webview);

	configurationParameters = {
		new ToolConfigurationParameter("Go to...", [this](QWidget* parent) { return EditUrl(parent); }, this),
		new ToolConfigurationParameter("User agent", [this](QWidget* parent) { return EditUserAgent(parent); }, this)
	};
}

QList<ToolConfigurationParameter*> Webview::GetConfigurationParameters() const
{
	return configurationParameters;
}

QJsonObject Webview::SaveState()
{
	auto state = Component::SaveState();
	state["Url"] = url;
	state["UserAgent"] = userAgent;
	return state;
}

void Webview::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	userAgent = state["UserAgent"].toString();
	webview->SetUserAgent(userAgent);

	url = state["Url"].toString();
	if (!url.isEmpty()) {
		webview->SetUrl(QUrl::fromUserInput(url));
	}
}

bool Webview::EditUrl(QWidget* parent)
{
	bool accepted = false;
	const auto newUrl = QInputDialog::getText(parent, "Webview", "Go to:", QLineEdit::Normal, url, &accepted).trimmed();
	if (!accepted || newUrl.isEmpty()) {
		return false;
	}

	url = newUrl;
	webview->SetUrl(QUrl::fromUserInput(url));
	return true;
}

bool Webview::EditUserAgent(QWidget* parent)
{
	bool accepted = false;
	const auto newUserAgent = QInputDialog::getText(parent, "Webview", "User agent:", QLineEdit::Normal, userAgent, &accepted);
	if (!accepted || newUserAgent == userAgent) {
		return false;
	}

	userAgent = newUserAgent;
	webview->SetUserAgent(userAgent);
	webview->Reload();
	return true;
}

#endif
