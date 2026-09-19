#pragma once

#include <QObject>
#include <QFutureWatcher>

struct ICMPPingResult
{
	bool online{};
	int roundTripTimeMs{};
};

class ICMPPinger : public QObject
{
	Q_OBJECT

public:
	explicit ICMPPinger(QObject* parent = nullptr);
	void Ping(const QString& host, int timeoutMs = 3'000);
	bool IsRunning() const;

signals:
	void PingFinished(const ICMPPingResult& result);

private:
	QFutureWatcher<ICMPPingResult> watcher;
};
