#pragma once

#include <QPixmap>
#include <QKeyEvent>
#include <QTimer>
#include <QWidget>

#include <vector>

class MeltingScreenSaver : public QWidget
{
public:
	static void Start();

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

private:
	struct Drip
	{
		qreal flow{};
		qreal acceleration{};
		qreal phase{};
	};

	MeltingScreenSaver(const QRect& geometry, QPixmap screenshot);
	void AdvanceMelt();
	static void Finish();

	QPixmap screenshot;
	std::vector<Drip> drips;
	QTimer meltTimer;

	static std::vector<MeltingScreenSaver*> activeSavers;
};
