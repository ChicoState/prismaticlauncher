#include "LauncherWindow.h"
#include "InstanceWindow.h"
#include "NexusPage.h"
#include "ui_AddInstanceDialog.h"
#include "ui_LauncherWindow.h"

#include <QDialog>
#include <QDate>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QResizeEvent>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QResource>
#include <QStatusBar>

#include "InstanceWindow.h"
#include "SmapiDetector.h"
#include "SmapiWindow.h"
#include "ui_AddInstanceDialog.h"
#include "ui_LauncherWindow.h"

const int INSTANCE_WIDTH = 146;
const int INSTANCE_HEIGHT = 162;

namespace {
QIcon instanceIcon(const QString& imagePath = {}) {
    QPixmap art(160, 160);
    art.fill(QColor("#253e39"));
    QPainter painter(&art);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPixmap packImage(imagePath);
    if (!packImage.isNull()) {
        const auto image = packImage.scaled(144, 144, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        painter.drawPixmap((art.width() - image.width()) / 2,
                           (art.height() - image.height()) / 2, image);
        return QIcon(art);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#72c7ab"));
    painter.drawEllipse(75, 32, 40, 40);
    painter.setBrush(QColor("#3c6655"));
    painter.drawPolygon(QPolygon{{0, 115}, {55, 60}, {110, 115}});
    painter.setBrush(QColor("#527b5f"));
    painter.drawPolygon(QPolygon{{55, 140}, {120, 65}, {180, 140}});
    painter.setBrush(QColor("#d8c495"));
    painter.drawRoundedRect(48, 101, 40, 32, 2, 2);
    painter.setBrush(QColor("#b97656"));
    painter.drawPolygon(QPolygon{{40, 102}, {68, 78}, {96, 102}});
    painter.setBrush(QColor("#253e39"));
    painter.drawRect(64, 113, 10, 20);
    return QIcon(art);
}
}  // namespace

LauncherWindow::LauncherWindow(const QString& configFile, QWidget* parent)
    : QMainWindow(parent), config_(configFile), ui_(std::make_unique<Ui::LauncherWindow>()) {
    Q_INIT_RESOURCE(resources);
    ui_->setupUi(this);
    auto* library = takeCentralWidget();
    auto* pages = new QStackedWidget;
    pages->setObjectName("pages");
    pages->addWidget(library);
    auto* nexus = new NexusPage;
    pages->addWidget(nexus);
    setCentralWidget(pages);
    auto* browse = new QPushButton("Browse Nexus mods", library);
    browse->setObjectName("browseNexusMods");
    ui_->libraryToolbar->insertWidget(3, browse);
    connect(browse, &QPushButton::clicked, this, [pages, nexus] {
        pages->setCurrentWidget(nexus);
        nexus->fetchMods();
    });
    connect(nexus, &NexusPage::backRequested, this, [pages, library, browse] {
        pages->setCurrentWidget(library);
        browse->setFocus();
    });
    resize(1120, 760);
    setWindowIcon(QIcon(":/resources/logo.png"));
    statusBar()->showMessage("Local launcher  /  UI preview");

    farmPackOverlay_ = new QLabel(ui_->centralWidget);
    farmPackOverlay_->setObjectName("farmPackOverlay");
    farmPackOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    farmPackOverlay_->setStyleSheet("background: transparent;");
    farmPackOpacityEffect_ = new QGraphicsOpacityEffect(farmPackOverlay_);
    farmPackOverlay_->setGraphicsEffect(farmPackOpacityEffect_);

    const auto packResult = FarmPack::load(":/farmpack/manifest.json");
    if (packResult.has_value()) {
        farmPack_ = packResult.pack;
        farmPackLoaded_ = true;
        farmPackEnabled_ = config_.farmPackEnabled();
    }
    ui_->farmPackToggle->setChecked(farmPackEnabled_);
    ui_->farmPackToggle->setEnabled(farmPackLoaded_);
    ui_->farmPackOpacity->setValue(config_.farmPackOpacityPercent());
    ui_->farmPackOpacity->setEnabled(farmPackLoaded_);
    ui_->farmPackOpacityLabel->setEnabled(farmPackLoaded_);
    farmPackOpacityEffect_->setOpacity(ui_->farmPackOpacity->value() / 100.0);

    connectSignals();
    populateInstances();
    refreshFarmPackOverlay();
}

LauncherWindow::~LauncherWindow() = default;

void LauncherWindow::connectSignals() {
    connect(ui_->smapi, &QPushButton::clicked, this, &LauncherWindow::openSmapiWindow);
    connect(ui_->instanceSearch, &QLineEdit::textChanged, this, &LauncherWindow::filterInstances);
    connect(ui_->addInstance, &QPushButton::clicked, this, &LauncherWindow::addInstance);
    connect(ui_->editInstance, &QPushButton::clicked, this,
            [this] { openInstanceWindow(); });
    connect(ui_->farmPackToggle, &QPushButton::toggled, this, [this](bool enabled) {
        if (!config_.saveFarmPackEnabled(enabled)) {
            ui_->farmPackToggle->setChecked(farmPackEnabled_);
            statusBar()->showMessage("Could not save Pack setting.");
            return;
        }
        farmPackEnabled_ = enabled;
        refreshFarmPackOverlay();
        statusBar()->showMessage(enabled ? "ChickPix artwork enabled."
                                         : "ChickPix artwork disabled.");
    });
    connect(ui_->farmPackOpacity, &QSlider::valueChanged, this, [this](int value) {
        farmPackOpacityEffect_->setOpacity(value / 100.0);
        if (!config_.saveFarmPackOpacityPercent(value)) {
            ui_->farmPackOpacity->setValue(config_.farmPackOpacityPercent());
            statusBar()->showMessage("Could not save Pack opacity.");
            return;
        }
        refreshFarmPackOverlay();
    });
    connect(ui_->playInstance, &QPushButton::clicked, this, &LauncherWindow::launchGame);
    connect(ui_->instances, &QListWidget::itemActivated, this, [this] { openInstanceWindow(); });
    connect(ui_->instances, &QListWidget::itemDoubleClicked, this,
            [this] { openInstanceWindow(); });
}

void LauncherWindow::openSmapiWindow() {
    if (!smapiWindow_) {
        smapiWindow_ = new SmapiWindow(config_.gameDirectory(), this);
    } else {
        smapiWindow_->setGameDirectory(config_.gameDirectory());
    }
    smapiWindow_->show();
    smapiWindow_->raise();
    smapiWindow_->activateWindow();
}

void LauncherWindow::populateInstances() {
    auto* item = new QListWidgetItem(instanceIcon(), "Stardew Valley\nVanilla",
                                     ui_->instances);
    item->setSizeHint(QSize(INSTANCE_WIDTH, INSTANCE_HEIGHT));
    item->setData(Qt::UserRole, "Stardew Valley");
    item->setData(Qt::UserRole + 1, false);
    ui_->instances->setCurrentItem(item);
}

void LauncherWindow::refreshFarmPackOverlay() {
    if (!farmPackLoaded_ || !farmPackEnabled_) {
        farmPackOverlay_->hide();
        return;
    }

    const QPixmap source(farmPack_.imageForDate(QDate::currentDate()));
    if (source.isNull()) {
        farmPackOverlay_->hide();
        return;
    }

    const auto* central = ui_->centralWidget;
    const int targetWidth = qBound(220, central->width() * 30 / 100, 420);
    const int targetHeight = qMax(220, central->height() * 55 / 100);
    const auto image = source.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
    farmPackOverlay_->setPixmap(image);
    farmPackOverlay_->adjustSize();
    farmPackOverlay_->move(central->width() - farmPackOverlay_->width() - 20,
                           central->height() - farmPackOverlay_->height() - 20);
    farmPackOverlay_->raise();
    farmPackOverlay_->show();
}

void LauncherWindow::filterInstances(const QString& query) {
    for (int index = 0; index < ui_->instances->count(); ++index) {
        auto* item = ui_->instances->item(index);
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
    }
}

void LauncherWindow::addInstance() {
    QDialog dialog(this);
    Ui::AddInstanceDialog dialogUi;
    dialogUi.setupUi(&dialog);
    dialogUi.buttons->button(QDialogButtonBox::Ok)->setText("Create instance");
    dialogUi.buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!dialogUi.name->text().trimmed().isEmpty());
    connect(dialogUi.name, &QLineEdit::textChanged, &dialog, [&dialogUi](const QString& value) {
        dialogUi.buttons->button(QDialogButtonBox::Ok)->setEnabled(!value.trimmed().isEmpty());
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bool modded = dialogUi.mode->currentIndex() == 1;
    auto* item = new QListWidgetItem(
        instanceIcon(),
        dialogUi.name->text().trimmed() + (modded ? "\nModded" : "\nVanilla"),
        ui_->instances);
    item->setSizeHint(QSize(INSTANCE_WIDTH, INSTANCE_HEIGHT));
    item->setData(Qt::UserRole, dialogUi.name->text().trimmed());
    item->setData(Qt::UserRole + 1, modded);
    ui_->instanceSearch->clear();
    filterInstances({});
    ui_->instances->setCurrentItem(item);
    openInstanceWindow();
}

void LauncherWindow::openInstanceWindow(int tab) {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }

    const auto name = item->data(Qt::UserRole).toString();
    const auto modded = item->data(Qt::UserRole + 1).toBool();
    if (!instanceWindow_) {
        instanceWindow_ = new InstanceWindow(name, modded, &config_, this);
        connect(instanceWindow_, &InstanceWindow::launchModeChanged, this, [this](bool enabled) {
            if (auto* current = ui_->instances->currentItem()) {
                current->setData(Qt::UserRole + 1, enabled);
                current->setText(current->data(Qt::UserRole).toString() +
                                 (enabled ? "\nModded" : "\nVanilla"));
            }
        });
    } else {
        instanceWindow_->setInstance(name, modded);
    }
    instanceWindow_->selectTab(tab);
    instanceWindow_->show();
    instanceWindow_->raise();
    instanceWindow_->activateWindow();
}

void LauncherWindow::launchGame() {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }
    const auto directory = config_.gameDirectory();
    const bool modded = item->data(Qt::UserRole + 1).toBool();
    if (modded) {
        const auto smapi = SmapiDetector().inspect(directory);
        if (!smapi.isInstalled()) {
            const auto message = "SMAPI is not installed or incomplete.\n" + smapi.diagnostic;
            QMessageBox::warning(this, "Unable to launch SMAPI", message);
            return;
        }
    }
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
        QMessageBox::warning(this, "Unable to launch",
                             message + "\n\nCheck your game path in Settings.");
        return;
    }
    QProcess process;
    process.setProgram(executable);
    process.setWorkingDirectory(directory);
    if (!process.startDetached()) {
        QMessageBox::warning(this, "Unable to launch", process.errorString());
        return;
    }
    statusBar()->showMessage("Game launched.");
}

void LauncherWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    refreshFarmPackOverlay();
}
