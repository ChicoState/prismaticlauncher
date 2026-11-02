#include "InstanceWindow.h"

#include "ui_InstanceWindow.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QTextCursor>

InstanceWindow::InstanceWindow(const QString& name, bool modded, LauncherConfig* config,
                               QWidget* parent)
    : QMainWindow(parent), config_(config), ui_(std::make_unique<Ui::InstanceWindow>()) {
    ui_->setupUi(this);
    const auto imageDirectory = config_->farmPackImageDirectory();
    QDir().mkpath(imageDirectory);
    for (const auto& image : {QString("default.png"), QString("spring.png"),
                              QString("summer.png")}) {
        const auto destination = QDir(imageDirectory).filePath(image);
        if (!QFileInfo::exists(destination)) QFile::copy(":/farmpack/" + image, destination);
    }
    for (int index = 1; index < ui_->chickPicsImage->count(); ++index) {
        const auto label = ui_->chickPicsImage->itemText(index).toLower();
        const auto name = label.contains("spring")
                              ? "spring.png"
                              : label.contains("summer") ? "summer.png" : "default.png";
        ui_->chickPicsImage->setItemData(index, QDir(imageDirectory).filePath(name));
    }
    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(960, 680);
    connectSignals();
    setInstance(name, modded);
}

InstanceWindow::~InstanceWindow() = default;

void InstanceWindow::connectSignals() {
    connect(ui_->navigation, &QListWidget::currentRowChanged, ui_->detailPages,
            &QStackedWidget::setCurrentIndex);
    connect(ui_->backToLibrary, &QPushButton::clicked, this, &QWidget::close);
    connect(ui_->playDetailInstance, &QPushButton::clicked, this,
            &InstanceWindow::launchGame);
    connect(ui_->logFilter, &QLineEdit::textChanged, this, [this](const QString& query) {
        auto cursor = ui_->logs->textCursor();
        cursor.movePosition(QTextCursor::Start);
        ui_->logs->setTextCursor(cursor);
        if (!query.isEmpty()) ui_->logs->find(query);
    });
    connect(ui_->browseGamePath, &QPushButton::clicked, this, [this] {
        const auto directory = QFileDialog::getExistingDirectory(
            this, "Select Stardew Valley folder", ui_->gamePath->text());
        if (!directory.isEmpty()) ui_->gamePath->setText(directory);
    });
    connect(ui_->savePath, &QPushButton::clicked, this, [this] {
        if (!config_->saveGameDirectory(ui_->gamePath->text())) {
            ui_->pathStatus->setText(
                "Could not save. Use an absolute path and a writable config location.");
            return;
        }
        refreshPathStatus();
        ui_->logs->appendPlainText("Saved game directory: " + config_->gameDirectory());
        statusBar()->showMessage("Game path saved.");
    });
    connect(ui_->launchMode, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit launchModeChanged(index == 1);
    });
    connect(ui_->chickPicsEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (config_->saveFarmPackEnabled(enabled)) emit chickPicsSettingsChanged();
    });
    connect(ui_->chickPicsImage, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        if (config_->saveFarmPackImagePath(ui_->chickPicsImage->itemData(index).toString())) {
            emit chickPicsSettingsChanged();
        }
    });
    connect(ui_->browseChickPicsImage, &QPushButton::clicked, this, [this] {
        const auto image = QFileDialog::getOpenFileName(
            this, "Add image to chickens folder", config_->farmPackImageDirectory(),
            "Images (*.png *.jpg *.jpeg *.webp)");
        if (image.isEmpty()) return;
        const auto importedImage = config_->importFarmPackImage(image);
        if (!importedImage.isEmpty() && config_->saveFarmPackImagePath(importedImage)) {
            ui_->chickPicsImage->setCurrentIndex(ui_->chickPicsImage->findData(importedImage));
            if (ui_->chickPicsImage->currentData().toString() != importedImage) {
                ui_->chickPicsImage->addItem(QFileInfo(importedImage).fileName(), importedImage);
                ui_->chickPicsImage->setCurrentIndex(ui_->chickPicsImage->count() - 1);
            }
            emit chickPicsSettingsChanged();
        }
    });
    connect(ui_->chickPicsSize, &QSlider::valueChanged, this, [this](int value) {
        if (config_->saveFarmPackSizePercent(value)) emit chickPicsSettingsChanged();
    });
    connect(ui_->chickPicsOpacity, &QSlider::valueChanged, this, [this](int value) {
        if (config_->saveFarmPackOpacityPercent(value)) emit chickPicsSettingsChanged();
    });
    connect(ui_->resetChickPicsImage, &QPushButton::clicked, this, [this] {
        const auto seasonalIndex = ui_->chickPicsImage->findData(QString());
        if (seasonalIndex >= 0) ui_->chickPicsImage->setCurrentIndex(seasonalIndex);
    });
}

