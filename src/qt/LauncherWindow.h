#pragma once

#include <QMainWindow>
#include <QPointer>
#include <memory>

#include "FarmPack.h"
#include "LauncherConfig.h"

namespace Ui {
class LauncherWindow;
}

class InstanceWindow;
class settings;
class SmapiWindow;
class QGraphicsOpacityEffect;
class QLabel;
class QResizeEvent;

class LauncherWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit LauncherWindow(const QString& configFile, QWidget* parent = nullptr);
    ~LauncherWindow() override;

   private:
    void connectSignals();
    void populateInstances();
    void addInstance();
    void openInstanceWindow(int tab = 0);
    void openSettingsWindow();
    /// @brief Opens the SMAPI settings window for the configured game directory.
    void openSmapiWindow();
    /// @brief Opens the selected instance and launches it with live console capture.
    void launchGame();
    void filterInstances(const QString& query);
    void refreshFarmPackOverlay();
    void resizeEvent(QResizeEvent* event) override;

    LauncherConfig config_;
    FarmPack farmPack_;
    bool farmPackLoaded_ = false;
    bool farmPackEnabled_ = false;
    QLabel* farmPackOverlay_ = nullptr;
    QGraphicsOpacityEffect* farmPackOpacityEffect_ = nullptr;
    QPointer<InstanceWindow> instanceWindow_;
    QPointer<settings> settingsWindow_;
    QPointer<SmapiWindow> smapiWindow_;
    std::unique_ptr<Ui::LauncherWindow> ui_;
};
