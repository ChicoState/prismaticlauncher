#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "LauncherConfig.h"
#include "LauncherWindow.h"

class LauncherTests : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        QFile style(":/ui/style.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyle("Fusion");
        qApp->setStyleSheet(style.readAll());
    }
    void defaultPath() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QCOMPARE(config.gameDirectory(),
                 QDir::homePath() + "/.local/share/Steam/steamapps/common/Stardew Valley");
        QVERIFY(!QFileInfo::exists(config.filePath()));
    }

    void persistPath() {
        QTemporaryDir temp;
        const auto file = temp.filePath("nested/launcher.ini");
        LauncherConfig config(file);
        QVERIFY(!config.saveGameDirectory("   "));
        QVERIFY(!config.saveGameDirectory("relative/game"));
        QVERIFY(config.saveGameDirectory(temp.filePath("Game with spaces")));
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.filePath("Game with spaces"));
        QCOMPARE(LauncherConfig::expandHome("~/Games"), QDir::homePath() + "/Games");
    }

    void invalidConfigDoesNotUseWorkingDirectory() {
        QTemporaryDir temp;
        const auto file = temp.filePath("launcher.ini");
        QSettings settings(file, QSettings::IniFormat);
        settings.setValue("game/directory", "relative/game");
        settings.sync();
        QCOMPARE(LauncherConfig(file).gameDirectory(),
                 QDir::homePath() + "/.local/share/Steam/steamapps/common/Stardew Valley");
    }

    void createSessionInstance() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        QTimer::singleShot(0, &window, [] {
            if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                dialog->findChild<QLineEdit*>()->setText("Test farm");
                dialog->accept();
            }
        });
        for (auto* button : window.findChildren<QPushButton*>()) {
            if (button->text() == "+  Add instance") button->click();
        }
        auto* instances = window.findChild<QListWidget*>("instances");
        QCOMPARE(instances->count(), 2);
        QCOMPARE(instances->currentItem()->data(Qt::UserRole).toString(), QString("Test farm"));
        QCOMPARE(window.findChild<QStackedWidget*>("pages")->currentIndex(), 1);
    }

    void navigateAndFilter() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        auto* instances = window.findChild<QListWidget*>("instances");
        auto* search = window.findChild<QLineEdit*>("instanceSearch");
        auto* pages = window.findChild<QStackedWidget*>("pages");
        QVERIFY(instances && search && pages);
        QCOMPARE(instances->count(), 1);
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QApplication::processEvents();
            QVERIFY(window.grab().save(screenshots + "/library.png"));
        }
        search->setText("does not exist");
        QVERIFY(instances->item(0)->isHidden());
        search->clear();
        QVERIFY(!instances->item(0)->isHidden());
        window.findChild<QPushButton*>("editInstance")->click();
        QCOMPARE(pages->currentIndex(), 1);
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QApplication::processEvents();
            QVERIFY(window.grab().save(screenshots + "/mods.png"));
        }
        window.findChild<QPushButton*>("backToLibrary")->click();
        QCOMPARE(pages->currentIndex(), 0);
    }

    void missingExecutable() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow window(config.filePath());
        bool warningShown = false;
        QTimer::singleShot(0, &window, [&warningShown] {
            if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                warningShown = warning->text().contains("Executable not found");
                warning->accept();
            }
        });
        for (auto* button : window.findChildren<QPushButton*>()) {
            if (button->text() == "▶  Play") button->click();
        }
        QVERIFY(warningShown);
    }

    void saveSettings() {
        QTemporaryDir temp;
        const auto file = temp.filePath("launcher.ini");
        LauncherWindow window(file);
        auto* path = window.findChild<QLineEdit*>("gamePath");
        QVERIFY(path);
        path->setText(temp.path());
        window.findChild<QPushButton*>("savePath")->click();
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.path());
        window.findChild<QPushButton*>("editInstance")->click();
        window.findChild<QListWidget*>("navigation")->setCurrentRow(2);
        window.resize(820, 600);
        window.show();
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QVERIFY(window.grab().save(screenshots + "/settings-minimum.png"));
        }
    }
};

QTEST_MAIN(LauncherTests)
#include "LauncherTests.moc"
