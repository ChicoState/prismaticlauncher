#pragma once

#include <QString>

class LauncherConfig {
   public:
    explicit LauncherConfig(QString filePath);
    [[nodiscard]] QString gameDirectory() const;
    [[nodiscard]] QString filePath() const;
    bool saveGameDirectory(const QString& directory);
    static QString expandHome(const QString& path);

   private:
    QString filePath_;
};
