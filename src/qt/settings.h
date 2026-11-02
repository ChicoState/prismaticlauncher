#ifndef SETTINGS_H
#define SETTINGS_H

#include <QWidget>

namespace Ui {
class settings;
}

class LauncherConfig;

class settings : public QWidget {
    Q_OBJECT

   public:
    explicit settings(LauncherConfig* config, QWidget* parent = nullptr);
    ~settings();
    void reload();

   signals:
    void gameDirectoryChanged(const QString& directory);
    void backToLibraryRequested();

   private:
    void saveGameDirectory();

    LauncherConfig* config_;
    Ui::settings* ui;
};

#endif  // SETTINGS_H
