#pragma once

#include <QWidget>

#include <memory>

class QUrl;

class WebView2Widget : public QWidget
{
	Q_OBJECT

public:
	explicit WebView2Widget(QWidget* parent = nullptr);
	~WebView2Widget() override;

	void SetUrl(const QUrl& url);
	void SetUserAgent(const QString& userAgent);
	void Reload();

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	class Implementation;
	std::unique_ptr<Implementation> implementation;
};
