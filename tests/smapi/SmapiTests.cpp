#include <QTemporaryDir>
#include <QtTest>
#include <filesystem>
#include <memory>

#include "SmapiManager.h"

/// @brief Exercises SMAPI validation failures using disposable directories.
class SmapiTests : public QObject {
    Q_OBJECT
   private slots:
    /// @brief Reports an invalid path when the requested game directory is missing.
    void managerRejectsMissingGameDirectory() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiLauncher>());

        const auto result = manager.inspect(temp.filePath("missing-game"));

        QCOMPARE(result.state, SmapiState::InvalidGamePath);
        QVERIFY(!result.isInstalled());
    }

    /// @brief Reports SMAPI as absent when the game directory contains no SMAPI files.
    void managerReportsMissingInstallation() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiLauncher>());

        const auto result = manager.inspect(temp.path());

        QCOMPARE(result.state, SmapiState::NotInstalled);
        QVERIFY(!result.isInstalled());
    }

    /// @brief Reports SMAPI as absent and rejects launching from an empty directory.
    void managerRejectsLaunchWithoutSmapi() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiLauncher>());

        // The empty directory triggers the installation guard before platform-specific launching.
        QVERIFY(!manager.isInstalled(temp.path()));
        const auto result = manager.launch(temp.path());

        QCOMPARE(result.status, LaunchStatus::SmapiNotInstalled);
        QVERIFY(!result.succeeded());
    }

    /// @brief Makes the launcher return false when the SMAPI executable is missing.
    void launcherRejectsMissingExecutable() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const SmapiLauncher launcher;

        // Exercise the launcher directly, independently of the manager's installation guard.
        QVERIFY(!launcher.launch(std::filesystem::path(temp.path().toStdU16String())));
    }
};

// Run with QCoreApplication; these tests do not need widgets or a display.
QTEST_GUILESS_MAIN(SmapiTests)
// Include the meta-object code generated for the QObject test class defined above.
#include "SmapiTests.moc"
