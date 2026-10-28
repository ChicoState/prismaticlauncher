
#ifndef SMAPI_MANAGER
#define SMAPI_MANAGER

#include <QString>
#include <memory>

#include "SmapiDetector.h"
#include "SmapiLauncher.h"

enum class LaunchStatus { Success, SmapiNotInstalled, LaunchFailed, UnsupportedPlatform };

/*
 * Describes the result of launching SMAPI
 */
struct LaunchResult {
    LaunchStatus status;
    QString message;

    /*
     * Returns if the launch has succeeded
     */
    [[nodiscard]] bool succeeded() const { return status == LaunchStatus::Success; }
};

/*
 * Inspects the user's SMAPI installation and launches the modded game.
 */
class SmapiManager {
   public:
    explicit SmapiManager(std::unique_ptr<SmapiLauncher> launcher);

    [[nodiscard]] SmapiInstallation inspect(const QString& gameDirectory) const;
    [[nodiscard]] bool isInstalled(const QString& gameDirectory) const;

    [[nodiscard]] LaunchResult launch(const QString& gameDirectory,
                                      const std::wstring& arguments = L"") const;

   private:
    SmapiDetector detector_;
    std::unique_ptr<SmapiLauncher> launcher_;
};

#endif
