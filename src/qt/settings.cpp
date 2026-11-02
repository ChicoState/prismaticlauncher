#include "settings.h"

#include "LauncherConfig.h"
#include "ui_settings.h"

#include <QFileDialog>
#include <QFileInfo>

settings::settings(LauncherConfig* config, QWidget* parent)
    : QWidget(parent, Qt::Window), config_(config), ui(new Ui::settings) {
    ui->setupUi(this);
    connect(ui->backLibrary, &QPushButton::clicked, this, [this] {
        close();
        emit backToLibraryRequested();
    });
    connect(ui->browseDirectory, &QPushButton::clicked, this, [this] {
        const auto directory = QFileDialog::getExistingDirectory(
            this, "Select Stardew Valley folder", ui->gameDirectoryValue->text());
        if (!directory.isEmpty()) ui->gameDirectoryValue->setText(directory);
    });
    connect(ui->savePath, &QPushButton::clicked, this, &settings::saveGameDirectory);
    reload();
}

settings::~settings() { delete ui; }

void settings::reload() {
    const auto directory = config_->gameDirectory();
    ui->gameDirectoryValue->setText(directory);
    const auto message = QFileInfo(directory).isDir()
                             ? QString("Game folder exists. Play checks the selected executable.")
                             : QString("Game folder not found. Choose your installation directory.");
    ui->pathStatus->setText(message + "\nSettings file: " + config_->filePath());
}

void settings::saveGameDirectory() {
    if (!config_->saveGameDirectory(ui->gameDirectoryValue->text())) {
        ui->pathStatus->setText(
            "Could not save. Use an absolute path and a writable config location.");
        return;
    }
    reload();
    ui->pathStatus->setText("Saved game path.\n" + ui->pathStatus->text());
    emit gameDirectoryChanged(config_->gameDirectory());
}
