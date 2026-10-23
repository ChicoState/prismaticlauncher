#include "SmapiWindow.h"

#include "ui_SmapiWindow.h"

SmapiWindow::SmapiWindow(QWidget* parent)
    : QDialog(parent), ui_(std::make_unique<Ui::SmapiWindow>()) {
    ui_->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);
}

SmapiWindow::~SmapiWindow() = default;
