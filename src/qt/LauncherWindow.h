#pragma once

#include <memory>

#include <QMainWindow>
#include <QPointer>

#include "LauncherConfig.h"

namespace Ui {
class LauncherWindow;
}

class InstanceWindow;

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
    /// @brief Opens the selected instance and launches it with live console capture.
    void launchGame();
    void filterInstances(const QString& query);

    LauncherConfig config_;
    QPointer<InstanceWindow> instanceWindow_;
    std::unique_ptr<Ui::LauncherWindow> ui_;
};
