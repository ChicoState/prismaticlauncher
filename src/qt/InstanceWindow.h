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
    explicit InstanceWindow(const QString& name, bool modded, LauncherConfig* config,
                            QWidget* parent = nullptr);
    ~InstanceWindow() override;

    void setInstance(const QString& name, bool modded);
    void selectTab(int tab);
    /// @brief Validates SMAPI for modded launches, then starts the game with console capture.
    void launchGame();

   signals:
    void launchModeChanged(bool modded);
    void chickPicsSettingsChanged();

   private:
    void connectSignals();
    void refreshPathStatus();

    NexusPage* mods_;
    bool modsLoaded_ = false;
    LauncherConfig* config_;
    std::unique_ptr<Ui::InstanceWindow> ui_;
    LogWindow* logWindow_;
    QPointer<QProcess> process_;
};
