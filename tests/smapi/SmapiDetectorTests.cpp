#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

#include "ManagedAssemblyVersionReader.h"
#include "SmapiDetector.h"
#include "SmapiFixture.h"

using SmapiTestFixtures::createSmapiFixture;
using SmapiTestFixtures::managedAssemblyFixture;
using SmapiTestFixtures::writeEmptyFile;
using SmapiTestFixtures::writeFile;

class SmapiDetectorTests : public QObject {
    Q_OBJECT

   private slots:
    void readsManagedAssemblyVersion() {
        QTemporaryDir temp;
        const auto path = temp.filePath("SMAPI.Toolkit.dll");
        QVERIFY(writeFile(path, managedAssemblyFixture(4, 5, 2)));

        QString error;
        const auto version = ManagedAssemblyVersionReader().read(path, &error);
        QVERIFY2(version.has_value(), qPrintable(error));
        QCOMPARE(version->toString(), QString("4.5.2"));
    }

    void rejectsNonAssemblyFiles() {
        QTemporaryDir temp;
        const auto path = temp.filePath("not-an-assembly.dll");
        QVERIFY(writeFile(path, "not a PE file"));

        QString error;
        QVERIFY(!ManagedAssemblyVersionReader().read(path, &error));
        QVERIFY(!error.isEmpty());
    }

    void detectsInstalledSmapiAndVersion() {
        QTemporaryDir temp;
        const SmapiVersion expected{4, 5, 2};
        QVERIFY(createSmapiFixture(temp.path(), expected, expected));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Installed);
        QVERIFY(installation.isInstalled());
        QVERIFY(installation.version.has_value());
        QCOMPARE(installation.version->toString(), QString("4.5.2"));
    }

    /// @brief Detects SMAPI when its metadata contains comments and literal comment-like strings.
    void acceptsCommentedMetadata() {
        QTemporaryDir temp;
        const QByteArray metadata =
            "{ /* Bundled SMAPI metadata */\n"
            "  // Comments are supported by SMAPI.\n"
            "  \"Mods\": { \"Url\": \"https://example.com/*literal*/\" }\n"
            "}";
        QVERIFY(createSmapiFixture(temp.path(), {4, 5, 2}, {4, 5, 2}, metadata));

        const auto installation = SmapiDetector().inspect(temp.path());
        QVERIFY2(installation.isInstalled(), qPrintable(installation.diagnostic));
        QCOMPARE(installation.version->toString(), QString("4.5.2"));
    }

#ifdef Q_OS_MACOS
    /// @brief Detects SMAPI inside a macOS bundle when given the bundle root.
    void detectsMacBundleInstallation() {
        QTemporaryDir temp;
        const auto bundle = temp.filePath("Stardew Valley.app");
        const auto executableDirectory = QDir(bundle).filePath("Contents/MacOS");
        QVERIFY(createSmapiFixture(executableDirectory, {4, 5, 2}, {4, 5, 2}));

        const auto installation = SmapiDetector().inspect(bundle);
        QVERIFY2(installation.isInstalled(), qPrintable(installation.diagnostic));
        QCOMPARE(installation.gameDirectory, executableDirectory);
        QCOMPARE(installation.executablePath,
                 QDir(executableDirectory).filePath("StardewModdingAPI"));
    }
#endif

    void reportsNotInstalledForPlainDirectory() {
        QTemporaryDir temp;

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::NotInstalled);
        QVERIFY(!installation.isInstalled());
    }

    void ignoresLegacyRootMetadataMarker() {
        QTemporaryDir temp;
        QVERIFY(writeFile(QDir(temp.path()).filePath("StardewModdingAPI.metadata.json"), "{}"));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::NotInstalled);
    }

    void reportsInvalidGamePath() {
        QTemporaryDir temp;
        const auto missingPath = QDir(temp.path()).filePath("missing-game");

        const auto installation = SmapiDetector().inspect(missingPath);
        QCOMPARE(installation.state, SmapiState::InvalidGamePath);
    }

    void reportsIncompleteInstallation() {
        QTemporaryDir temp;
        QVERIFY(writeEmptyFile(QDir(temp.path()).filePath("StardewModdingAPI.dll")));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Incomplete);
        QVERIFY(installation.diagnostic.contains("smapi-internal/"));
    }

    void reportsMismatchedComponentVersions() {
        QTemporaryDir temp;
        QVERIFY(createSmapiFixture(temp.path(), {4, 5, 2}, {4, 5, 1}));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::ComponentMismatch);
        QVERIFY(installation.diagnostic.contains("4.5.2"));
        QVERIFY(installation.diagnostic.contains("4.5.1"));
    }

    void rejectsInvalidMetadata() {
        QTemporaryDir temp;
        QVERIFY(createSmapiFixture(temp.path(), {4, 5, 2}, {4, 5, 2}, "not json"));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Incomplete);
        QVERIFY(installation.diagnostic.contains("valid JSON"));
    }
};

QTEST_MAIN(SmapiDetectorTests)
#include "SmapiDetectorTests.moc"
