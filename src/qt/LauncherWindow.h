#pragma once

#include <QMainWindow>
#include <QPixmap>
#include <QPointer>
#include <memory>

#include "InstanceStore.h"
#include "LauncherConfig.h"

namespace Ui {
class LauncherWindow;
}

class InstanceWindow;
class settings;
class SmapiWindow;
class QGraphicsOpacityEffect;
class QLabel;
class QEvent;

class LauncherWindow : public QMainWindow {
    Q_OBJECT

   public:
    /// @brief Loads saved instances and applies saved ChickPix artwork and quick-control values.
    explicit LauncherWindow(const QString& configFile, QWidget* parent = nullptr);
    ~LauncherWindow() override;

   private:
    /// @brief Connects launcher actions and persists quick ChickPix changes with visible failures.
    void connectSignals();
    /// @brief Loads saved instance cards and reports storage problems without replacing files.
    void populateInstances();
    /// @brief Appends a card for a successfully saved instance and retains its directory identity.
    void appendInstance(const InstanceRecord& record);
    /// @brief Validates and saves a named instance before publishing its card and detail window.
    void addInstance();
    /// @brief Opens the selected saved instance with its own mods directory.
    void openInstanceWindow(int tab = 0);
    /// @brief Shows launcher settings and applies successful ChickPix saves to the library.
    void openSettingsWindow();
    /// @brief Opens the SMAPI settings window for the configured game directory.
    void openSmapiWindow();
    /// @brief Opens the selected instance and launches it with live console capture.
    void launchGame();
    void filterInstances(const QString& query);
    /// @brief Loads the saved image or Default fallback and synchronizes the ChickPix toggle.
    void reloadChickPix();
    /// @brief Resizes and positions the cached ChickPix image without reading image files.
    void refreshFarmPackOverlay();
    /// @brief Refreshes ChickPix when the library page receives its final size or becomes visible.
    /// @return True if the base filter consumes the event, or false to continue normal dispatch.
    bool eventFilter(QObject* watched, QEvent* event) override;

    LauncherConfig config_;
    InstanceStore instances_;
    QPixmap farmPackImage_;
    bool farmPackLoaded_ = false;
    bool farmPackEnabled_ = false;
    QLabel* farmPackOverlay_ = nullptr;
    QGraphicsOpacityEffect* farmPackOpacityEffect_ = nullptr;
    QPointer<InstanceWindow> instanceWindow_;
    QPointer<settings> settingsWindow_;
    QPointer<SmapiWindow> smapiWindow_;
    std::unique_ptr<Ui::LauncherWindow> ui_;
};
