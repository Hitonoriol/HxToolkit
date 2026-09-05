#include "HxNxToolkit.h"

#include <QApplication>

#include <QLocale>
#include <QTranslator>

#include <oclero/qlementine.hpp>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

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
    mainWindow.show();

    return a.exec();
}
