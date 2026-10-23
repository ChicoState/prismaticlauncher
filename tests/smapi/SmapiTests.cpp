#include <QTemporaryDir>
#include <QtTest>
#include <filesystem>
#include <memory>

#include "SmapiManager.h"

/// @brief Exercises SMAPI validation failures using disposable directories.
class SmapiTests : public QObject {
    Q_OBJECT
   private slots:
    /// @brief Rejects installation when the requested game directory is missing.
    void managerRejectsMissingGameDirectory() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiInstaller>(), std::make_unique<SmapiLauncher>());

        // Keep the installer directory valid so the missing game directory causes the failure.
        const auto result = manager.install(temp.filePath("missing-game"), temp.path());

        QCOMPARE(result.status, InstallStatus::InvalidGameDirectory);
        QVERIFY(!result.succeeded());
    }

    /// @brief Rejects a missing installer directory when the game directory exists.
    void managerRejectsMissingInstallerDirectory() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiInstaller>(), std::make_unique<SmapiLauncher>());

        // An existing game directory lets validation reach the missing installer directory.
        const auto result = manager.install(temp.path(), temp.filePath("missing-installer"));

        QCOMPARE(result.status, InstallStatus::InvalidInstaller);
        QVERIFY(!result.succeeded());
    }

    /// @brief Reports SMAPI as absent and rejects launching from an empty directory.
    void managerRejectsLaunchWithoutSmapi() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SmapiManager manager(std::make_unique<SmapiInstaller>(), std::make_unique<SmapiLauncher>());

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
