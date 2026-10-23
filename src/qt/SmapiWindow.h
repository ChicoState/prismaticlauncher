#pragma once

#include <memory>

#include <QDialog>

namespace Ui {
class SmapiWindow;
}

class SmapiWindow : public QDialog {
    Q_OBJECT

   public:
    explicit SmapiWindow(QWidget* parent = nullptr);
    ~SmapiWindow() override;

   private:
    std::unique_ptr<Ui::SmapiWindow> ui_;
};
