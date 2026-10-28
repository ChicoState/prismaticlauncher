
#include "SmapiManager.h"

#include <utility>

SmapiManager::SmapiManager(std::unique_ptr<SmapiLauncher> launcher)
    : launcher_(std::move(launcher)) {}

SmapiInstallation SmapiManager::inspect(const QString& gameDirectory) const {
    return detector_.inspect(gameDirectory);
}

/*
 * Verifies the user's SMAPI installation.
 * @param the game directory
 * @returns whether SMAPI is intalled
 */
bool SmapiManager::isInstalled(const QString& gameDirectory) const {
    return inspect(gameDirectory).isInstalled();
}

/*
 * Launches the modded game using the attached launcher after verification.
 * @param the game directory
 * @returns the result of the launch
 */
LaunchResult SmapiManager::launch(const QString& gameDirectory,
                                  const std::wstring& arguments) const {
    if (!inspect(gameDirectory).isInstalled()) {
        return {.status = LaunchStatus::SmapiNotInstalled,
                .message = "SMAPI is not installed or its installation is incomplete."};
    }

#ifdef _WIN32
    const bool launched =
        launcher_->launch(std::filesystem::path(gameDirectory.toStdWString()), arguments);

    return launched ? LaunchResult{.status = LaunchStatus::Success,
                                   .message = "Stardew Valley started through SMAPI."}
                    : LaunchResult{.status = LaunchStatus::LaunchFailed,
                                   .message = "Unable to start Stardew Valley through SMAPI."};
#else
    return {.status = LaunchStatus::UnsupportedPlatform,
            .message = "SMAPI launching is not implemented for this platform yet."};
#endif
}
