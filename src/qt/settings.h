#ifndef SETTINGS_H
#define SETTINGS_H

#include <QWidget>

#include "GameDirectoryDetector.h"

namespace Ui {
class settings;
}

class LauncherConfig;

class settings : public QWidget {
    Q_OBJECT

   public:
    explicit settings(LauncherConfig* config, QWidget* parent = nullptr,
                      GameDirectoryDetector detector = GameDirectoryDetector());
    ~settings();
    void reload();

   signals:
    void gameDirectoryChanged(const QString& directory);
    void backToLibraryRequested();

   private:
    void detectGameDirectory();
    void saveGameDirectory();

    LauncherConfig* config_;
    GameDirectoryDetector detector_;
    Ui::settings* ui;
};

#endif  // SETTINGS_H
