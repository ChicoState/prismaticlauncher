#pragma once

#include <QWidget>

class LauncherConfig;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QThread;

class ModInstallPage : public QWidget {
    Q_OBJECT
   public:
    explicit ModInstallPage(LauncherConfig* config, QWidget* parent = nullptr);
    ~ModInstallPage() override;
    void refresh();
    void installSources(const QStringList& sources);
    [[nodiscard]] bool isBusy() const { return worker_ != nullptr; }

   signals:
    void operationFinished(bool success);
    void activity(const QString& message);

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;

   private:
    void startOperation(const QStringList& sources, bool installing);
    void setDropActive(bool active);
    LauncherConfig* config_;
    QThread* worker_ = nullptr;
    QWidget* dropZone_;
    QLabel* destination_;
    QLabel* status_;
    QPlainTextEdit* result_;
    QTableWidget* table_;
    QPushButton* files_;
    QPushButton* folder_;
    QPushButton* refresh_;
};
