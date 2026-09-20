#pragma once

#include <QObject>
#include <QTimer>

#include <cstdint>

class IdleService : public QObject
{
	Q_OBJECT

public:
	explicit IdleService(QObject* parent = nullptr);

signals:
	void idleTime(uint64_t seconds);

private:
	void CheckIdleTime();

	QTimer timer;
};
