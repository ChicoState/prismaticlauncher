#ifndef PRISMATIC_LAUNCHER_PATHS_H
#define PRISMATIC_LAUNCHER_PATHS_H

#include <filesystem>

// Shared locations supplied by application setup or user settings. Constructing
// Paths does not discover installations, create directories, or require them to exist.
// TODO: Add platform-specific launcher data/config defaults once their locations are agreed.
// TODO: Connect game-directory discovery or selection when application setup exists.
class Paths {
public:
    // Throws std::invalid_argument if any supplied directory is empty.
    Paths(std::filesystem::path gameDirectory, std::filesystem::path dataDirectory,
          std::filesystem::path configDirectory);

    [[nodiscard]] const std::filesystem::path& gameDirectory() const noexcept;
    [[nodiscard]] const std::filesystem::path& dataDirectory() const noexcept;
    [[nodiscard]] const std::filesystem::path& configDirectory() const noexcept;

    [[nodiscard]] std::filesystem::path modsDirectory() const;
    [[nodiscard]] std::filesystem::path smapiExecutable() const;
    [[nodiscard]] std::filesystem::path smapiInternalDirectory() const;

private:
    std::filesystem::path gameDirectory_;
    std::filesystem::path dataDirectory_;
    std::filesystem::path configDirectory_;
};

#endif  // PRISMATIC_LAUNCHER_PATHS_H
