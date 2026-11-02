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
    // Reads manifests from disk without following links. Errors prevent installation.
    [[nodiscard]] static ModInstallResult installed(const QString& gameDirectory);
    // Copies sources; never executes them or overwrites existing mods. All sources
    // are validated and staged before publishing; failures roll back new folders.
    [[nodiscard]] static ModInstallResult install(const QStringList& sources,
                                                  const QString& gameDirectory);
};
