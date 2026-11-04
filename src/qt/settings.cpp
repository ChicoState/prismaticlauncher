#include "settings.h"

#include <QFileDialog>
#include <QFileInfo>
#include <utility>

#include "LauncherConfig.h"
#include "ui_settings.h"

settings::settings(LauncherConfig* config, QWidget* parent, GameDirectoryDetector detector)
    : QWidget(parent, Qt::Window),
      config_(config),
      detector_(std::move(detector)),
      ui(new Ui::settings) {
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
    connect(ui->detectDirectory, &QPushButton::clicked, this, &settings::detectGameDirectory);
    reload();
}

settings::~settings() { delete ui; }

void settings::reload() {
    const auto directory = config_->gameDirectory();
    ui->gameDirectoryValue->setText(directory);
    const auto message =
        QFileInfo(directory).isDir()
            ? QString("Game folder exists. Play checks the selected executable.")
            : QString("Game folder not found. Choose your installation directory.");
    ui->pathStatus->setText(message + "\nSettings file: " + config_->filePath());
}

void settings::detectGameDirectory() {
    const auto directory = detector_.detect(ui->gameDirectoryValue->text());
    if (directory.isEmpty()) {
        ui->pathStatus->setText(
            "Could not detect Stardew Valley. Choose your game folder with Browse.");
    } else {
        ui->gameDirectoryValue->setText(directory);
        ui->pathStatus->setText("Detected game directory. Select Save game path to use it.");
    }
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
