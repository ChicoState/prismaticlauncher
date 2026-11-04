#pragma once

#include <QString>
#include <QStringList>

class GameDirectoryDetector {
   public:
    GameDirectoryDetector();
    explicit GameDirectoryDetector(QStringList steamRoots, QStringList gameDirectories = {});

    // Prefer the supplied installation, then search known locations without modifying any files.
    // Return an absolute game directory (bundle root on macOS), or empty when none is found.
    [[nodiscard]] QString detect(const QString& preferredDirectory = {}) const;

   private:
    QStringList steamRoots_;
    QStringList gameDirectories_;
};