void InstanceWindow::setInstance(const QString& name, bool modded) {
    ui_->detailTitle->setText(name);
    setWindowTitle(name + " — Instance");
    {
        const QSignalBlocker blocker(ui_->launchMode);
        ui_->launchMode->setCurrentIndex(modded ? 1 : 0);
    }
    {
        const QSignalBlocker enabledBlocker(ui_->chickPicsEnabled);
        ui_->chickPicsEnabled->setChecked(config_->farmPackEnabled());
    }
    {
        const QSignalBlocker imageBlocker(ui_->chickPicsImage);
        const auto imagePath = config_->farmPackImagePath();
        const auto imageIndex = ui_->chickPicsImage->findData(imagePath);
        ui_->chickPicsImage->setCurrentIndex(imageIndex >= 0 ? imageIndex : 0);
    }
    {
        const QSignalBlocker sizeBlocker(ui_->chickPicsSize);
        ui_->chickPicsSize->setValue(config_->farmPackSizePercent());
    }
    {
        const QSignalBlocker opacityBlocker(ui_->chickPicsOpacity);
        ui_->chickPicsOpacity->setValue(config_->farmPackOpacityPercent());
    }
    refreshPathStatus();
}

void InstanceWindow::selectTab(int tab) {
    if (tab < 0 || tab >= ui_->navigation->count()) return;
    ui_->navigation->setCurrentRow(tab);
}

void InstanceWindow::refreshPathStatus() {
    const auto directory = config_->gameDirectory();
    ui_->gamePath->setText(directory);
    ui_->pathStatus->setText(
        QFileInfo(directory).isDir()
            ? "Game folder exists. Play checks the selected executable."
            : "Game folder not found. Set your installation path in Settings.");
}

void InstanceWindow::launchGame() {
    const auto directory = config_->gameDirectory();
    const bool modded = ui_->launchMode->currentIndex() == 1;
#ifdef Q_OS_WIN
    const auto binary = modded ? "StardewModdingAPI.exe" : "Stardew Valley.exe";
#elif defined(Q_OS_MACOS)
    const auto binary =
        modded ? "Contents/MacOS/StardewModdingAPI" : "Contents/MacOS/StardewValley";
#else
    const auto binary = modded ? "StardewModdingAPI" : "StardewValley";
#endif
    const auto executable = directory + "/" + binary;
    if (!QFileInfo(executable).isFile() || !QFileInfo(executable).isExecutable()) {
        const auto message = "Executable not found or not runnable:\n" + executable;
        ui_->logs->appendPlainText(message);
        QMessageBox::warning(this, "Unable to launch",
                             message + "\n\nCheck your game path in Settings.");
        return;
    }
    QProcess process;
    process.setProgram(executable);
    process.setWorkingDirectory(directory);
    if (!process.startDetached()) {
        ui_->logs->appendPlainText("Launch failed: " + process.errorString());
        QMessageBox::warning(this, "Unable to launch", process.errorString());
        return;
    }
    ui_->logs->appendPlainText("Started " + executable);
    statusBar()->showMessage("Game launched.");
}
