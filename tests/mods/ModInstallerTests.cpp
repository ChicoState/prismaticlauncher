#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "ModInstaller.h"

namespace {
void writeFile(const QString& path, const QByteArray& data) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

void makeMod(const QString& path, const QString& id = "Test.Mod", bool pack = false) {
    QJsonObject manifest{{"Name", "Test mod"},
                         {"Author", "Test"},
                         {"Version", "1.0.0"},
                         {"Description", "Fixture"},
                         {"UniqueID", id}};
    if (pack)
        manifest["ContentPackFor"] = QJsonObject{{"UniqueID", "Pathoschild.ContentPatcher"}};
    else
        manifest["EntryDll"] = "Test.dll";
    writeFile(path + "/manifest.json", QJsonDocument(manifest).toJson());
    writeFile(path + (pack ? "/content.json" : "/Test.dll"), "fixture");
    writeFile(path + "/assets/sprite.png", "asset bytes");
}

QByteArray manifestBytes(const QString& directory) {
    QFile file(directory + "/manifest.json");
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

void makeArchive(const QString& path, const QList<QPair<QString, QByteArray>>& files,
                 const QString& link = {}) {
    auto* writer = archive_write_new();
    if (path.endsWith(".7z"))
        QCOMPARE(archive_write_set_format_7zip(writer), ARCHIVE_OK);
    else
        QCOMPARE(archive_write_set_format_zip(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_open_filename(writer, QFile::encodeName(path).constData()), ARCHIVE_OK);
    for (const auto& [name, data] : files) {
        auto* entry = archive_entry_new();
        archive_entry_set_pathname_utf8(entry, name.toUtf8().constData());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_entry_set_size(entry, data.size());
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        QCOMPARE(archive_write_data(writer, data.constData(), data.size()), data.size());
        archive_entry_free(entry);
    }
    if (!link.isEmpty()) {
        auto* entry = archive_entry_new();
        archive_entry_set_pathname(entry, "Mod/link");
        archive_entry_set_filetype(entry, AE_IFLNK);
        archive_entry_set_symlink_utf8(entry, link.toUtf8().constData());
        archive_entry_set_size(entry, 0);
        QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
        archive_entry_free(entry);
    }
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);
}

QString makeGame(QTemporaryDir& temp) {
    const auto game = temp.filePath("Game with spaces");
    writeFile(game + "/StardewValley.dll", "fixture");
    return game;
}
}  // namespace

class ModInstallerTests : public QObject {
    Q_OBJECT
   private slots:
    void bundledSmapiManifestDoesNotBlockInstallation() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto console = game + "/Mods/ConsoleCommands";
        const QByteArray manifest = R"({
            "Name": "Console Commands",
            "Author": "SMAPI",
            "Version": "4.5.2",
            "Description": "Adds SMAPI console commands that let you manipulate the game.",
            "UniqueId": "SMAPI.ConsoleCommands",
            "EntryDll": "ConsoleCommands.dll",
            "MinimumApiVersion": "4.5.2"
        })";
        writeFile(console + "/manifest.json", manifest);
        writeFile(console + "/ConsoleCommands.dll", "fixture");
        const auto listing = ModInstaller::installed(game);
        QVERIFY2(listing.succeeded(), qPrintable(listing.error));
        QCOMPARE(listing.mods.size(), 1);
        QCOMPARE(listing.mods.first().uniqueId, "SMAPI.ConsoleCommands");

        makeMod(temp.filePath("NewMod"));
        const auto result = ModInstaller::install({temp.filePath("NewMod")}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(QFileInfo::exists(game + "/Mods/NewMod/Test.dll"));
        QCOMPARE(manifestBytes(console), manifest);

        makeMod(temp.filePath("Duplicate"), "SMAPI.ConsoleCommands");
        const auto duplicate = ModInstaller::install({temp.filePath("Duplicate")}, game);
        QVERIFY(!duplicate.succeeded());
        QVERIFY(duplicate.error.contains("already installed"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/Duplicate"));
    }

    void manifestFieldNamesAreCaseInsensitive_data() {
        QTest::addColumn<bool>("pack");
        QTest::newRow("code-mod") << false;
        QTest::newRow("content-pack") << true;
    }

    void manifestFieldNamesAreCaseInsensitive() {
        QFETCH(bool, pack);
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto source = temp.filePath("MixedCase");
        makeMod(source, "Test.MixedCase", pack);
        const auto original = QJsonDocument::fromJson(manifestBytes(source)).object();
        QJsonObject lowercase;
        for (auto it = original.begin(); it != original.end(); ++it) {
            lowercase.insert(it.key().toLower(), it.value());
        }
        if (pack) {
            lowercase["contentpackfor"] = QJsonObject{{"uniqueId", "Pathoschild.ContentPatcher"}};
        }
        writeFile(source + "/manifest.json", QJsonDocument(lowercase).toJson());
        const auto result = ModInstaller::install({source}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.size(), 1);
        QCOMPARE(result.mods.first().uniqueId, "Test.MixedCase");
        const auto listing = ModInstaller::installed(game);
        QVERIFY2(listing.succeeded(), qPrintable(listing.error));
        QCOMPARE(listing.mods.first().uniqueId, "Test.MixedCase");
    }

    void installsCompleteFolderAndListsFromDisk() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto source = temp.filePath("download/MyMod");
        makeMod(source);
        const auto result = ModInstaller::install({source}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.size(), 1);
        QVERIFY(QFileInfo::exists(game + "/Mods/MyMod/assets/sprite.png"));
        QVERIFY(QFileInfo::exists(source + "/Test.dll"));
        const auto listing = ModInstaller::installed(game);
        QVERIFY2(listing.succeeded(), qPrintable(listing.error));
        QCOMPARE(listing.mods.size(), 1);
        QCOMPARE(listing.mods.first().uniqueId, "Test.Mod");
    }

    void findsModsInsideDownloadWrappers() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto source = temp.filePath("download-123-456/Mods");
        makeMod(source + "/Code", "Test.Code");
        makeMod(source + "/[CP] Pack", "Test.Pack", true);
        writeFile(source + "/README.txt", "ignored wrapper documentation");
        const auto result = ModInstaller::install({QFileInfo(source).absolutePath()}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.size(), 2);
        QVERIFY(QFileInfo::exists(game + "/Mods/[CP] Pack/content.json"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/README.txt"));
    }

    void individualFileResolvesItsModFolder() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto source = temp.filePath("download/Pack");
        makeMod(source, "Test.Pack", true);
        const auto result = ModInstaller::install({source + "/assets/sprite.png"}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(QFileInfo::exists(game + "/Mods/Pack/manifest.json"));
        QVERIFY(QFileInfo::exists(game + "/Mods/Pack/content.json"));
    }

    void repeatedFilesFromOneModAreDeduplicated() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        const auto source = temp.filePath("download/Mod");
        makeMod(source);
        const auto result =
            ModInstaller::install({source + "/manifest.json", source + "/Test.dll"}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.size(), 1);
    }

    void rejectsDuplicateIdentityInDifferentFolderWithoutOverwriting() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(game + "/Mods/Group/AlreadyInstalled");
        const auto source = temp.filePath("download/DifferentName");
        makeMod(source);
        const auto result = ModInstaller::install({source}, game);
        QVERIFY(!result.succeeded());
        QVERIFY(result.error.contains("Test.Mod"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/DifferentName"));
        QVERIFY(QFileInfo::exists(game + "/Mods/Group/AlreadyInstalled/Test.dll"));
    }

    void validatesWholeBatchBeforeInstalling() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Good"));
        writeFile(temp.filePath("loose.xnb"), "not a SMAPI package");
        const auto result =
            ModInstaller::install({temp.filePath("Good"), temp.filePath("loose.xnb")}, game);
        QVERIFY(!result.succeeded());
        QVERIFY(!QFileInfo::exists(game + "/Mods/Good"));
    }

    void rejectsMissingEntryDll() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Broken"));
        QVERIFY(QFile::remove(temp.filePath("Broken/Test.dll")));
        const auto result = ModInstaller::install({temp.filePath("Broken")}, game);
        QVERIFY(!result.succeeded());
        QVERIFY(!QFileInfo::exists(game + "/Mods/Broken"));
    }

    void rejectsInvalidGame() {
        QTemporaryDir temp;
        makeMod(temp.filePath("Mod"));
        const auto result = ModInstaller::install({temp.filePath("Mod")}, temp.path());
        QVERIFY(!result.succeeded());
        QVERIFY(!QFileInfo::exists(temp.filePath("Mods")));
    }

    void installsArchives_data() {
        QTest::addColumn<QString>("extension");
        QTest::addColumn<QString>("prefix");
        QTest::newRow("nexus-zip") << ".zip" << "download-123/Mod/";
        QTest::newRow("7zip") << ".7z" << "Mod/";
        QTest::newRow("root-manifest") << ".zip" << "";
        QTest::newRow("windows-separators") << ".zip" << "download\\Mod\\";
    }

    void installsArchives() {
        QFETCH(QString, extension);
        QFETCH(QString, prefix);
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Fixture"));
        const auto archive = temp.filePath("Nexus-123-1-0" + extension);
        makeArchive(archive, {{prefix + "manifest.json", manifestBytes(temp.filePath("Fixture"))},
                              {prefix + "Test.dll", "DLL"},
                              {prefix + "assets/sprite.png", "sprite"}});
        const auto result = ModInstaller::install({archive}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.size(), 1);
        const auto folder = prefix.isEmpty() ? "Test.Mod" : "Mod";
        QVERIFY(QFileInfo::exists(game + "/Mods/" + folder + "/assets/sprite.png"));
        QVERIFY(QFileInfo::exists(archive));
    }

    void rejectsUnsafeArchiveEntries_data() {
        QTest::addColumn<QString>("path");
        QTest::newRow("traversal") << "../outside.txt";
        QTest::newRow("absolute") << "/tmp/outside.txt";
        QTest::newRow("drive") << "C:/outside.txt";
        QTest::newRow("backslash-traversal") << "Mod\\..\\..\\outside.txt";
        QTest::newRow("windows-device") << "Mod/CON.txt";
        QTest::newRow("alternate-stream") << "Mod/file:stream";
    }

    void rejectsUnsafeArchiveEntries() {
        QFETCH(QString, path);
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Fixture"));
        makeArchive(temp.filePath("bad.zip"),
                    {{"Mod/manifest.json", manifestBytes(temp.filePath("Fixture"))},
                     {"Mod/Test.dll", "fixture"},
                     {path, "malicious"}});
        const auto result = ModInstaller::install({temp.filePath("bad.zip")}, game);
        QVERIFY(!result.succeeded());
        QVERIFY(!QFileInfo::exists(game + "/Mods/Mod"));
        QVERIFY(!QFileInfo::exists(temp.filePath("outside.txt")));
    }

    void rejectsArchiveLinksAndCaseCollisions() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Fixture"));
        const QList<QPair<QString, QByteArray>> files{
            {"Mod/manifest.json", manifestBytes(temp.filePath("Fixture"))},
            {"Mod/Test.dll", "fixture"}};
        makeArchive(temp.filePath("link.zip"), files, temp.path());
        QVERIFY(!ModInstaller::install({temp.filePath("link.zip")}, game).succeeded());
        auto conflicting = files;
        conflicting.append(QPair<QString, QByteArray>{"Mod/TEST.dll", "collision"});
        makeArchive(temp.filePath("case.zip"), conflicting);
        QVERIFY(!ModInstaller::install({temp.filePath("case.zip")}, game).succeeded());
        QVERIFY(!QFileInfo::exists(game + "/Mods/Mod"));
    }

    void rejectsCorruptArchive() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        writeFile(temp.filePath("corrupt.zip"), "not an archive");
        QVERIFY(!ModInstaller::install({temp.filePath("corrupt.zip")}, game).succeeded());
    }

    void rejectsLinkedSourceAndDestination() {
#ifdef Q_OS_UNIX
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Mod"));
        QVERIFY(QFile::link(temp.filePath("Mod/Test.dll"), temp.filePath("Mod/linked.dll")));
        QVERIFY(!ModInstaller::install({temp.filePath("Mod")}, game).succeeded());
        QVERIFY(QFile::remove(temp.filePath("Mod/linked.dll")));
        QVERIFY(QDir().mkdir(temp.filePath("Elsewhere")));
        QVERIFY(QFile::link(temp.filePath("Elsewhere"), game + "/Mods"));
        QVERIFY(!ModInstaller::install({temp.filePath("Mod")}, game).succeeded());
        QVERIFY(QDir(temp.filePath("Elsewhere")).isEmpty());
#endif
    }

    void resolvesMacBundleRoot() {
        QTemporaryDir temp;
        const auto game = temp.filePath("Stardew Valley.app");
        writeFile(game + "/Contents/MacOS/StardewValley.dll", "fixture");
        makeMod(temp.filePath("Mod"));
        const auto result = ModInstaller::install({temp.filePath("Mod")}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(QFileInfo::exists(game + "/Contents/MacOS/Mods/Mod/manifest.json"));
    }

    void rejectsCollidingFoldersAndBatchIdentities() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(game + "/Mods/Cozy", "Existing.Cozy");
        makeMod(temp.filePath("download/cozy"), "New.Cozy");
        QVERIFY(!ModInstaller::install({temp.filePath("download/cozy")}, game).succeeded());
        makeMod(temp.filePath("First"), "Same.Identity");
        makeMod(temp.filePath("Second"), "Same.Identity");
        QVERIFY(!ModInstaller::install({temp.filePath("First"), temp.filePath("Second")}, game)
                     .succeeded());
        QVERIFY(!QFileInfo::exists(game + "/Mods/First"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/Second"));
    }

    void rejectsOversizedFilesAndDeepFolders() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Big"));
        QFile big(temp.filePath("Big/large.bin"));
        QVERIFY(big.open(QIODevice::WriteOnly));
        QVERIFY(big.resize(513LL * 1024 * 1024));
        big.close();
        const auto oversized = ModInstaller::install({temp.filePath("Big")}, game);
        QVERIFY(!oversized.succeeded());
        QVERIFY(oversized.error.contains("512 MiB"));
        auto deep = temp.filePath("Deep");
        for (int i = 0; i < 66; ++i) deep += "/d";
        makeMod(deep);
        const auto nested = ModInstaller::install({temp.filePath("Deep")}, game);
        QVERIFY(!nested.succeeded());
        QVERIFY(nested.error.contains("64-level"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/Big"));
        QVERIFY(!QFileInfo::exists(game + "/Mods/Mod"));
        QVERIFY(
            QDir(game).entryList({".prismatic-install-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }

    void rejectsMalformedManifestAndAcceptsComments() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("Bad"));
        writeFile(temp.filePath("Bad/manifest.json"), "{broken json}");
        QVERIFY(!ModInstaller::install({temp.filePath("Bad")}, game).succeeded());
        makeMod(temp.filePath("Commented"));
        const auto original = manifestBytes(temp.filePath("Commented"));
        writeFile(temp.filePath("Commented/manifest.json"), "// Mod author comment\n" + original);
        const auto result = ModInstaller::install({temp.filePath("Commented")}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
    }

    void rejectsExcessiveManifestNesting() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("DeepJson"));
        QByteArray data = manifestBytes(temp.filePath("DeepJson"));
        const auto end = data.lastIndexOf('}');
        data.insert(end, ",\"Extra\":" + QByteArray(80, '[') + "0" + QByteArray(80, ']'));
        writeFile(temp.filePath("DeepJson/manifest.json"), data);
        const auto result = ModInstaller::install({temp.filePath("DeepJson")}, game);
        QVERIFY(!result.succeeded());
        QVERIFY(result.error.contains("nesting"));
    }

    void acceptsSmapiStyleTrailingCommasWithoutChangingStrings() {
        QTemporaryDir temp;
        const auto game = makeGame(temp);
        makeMod(temp.filePath("RelaxedJson"));
        writeFile(
            temp.filePath("RelaxedJson/manifest.json"),
            "{\"Name\":\"Literal ,} and ,] strings\", \"Author\":\"Test\", \"Version\":\"1.0.0\","
            "\"Description\":\"https://example.com/a//b\", \"UniqueID\":\"Test.Relaxed\","
            "\"EntryDll\":\"Test.dll\", \"UpdateKeys\":[\"Nexus:123\", /* comment */],}");
        const auto result = ModInstaller::install({temp.filePath("RelaxedJson")}, game);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QCOMPARE(result.mods.first().name, "Literal ,} and ,] strings");
    }
};

QTEST_GUILESS_MAIN(ModInstallerTests)
#include "ModInstallerTests.moc"
