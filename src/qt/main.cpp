#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QStandardPaths>

#include "LauncherWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Prismatic Launcher");
    QApplication::setOrganizationName("Prismatic");
    QApplication::setStyle("Fusion");
    QApplication::setFont(QFont("Sans Serif", 10));

    QCommandLineParser parser;
    parser.setApplicationDescription("Prismatic Launcher — Stardew Valley launcher UI");
    parser.addHelpOption();
    parser.addOption({"config", "Use an alternate INI configuration file.", "file"});
    parser.process(app);

    QFile stylesheet(":/ui/style.qss");
    if (stylesheet.open(QIODevice::ReadOnly)) app.setStyleSheet(stylesheet.readAll());

    const auto configFile =
        parser.isSet("config")
            ? QFileInfo(parser.value("config")).absoluteFilePath()
            : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/launcher.ini";
    LauncherWindow window(configFile);
    window.show();
    return QApplication::exec();
}
