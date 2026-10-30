#pragma once

#include <QDialog>
#include <memory>

#include "SmapiDetector.h"

namespace Ui {
class SmapiWindow;
}

class SmapiWindow : public QDialog {
    Q_OBJECT

   public:
    explicit SmapiWindow(const QString& gameDirectory, QWidget* parent = nullptr);
    ~SmapiWindow() override;

    void setGameDirectory(const QString& gameDirectory);

   private:
    void refresh();

    QString gameDirectory_;
    SmapiDetector detector_;
    std::unique_ptr<Ui::SmapiWindow> ui_;
};
