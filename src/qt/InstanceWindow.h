#pragma once

#include <QMainWindow>
#include <memory>

#include "LauncherConfig.h"

class NexusPage;
class ModInstallPage;
class QTabWidget;

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

   signals:
    void launchModeChanged(bool modded);

   private:
    void connectSignals();
    void refreshPathStatus();
    void launchGame();

    NexusPage* mods_;
    ModInstallPage* installedMods_;
    QTabWidget* modTabs_;
    bool modsLoaded_ = false;
    LauncherConfig* config_;
    std::unique_ptr<Ui::InstanceWindow> ui_;
};
