#pragma once

#include <QString>
#include <optional>

#include "ManagedAssemblyVersionReader.h"

enum class SmapiState {
    InvalidGamePath,
    NotInstalled,
    Installed,
    Incomplete,
    ComponentMismatch,
    Unreadable
};

struct SmapiInstallation {
    SmapiState state = SmapiState::NotInstalled;
    QString gameDirectory;
    QString executablePath;
    std::optional<SmapiVersion> version;
    QString diagnostic;

    [[nodiscard]] bool isInstalled() const { return state == SmapiState::Installed; }
};

class SmapiDetector {
   public:
    [[nodiscard]] SmapiInstallation inspect(const QString& gameDirectory) const;

   private:
    ManagedAssemblyVersionReader versionReader_;
};
