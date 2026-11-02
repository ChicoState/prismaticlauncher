#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
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
#include "settings.h"

class LauncherTests : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
        auto* addButton = window.findChild<QPushButton*>("addInstance");
        QVERIFY(addButton);
        addButton->click();
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

    void launcherSettingsOpensSeparateWindow() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        window.findChild<QListWidget*>("instances")->setCurrentRow(-1);

        auto* button = window.findChild<QPushButton*>("settings");
        QVERIFY(button);
        button->click();
        auto* settingsWindow = window.findChild<settings*>();
        QVERIFY(settingsWindow);
        QVERIFY(settingsWindow->isWindow());
        QVERIFY(settingsWindow->isVisible());
        QVERIFY(!window.findChild<InstanceWindow*>());
        auto* path = settingsWindow->findChild<QLineEdit*>("gameDirectoryValue");
        QVERIFY(path);
        QCOMPARE(path->text(), LauncherConfig(temp.filePath("launcher.ini")).gameDirectory());
        path->setText("unsaved edit");
        button->click();
        QCOMPARE(window.findChildren<settings*>().size(), 1);
        QCOMPARE(path->text(), QString("unsaved edit"));
        settingsWindow->findChild<QPushButton*>("backLibrary")->click();
        QVERIFY(!settingsWindow->isVisible());
        QVERIFY(window.isVisible());
        button->click();
        QCOMPARE(path->text(), LauncherConfig(temp.filePath("launcher.ini")).gameDirectory());
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
        instanceWindow->findChild<QListWidget*>("navigation")->setCurrentRow(2);
        auto* openSettings = instanceWindow->findChild<QPushButton*>("openSettings");
        QVERIFY(openSettings);
        openSettings->click();
        auto* settingsWindow = window.findChild<settings*>();
        QVERIFY(settingsWindow);
        QVERIFY(settingsWindow->isVisible());
        auto* path = settingsWindow->findChild<QLineEdit*>("gameDirectoryValue");
        QVERIFY(path);
        path->setText(temp.path());
        auto* save = settingsWindow->findChild<QPushButton*>("savePath");
        QVERIFY(save);
        save->click();
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.path());
        QVERIFY(instanceWindow->findChild<QLabel*>("pathStatus")->text().contains(temp.path()));
        auto* status = settingsWindow->findChild<QLabel*>("pathStatus");
        QVERIFY(status);
        QVERIFY(status->text().contains("Saved"));
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QVERIFY(settingsWindow->grab().save(screenshots + "/settings-window.png"));
        }
        settingsWindow->findChild<QPushButton*>("backLibrary")->click();
        QVERIFY(!instanceWindow->isVisible());
        LauncherWindow reopened(file);
        reopened.findChild<QPushButton*>("settings")->click();
        QCOMPARE(reopened.findChild<settings*>()->findChild<QLineEdit*>("gameDirectoryValue")->text(),
                 temp.path());
    }

    void invalidSettingsPathIsNotSaved_data() {
        QTest::addColumn<QString>("path");
        QTest::newRow("empty") << QString();
        QTest::newRow("blank") << QString("   ");
        QTest::newRow("relative") << QString("relative/game");
    }

    void invalidSettingsPathIsNotSaved() {
        QFETCH(QString, path);
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        settings settingsWindow(&config);
        settingsWindow.findChild<QLineEdit*>("gameDirectoryValue")->setText(path);
        settingsWindow.findChild<QPushButton*>("savePath")->click();
        QCOMPARE(config.gameDirectory(), temp.path());
        QVERIFY(settingsWindow.findChild<QLabel*>("pathStatus")->text().contains("absolute path"));
    }

    void settingsReportsUnwritableConfig() {
        QTemporaryDir temp;
        QFile blocker(temp.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        LauncherConfig config(temp.filePath("blocked/launcher.ini"));
        settings settingsWindow(&config);
        settingsWindow.findChild<QLineEdit*>("gameDirectoryValue")->setText(temp.path());
        settingsWindow.findChild<QPushButton*>("savePath")->click();
        QVERIFY(settingsWindow.findChild<QLabel*>("pathStatus")->text().contains("Could not save"));
        QVERIFY(!QFileInfo::exists(config.filePath()));
    }

    void browseSettingsDirectory() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        settings settingsWindow(&config);
        settingsWindow.show();
        bool dialogSeen = false;
        QTimer::singleShot(0, &settingsWindow, [&] {
            if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                dialogSeen = true;
                dialog->setDirectory(temp.path());
                dialog->selectFile(temp.path());
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
        });
        settingsWindow.findChild<QPushButton*>("browseDirectory")->click();
        QVERIFY(dialogSeen);
        const auto selected = settingsWindow.findChild<QLineEdit*>("gameDirectoryValue")->text();
        QCOMPARE(QFileInfo(selected).canonicalFilePath(), QFileInfo(temp.path()).canonicalFilePath());
        QVERIFY(!QFileInfo::exists(config.filePath()));
        QTimer::singleShot(0, &settingsWindow, [] {
            if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                dialog->reject();
            }
        });
        settingsWindow.findChild<QPushButton*>("browseDirectory")->click();
        QCOMPARE(settingsWindow.findChild<QLineEdit*>("gameDirectoryValue")->text(), selected);
    }

};

QTEST_MAIN(LauncherTests)
#include "LauncherTests.moc"
