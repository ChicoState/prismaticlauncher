#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "InstanceWindow.h"
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
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        QVERIFY(instanceWindow->isVisible());
        QCOMPARE(instanceWindow->findChild<QLabel*>("detailTitle")->text(), QString("Test farm"));
    }

    void navigateAndFilter() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        auto* instances = window.findChild<QListWidget*>("instances");
        auto* search = window.findChild<QLineEdit*>("instanceSearch");
        QVERIFY(instances && search);
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
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        QVERIFY(instanceWindow->isVisible());
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QApplication::processEvents();
            QVERIFY(instanceWindow->grab().save(screenshots + "/mods.png"));
        }
        instanceWindow->findChild<QPushButton*>("backToLibrary")->click();
        QVERIFY(!instanceWindow->isVisible());
    }

    void instanceWindowUpdatesLibraryMode() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        auto* item = window.findChild<QListWidget*>("instances")->currentItem();

        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        instanceWindow->findChild<QComboBox*>("launchMode")->setCurrentIndex(1);

        QCOMPARE(item->data(Qt::UserRole + 1).toBool(), true);
        QCOMPARE(item->text(), QString("Stardew Valley\nModded"));
    }

    void launcherSettingsOpensInstanceSettingsTab() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));

        window.findChild<QPushButton*>("headerSettings")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        QCOMPARE(instanceWindow->findChild<QListWidget*>("navigation")->currentRow(), 2);
        QVERIFY(instanceWindow->isVisible());
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
        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        auto* path = instanceWindow->findChild<QLineEdit*>("gamePath");
        QVERIFY(path);
        path->setText(temp.path());
        instanceWindow->findChild<QPushButton*>("savePath")->click();
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.path());
        instanceWindow->findChild<QListWidget*>("navigation")->setCurrentRow(2);
        instanceWindow->resize(820, 600);
        instanceWindow->show();
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QVERIFY(instanceWindow->grab().save(screenshots + "/settings-minimum.png"));
        }
    }
};

QTEST_MAIN(LauncherTests)
#include "LauncherTests.moc"
