#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QLabel>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

#include "InstanceWindow.h"
#include "LauncherConfig.h"
#include "ModInstallPage.h"

namespace {
void writeFile(const QString& path, const QByteArray& bytes) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
}
void makeMod(const QString& directory) {
    writeFile(directory + "/manifest.json",
              R"({"Name":"Cozy farm","Author":"Test","Version":"1.2.0",
        "Description":"Fixture","UniqueID":"Test.Cozy","EntryDll":"Cozy.dll"})");
    writeFile(directory + "/Cozy.dll", "fixture");
}
}  // namespace

class ModInstallTests : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        QFile style(":/ui/style.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyle("Fusion");
        qApp->setStyleSheet(style.readAll());
        qunsetenv("NEXUS_API_KEY");
    }

    void dropInstallsAndRefreshesLibrary() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        const auto game = temp.filePath("Game");
        writeFile(game + "/StardewValley.dll", "fixture");
        QVERIFY(config.saveGameDirectory(game));
        makeMod(temp.filePath("Cozy"));
        ModInstallPage page(&config);
        QSignalSpy completed(&page, &ModInstallPage::operationFinished);
        page.show();
        QTRY_VERIFY_WITH_TIMEOUT(!page.isBusy(), 5000);
        auto* table = page.findChild<QTableWidget*>("installedMods");
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 0);
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(temp.filePath("Cozy"))});
        QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction | Qt::MoveAction, &mime,
                              Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&page, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(20, 20), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(&page, &drop);
        QVERIFY(drop.isAccepted());
        QCOMPARE(drop.dropAction(), Qt::CopyAction);
        QVERIFY(page.isBusy());
        QVERIFY(!page.findChild<QPushButton*>("installModFiles")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!page.isBusy(), 5000);
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->text(), "Cozy farm");
        QVERIFY(QFileInfo::exists(game + "/Mods/Cozy/Cozy.dll"));
        QVERIFY(QFileInfo::exists(temp.filePath("Cozy/Cozy.dll")));
        QVERIFY(page.findChild<QPlainTextEdit*>("modInstallResult")
                    ->toPlainText()
                    .contains("Installed"));
        QVERIFY(completed.last().first().toBool());

        page.installSources({temp.filePath("Cozy")});
        QTRY_VERIFY_WITH_TIMEOUT(!page.isBusy(), 5000);
        QVERIFY(!completed.last().first().toBool());
        QCOMPARE(table->rowCount(), 1);
        QVERIFY(page.findChild<QPlainTextEdit*>("modInstallResult")
                    ->toPlainText()
                    .contains("already installed"));
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            page.resize(640, 500);
            QVERIFY(page.grab().save(screenshots + "/local-mods.png"));
        }
    }

    void rejectsRemoteAndMixedDrops() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        ModInstallPage page(&config);
        QMimeData mime;
        mime.setUrls(
            {QUrl::fromLocalFile(temp.filePath("Mod")), QUrl("https://example.com/mod.zip")});
        QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(&page, &enter);
        QVERIFY(!enter.isAccepted());
    }

    void invalidGameIsActionableAndPathChangesRefresh() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        ModInstallPage page(&config);
        page.show();
        QTRY_VERIFY_WITH_TIMEOUT(!page.isBusy(), 5000);
        auto* status = page.findChild<QLabel*>("modInstallStatus");
        QVERIFY(status);
        QVERIFY(status->text().contains("Settings"));
        const auto game = temp.filePath("Game");
        writeFile(game + "/StardewValley.dll", "fixture");
        makeMod(game + "/Mods/Existing");
        QVERIFY(config.saveGameDirectory(game));
        page.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!page.isBusy(), 5000);
        QCOMPARE(page.findChild<QTableWidget*>("installedMods")->rowCount(), 1);
        QVERIFY(page.findChild<QLabel*>("modInstallDestination")->text().contains(game));
    }

    void instanceWindowUsesSharedGameAndLogsInstallation() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        const auto game = temp.filePath("Game");
        writeFile(game + "/StardewValley.dll", "fixture");
        QVERIFY(config.saveGameDirectory(game));
        makeMod(temp.filePath("Cozy"));
        InstanceWindow window("Shared game", true, &config);
        window.show();
        auto* views = window.findChild<QTabWidget*>("modViews");
        QVERIFY(views);
        QCOMPARE(views->currentIndex(), 0);
        auto* page = window.findChild<ModInstallPage*>();
        QVERIFY(page);
        QTRY_VERIFY_WITH_TIMEOUT(!page->isBusy(), 5000);
        page->installSources({temp.filePath("Cozy/Cozy.dll")});
        QTRY_VERIFY_WITH_TIMEOUT(!page->isBusy(), 5000);
        QVERIFY(
            window.findChild<QPlainTextEdit*>("logs")->toPlainText().contains("Installed 1 mod"));
        window.setInstance("Another library entry", true);
        page->refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!page->isBusy(), 5000);
        QCOMPARE(page->findChild<QTableWidget*>("installedMods")->rowCount(), 1);
        window.resize(820, 600);
        QApplication::processEvents();
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QVERIFY(window.grab().save(screenshots + "/mod-install-window.png"));
        }
    }
};

QTEST_MAIN(ModInstallTests)
#include "ModInstallTests.moc"
