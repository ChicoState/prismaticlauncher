#pragma once

#include <QMainWindow>
#include <QPointer>
#include <memory>

#include "LauncherConfig.h"

class LogWindow;
class NexusPage;
class QProcess;

namespace Ui {
class InstanceWindow;
}

class InstanceWindow : public QMainWindow {
    Q_OBJECT

   public:
    /// @brief Creates instance pages and an embedded console using a borrowed shared configuration.
    explicit InstanceWindow(const QString& name, bool modded, LauncherConfig* config,
                            QWidget* parent = nullptr);
    /// @brief Destroys the instance UI while independently owned game processes keep running.
    ~InstanceWindow() override;

    /// @brief Updates the instance title, launch mode, and shared game path without emitting a mode
    /// change.
    void setInstance(const QString& name, bool modded);
    /// @brief Selects a valid instance page and lazily loads mods on their first selection.
    void selectTab(int tab);
    /// @brief Refreshes the shared game path and records the saved directory in the embedded
    /// console.
    void refreshGameDirectory();
    /// @brief Selects Console, validates the executable and SMAPI, and starts asynchronous output
    /// capture. The game process outlives this window; UTF-8 fragments retain decoder state until
    /// process exit.
    void launchGame();

   signals:
    /// @brief Announces a user-selected launch mode; true means modded and false means vanilla.
    void launchModeChanged(bool modded);
    /// @brief Requests that the owner open launcher settings for the shared game directory.
    void settingsRequested();

   private:
    /// @brief Connects instance controls to navigation, launching, logs, and shared settings.
    void connectSignals();
    /// @brief Displays the configured game folder and whether that directory currently exists.
    void refreshPathStatus();

    NexusPage* mods_;
    bool modsLoaded_ = false;
    LauncherConfig* config_;
    std::unique_ptr<Ui::InstanceWindow> ui_;
    LogWindow* logWindow_;
    QPointer<QProcess> process_;
};
