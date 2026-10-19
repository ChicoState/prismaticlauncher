#pragma once

#include <QMainWindow>

#include "LauncherConfig.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QPlainTextEdit;
class QComboBox;

class LauncherWindow : public QMainWindow {
   public:
    explicit LauncherWindow(const QString& configFile, QWidget* parent = nullptr);

   private:
    QWidget* createLibrary();
    QWidget* createDetails();
    QWidget* createModsPage();
    QWidget* createLogsPage();
    QWidget* createSettingsPage();
    void addInstance();
    void openInstance(int tab = 0);
    void launchGame();
    void refreshPathStatus();
    void filterInstances(const QString& query);

    LauncherConfig config_;
    QStackedWidget* pages_ = nullptr;
    QStackedWidget* detailPages_ = nullptr;
    QListWidget* instances_ = nullptr;
    QListWidget* navigation_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLineEdit* gamePath_ = nullptr;
    QLabel* detailTitle_ = nullptr;
    QLabel* pathStatus_ = nullptr;
    QLabel* instanceCount_ = nullptr;
    QPlainTextEdit* logs_ = nullptr;
    QComboBox* launchMode_ = nullptr;
};
