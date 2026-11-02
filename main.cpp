#include "HxNxToolkit.h"
#include "System/KeystrokeListener.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include <QLocale>
#include <QSharedMemory>
#include <QTranslator>

#include <oclero/qlementine.hpp>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
	const QString instanceKey = "HxNxToolkit.SingleInstance";
	QSharedMemory instanceLock(instanceKey);

	if (!instanceLock.create(1)) {
		QLocalSocket instanceSocket;
		instanceSocket.connectToServer(instanceKey);
		instanceSocket.waitForConnected(1'000);
		return 0;
	}

    KeystrokeListener keystrokeListener(&a);

    auto style = new oclero::qlementine::QlementineStyle(&a);
    style->setThemeJsonPath(":/themes/qlementine-dark.json");
    QApplication::setStyle(style);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = "hxnxtk_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    HxNxToolkit mainWindow;
	QLocalServer::removeServer(instanceKey);
	QLocalServer instanceServer;
	instanceServer.listen(instanceKey);
	QObject::connect(&instanceServer, &QLocalServer::newConnection, &mainWindow,
		[&instanceServer, &mainWindow] {
			while (auto connection = instanceServer.nextPendingConnection()) {
				connection->deleteLater();
			}

			mainWindow.ActivateWindow();
		});
    mainWindow.show();

    return a.exec();
}
