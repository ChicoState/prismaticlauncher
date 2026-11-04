#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "GameDirectoryDetector.h"
#include "LauncherConfig.h"
#include "settings.h"

namespace {
bool writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

bool createGame(const QString& path) {
    const QDir directory(path);
#ifdef Q_OS_WIN
    const auto executable = QString("Stardew Valley.exe");
#elif defined(Q_OS_MACOS)
    const auto executable = QString("Contents/MacOS/StardewValley");
#else
    const auto executable = QString("StardewValley");
#endif
    return QDir().mkpath(QFileInfo(directory.filePath(executable)).absolutePath()) &&
           writeFile(directory.filePath(executable), {});
}

QByteArray vdfPath(QString path) {
    path.replace("\\", "\\\\");
    path.replace("\"", "\\\"");
    return path.toUtf8();
}
}  // namespace

class SettingsDetectionTests : public QObject {
    Q_OBJECT

   private slots:
    void detectsDefaultSteamLibrary() {
        QTemporaryDir temp;
        const auto game = temp.filePath("Steam/steamapps/common/Stardew Valley");
        QVERIFY(createGame(game));
        QCOMPARE(GameDirectoryDetector({temp.filePath("Steam")}).detect(), game);
    }

    void prefersCurrentValidInstallation() {
        QTemporaryDir temp;
        const auto preferred = temp.filePath("My Game");
        const auto other = temp.filePath("Steam/steamapps/common/Stardew Valley");
        QVERIFY(createGame(preferred));
        QVERIFY(createGame(other));
        QCOMPARE(GameDirectoryDetector({temp.filePath("Steam")}).detect(preferred), preferred);
    }

    void detectsAdditionalSteamLibrary_data() {
        QTest::addColumn<bool>("legacy");
        QTest::addColumn<QString>("manifest");
        QTest::newRow("modern") << false << "steamapps/libraryfolders.vdf";
        QTest::newRow("legacy") << true << "steamapps/libraryfolders.vdf";
        QTest::newRow("config-copy") << false << "config/libraryfolders.vdf";
    }

    void detectsAdditionalSteamLibrary() {
        QFETCH(bool, legacy);
        QFETCH(QString, manifest);
        QTemporaryDir temp;
        const auto steam = temp.filePath("Steam");
        const auto library = temp.filePath("Extra Library é");
        const auto game = QDir(library).filePath("steamapps/common/Stardew Valley");
        QVERIFY(createGame(game));
        QVERIFY(QDir(steam).mkpath(QFileInfo(manifest).path()));
        const auto entry = legacy ? "\"1\" \"" + vdfPath(library) + "\""
                                  : "\"1\" { \"path\" \"" + vdfPath(library) + "\" }";
        QVERIFY(writeFile(QDir(steam).filePath(manifest), "\"libraryfolders\" { " + entry + " }"));
        QCOMPARE(GameDirectoryDetector({steam}).detect(), game);
    }

    void skipsMissingAndInvalidCandidates() {
        QTemporaryDir temp;
        const auto game = temp.filePath("GOG Game");
        QVERIFY(createGame(game));
        const GameDirectoryDetector detector({temp.filePath("Missing Steam")}, {temp.path(), game});
        QCOMPARE(detector.detect(temp.filePath("Missing Game")), game);
        QVERIFY(GameDirectoryDetector({}, {temp.path()}).detect().isEmpty());
        QVERIFY(GameDirectoryDetector({}, {"relative/game", ""}).detect(" ").isEmpty());
    }

    void ignoresMalformedOrOversizedLibraryList() {
        QTemporaryDir temp;
        QVERIFY(QDir(temp.path()).mkpath("steamapps"));
        const auto manifest = temp.filePath("steamapps/libraryfolders.vdf");
        const GameDirectoryDetector detector({temp.path()});
        QVERIFY(writeFile(manifest, "\"path\" \"relative/library\"\nmalformed"));
        QVERIFY(detector.detect().isEmpty());
        QVERIFY(writeFile(manifest, QByteArray(1024 * 1024 + 1, 'x')));
        QVERIFY(detector.detect().isEmpty());
    }

#ifdef Q_OS_MACOS
    void normalizesMacExecutableDirectoryToBundleRoot() {
        QTemporaryDir temp;
        const auto game = temp.filePath("Stardew Valley.app");
        QVERIFY(createGame(game));
        const GameDirectoryDetector detector(QStringList{});
        QCOMPARE(detector.detect(game + "/Contents/MacOS"), game);
    }
#endif

    void detectButtonFillsPathWithoutSaving() {
        QTemporaryDir temp;
        const auto game = temp.filePath("Steam/steamapps/common/Stardew Valley");
        QVERIFY(createGame(game));
        LauncherConfig config(temp.filePath("launcher.ini"));
        settings window(&config, nullptr, GameDirectoryDetector({temp.filePath("Steam")}));
        QSignalSpy saved(&window, &settings::gameDirectoryChanged);
        window.findChild<QPushButton*>("detectDirectory")->click();
        QCOMPARE(window.findChild<QLineEdit*>("gameDirectoryValue")->text(), game);
        QVERIFY(
            window.findChild<QLabel*>("pathStatus")->text().contains("Detected game directory"));
        QVERIFY(!QFileInfo::exists(config.filePath()));
        QCOMPARE(saved.count(), 0);
        window.findChild<QPushButton*>("savePath")->click();
        QCOMPARE(config.gameDirectory(), game);
        QCOMPARE(saved.count(), 1);
    }

    void failedDetectionKeepsPathAndSettings() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        settings window(&config, nullptr, GameDirectoryDetector(QStringList{}));
        auto* path = window.findChild<QLineEdit*>("gameDirectoryValue");
        path->setText(temp.filePath("unsaved"));
        window.findChild<QPushButton*>("detectDirectory")->click();
        QCOMPARE(path->text(), temp.filePath("unsaved"));
        QCOMPARE(config.gameDirectory(), temp.path());
        QVERIFY(window.findChild<QLabel*>("pathStatus")->text().contains("Could not detect"));
    }
};

QTEST_MAIN(SettingsDetectionTests)
#include "SettingsDetectionTests.moc"
