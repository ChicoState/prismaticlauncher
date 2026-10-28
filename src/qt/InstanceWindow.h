#pragma once

#include <QMainWindow>
#include <QPointer>
#include <memory>

#include "LauncherConfig.h"

class LogWindow;
class QProcess;

namespace Ui {
class InstanceWindow;
}

class InstanceWindow : public QMainWindow {
    Q_OBJECT

   public:
    /// @brief Creates an instance editor with a reusable live log window.
    explicit InstanceWindow(const QString& name, bool modded, LauncherConfig* config,
                            QWidget* parent = nullptr);
    ~InstanceWindow() override;

    /// @brief Updates the displayed instance name and launch mode.
    void setInstance(const QString& name, bool modded);
    /// @brief Selects an instance section when its index is valid.
    void selectTab(int tab);
    /// @brief Starts the selected game asynchronously and captures its console output.
    void launchGame();

   signals:
    void launchModeChanged(bool modded);

   private:
    /// @brief Connects instance navigation, settings, launch, and log controls.
    void connectSignals();
    /// @brief Refreshes the configured game directory and its validation message.
    void refreshPathStatus();

    LauncherConfig* config_;
    std::unique_ptr<Ui::InstanceWindow> ui_;
    LogWindow* logWindow_;
    QPointer<QProcess> process_;
};