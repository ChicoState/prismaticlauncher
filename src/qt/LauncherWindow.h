#pragma once

#include <memory>

#include <QMainWindow>
#include <QPointer>

#include "LauncherConfig.h"
#include "FarmPack.h"

namespace Ui {
class LauncherWindow;
}

class InstanceWindow;
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
    std::unique_ptr<Ui::LauncherWindow> ui_;
};
