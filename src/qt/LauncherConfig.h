#pragma once

#include <QString>

class LauncherConfig {
   public:
    explicit LauncherConfig(QString filePath);
    [[nodiscard]] QString gameDirectory() const;
    [[nodiscard]] QString filePath() const;
    bool saveGameDirectory(const QString& directory);
    [[nodiscard]] bool farmPackEnabled() const;
    /// @brief Saves visibility while retaining image and opacity.
    /// @return True after syncing the settings, or false on write failure.
    bool saveFarmPackEnabled(bool enabled);
    [[nodiscard]] int farmPackOpacityPercent() const;
    /// @brief Saves opacity in 0–100 while retaining other values.
    /// @return True after syncing the settings, or false on invalid input or write failure.
    bool saveFarmPackOpacityPercent(int percent);
    /// @brief Returns the saved image identifier; an empty legacy value uses Default artwork.
    [[nodiscard]] QString farmPackImage() const;
    /// @brief Syncs all three ChickPix values and restores cached entries on write failure.
    /// @return True after syncing the settings, or false on invalid opacity or write failure.
    bool saveChickPix(const QString& image, int opacityPercent, bool enabled);
    static QString expandHome(const QString& path);

   private:
    QString filePath_;
};
