#pragma once

#include <QString>

class LauncherConfig {
   public:
    explicit LauncherConfig(QString filePath);
    [[nodiscard]] QString gameDirectory() const;
    [[nodiscard]] QString filePath() const;
    bool saveGameDirectory(const QString& directory);
    [[nodiscard]] bool farmPackEnabled() const;
    bool saveFarmPackEnabled(bool enabled);
    [[nodiscard]] int farmPackOpacityPercent() const;
    bool saveFarmPackOpacityPercent(int percent);
    [[nodiscard]] int farmPackSizePercent() const;
    bool saveFarmPackSizePercent(int percent);
    [[nodiscard]] QString farmPackImagePath() const;
    bool saveFarmPackImagePath(const QString& imagePath);
    [[nodiscard]] QString farmPackImageDirectory() const;
    [[nodiscard]] QString importFarmPackImage(const QString& imagePath) const;
    static QString expandHome(const QString& path);

   private:
    QString filePath_;
};
