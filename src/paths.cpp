#include "paths.h"

#include <stdexcept>
#include <utility>

Paths::Paths(std::filesystem::path gameDirectory, std::filesystem::path dataDirectory,
             std::filesystem::path configDirectory)
    : gameDirectory_(std::move(gameDirectory)),
      dataDirectory_(std::move(dataDirectory)),
      configDirectory_(std::move(configDirectory)) {
    // An empty game root would silently resolve Mods/SMAPI against the working directory.
    if (gameDirectory_.empty() || dataDirectory_.empty() || configDirectory_.empty()) {
        throw std::invalid_argument("Game, data, and config directories must not be empty.");
    }
}

const std::filesystem::path& Paths::gameDirectory() const noexcept {
    return gameDirectory_;
}

const std::filesystem::path& Paths::dataDirectory() const noexcept {
    return dataDirectory_;
}

const std::filesystem::path& Paths::configDirectory() const noexcept {
    return configDirectory_;
}

std::filesystem::path Paths::modsDirectory() const {
    return gameDirectory_ / "Mods";
}

std::filesystem::path Paths::smapiExecutable() const {
#ifdef _WIN32
    return gameDirectory_ / "StardewModdingAPI.exe";
#else
    return gameDirectory_ / "StardewModdingAPI";
#endif
}
// TODO May need changing depending on SMAPI branch implementation
std::filesystem::path Paths::smapiInternalDirectory() const {
    return gameDirectory_ / "smapi-internal";
}
