#pragma once

#include <QList>
#include <QStringList>

struct InstalledMod {
    QString name;
    QString version;
    QString uniqueId;
    QString directory;
};

struct ModInstallResult {
    QList<InstalledMod> mods;
    QString error;
    [[nodiscard]] bool succeeded() const { return error.isEmpty(); }
};

class ModInstaller {
   public:
    // Resolves a configured macOS bundle root to Contents/MacOS when present.
    [[nodiscard]] static QString modsDirectory(const QString& gameDirectory);
    /// @brief Reads installed manifests from the explicit mods folder, or the game's Mods folder.
    /// The actual game directory is validated in both cases; links are rejected at the destination.
    [[nodiscard]] static ModInstallResult installed(const QString& gameDirectory,
                                                    const QString& explicitModsDirectory = {});
    /// @brief Validates and stages sources before publishing to the explicit or shared mods folder.
    /// Never executes sources or replaces existing mods; failures roll back newly published
    /// folders.
    [[nodiscard]] static ModInstallResult install(const QStringList& sources,
                                                  const QString& gameDirectory,
                                                  const QString& explicitModsDirectory = {});
};
