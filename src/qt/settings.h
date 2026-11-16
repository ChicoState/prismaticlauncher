#ifndef SETTINGS_H
#define SETTINGS_H

#include <QPointer>
#include <QWidget>

#include "GameDirectoryDetector.h"

namespace Ui {
class settings;
}

class LauncherConfig;
class ChickPixDialog;

class settings : public QWidget {
    Q_OBJECT

   public:
    /// @brief Builds game-path settings and the entry to the ChickPix editor using the supplied
    /// config.
    explicit settings(LauncherConfig* config, QWidget* parent = nullptr,
                      GameDirectoryDetector detector = GameDirectoryDetector());
    ~settings();
    void reload();

   signals:
    /// @brief Announces that the ChickPix editor successfully synced its pending settings.
    void chickPixChanged();
    void gameDirectoryChanged(const QString& directory);
    void backToLibraryRequested();

   private:
    /// @brief Opens the ChickPix editor without blocking the event loop.
    void configureChickPix();
    void detectGameDirectory();
    void saveGameDirectory();

    LauncherConfig* config_;
    QPointer<ChickPixDialog> chickPixDialog_;
    GameDirectoryDetector detector_;
    Ui::settings* ui;
};

#endif  // SETTINGS_H
