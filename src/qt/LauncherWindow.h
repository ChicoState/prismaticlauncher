#pragma once

#include <memory>

#include <QMainWindow>

#include "LauncherConfig.h"

namespace Ui {
class LauncherWindow;
}

class LauncherWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit LauncherWindow(const QString& configFile, QWidget* parent = nullptr);
    ~LauncherWindow() override;

   private:
    void connectSignals();
    void populateInstances();
    void addInstance();
    void openInstance(int tab = 0);
    void launchGame();
    void refreshPathStatus();
    void filterInstances(const QString& query);

    LauncherConfig config_;
    std::unique_ptr<Ui::LauncherWindow> ui_;
};
