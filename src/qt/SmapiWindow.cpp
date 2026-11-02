#include "SmapiWindow.h"

#include "ui_SmapiWindow.h"

SmapiWindow::SmapiWindow(const QString& gameDirectory, QWidget* parent)
    : QDialog(parent), gameDirectory_(gameDirectory), ui_(std::make_unique<Ui::SmapiWindow>()) {
    ui_->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);
    refresh();
}

SmapiWindow::~SmapiWindow() = default;

void SmapiWindow::setGameDirectory(const QString& gameDirectory) {
    gameDirectory_ = gameDirectory;
    refresh();
}

void SmapiWindow::refresh() {
    const auto installation = detector_.inspect(gameDirectory_);
    QString status;
    switch (installation.state) {
        case SmapiState::InvalidGamePath:
            status = "Game directory not found";
            break;
        case SmapiState::NotInstalled:
            status = "SMAPI is not installed";
            break;
        case SmapiState::Installed:
            status = "SMAPI installed\nVersion " +
                     (installation.version ? installation.version->toString() : "unknown");
            break;
        case SmapiState::Incomplete:
            status = "SMAPI installation is incomplete";
            break;
        case SmapiState::ComponentMismatch:
            status = "SMAPI components have mismatched versions";
            break;
        case SmapiState::Unreadable:
            status = "SMAPI files could not be read";
            break;
    }
    if (installation.state != SmapiState::Installed &&
        installation.state != SmapiState::NotInstalled && !installation.diagnostic.isEmpty()) {
        status += "\n" + installation.diagnostic;
    }
    ui_->stateLabel->setText(status);
    ui_->stateLabel->setAlignment(Qt::AlignCenter);
}
