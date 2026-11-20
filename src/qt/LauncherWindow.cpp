#include "LauncherWindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QResource>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>

#include "ChickPixImages.h"
#include "InstanceWindow.h"
#include "NexusPage.h"
#include "SmapiDetector.h"
#include "SmapiWindow.h"
#include "settings.h"
#include "ui_AddInstanceDialog.h"
#include "ui_LauncherWindow.h"

const int INSTANCE_WIDTH = 146;
const int INSTANCE_HEIGHT = 162;

namespace {
enum InstanceItemRole {
    InstanceNameRole = Qt::UserRole,
    InstanceModdedRole,
    InstanceDirectoryRole,
};

/// @brief Draws the instance artwork or a built-in fallback icon.
QIcon instanceIcon(const QString& imagePath = {}) {
    QPixmap art(160, 160);
    art.fill(QColor("#253e39"));
    QPainter painter(&art);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPixmap packImage(imagePath);
    if (!packImage.isNull()) {
        const auto image =
            packImage.scaled(144, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap((art.width() - image.width()) / 2, (art.height() - image.height()) / 2,
                           image);
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
    : QMainWindow(parent),
      config_(configFile),
      instances_(InstanceStore::rootForConfig(configFile)),
      ui_(std::make_unique<Ui::LauncherWindow>()) {
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
    statusBar()->showMessage("Local instance library");

    farmPackOverlay_ = new QLabel(ui_->centralWidget);
    farmPackOverlay_->setObjectName("farmPackOverlay");
    farmPackOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    farmPackOverlay_->setStyleSheet("background: transparent;");
    farmPackOpacityEffect_ = new QGraphicsOpacityEffect(farmPackOverlay_);
    farmPackOverlay_->setGraphicsEffect(farmPackOpacityEffect_);
    ui_->centralWidget->installEventFilter(this);

    reloadChickPix();

    connectSignals();
    populateInstances();
    refreshFarmPackOverlay();
}

LauncherWindow::~LauncherWindow() = default;

void LauncherWindow::connectSignals() {
    connect(ui_->settings, &QPushButton::clicked, this, &LauncherWindow::openSettingsWindow);
    connect(ui_->instanceSearch, &QLineEdit::textChanged, this, &LauncherWindow::filterInstances);
    connect(ui_->smapi, &QPushButton::clicked, this, &LauncherWindow::openSmapiWindow);
    connect(ui_->instanceSearch, &QLineEdit::textChanged, this, &LauncherWindow::filterInstances);
    connect(ui_->addInstance, &QPushButton::clicked, this, &LauncherWindow::addInstance);
    connect(ui_->editInstance, &QPushButton::clicked, this, [this] { openInstanceWindow(); });
    connect(ui_->farmPackToggle, &QPushButton::toggled, this, [this](bool enabled) {
        if (!config_.saveFarmPackEnabled(enabled)) {
            const QSignalBlocker blocker(ui_->farmPackToggle);
            ui_->farmPackToggle->setChecked(farmPackEnabled_);
            statusBar()->showMessage("Could not save ChickPix setting.");
            return;
        }
        farmPackEnabled_ = enabled;
        refreshFarmPackOverlay();
        statusBar()->showMessage(enabled ? "ChickPix artwork enabled."
                                         : "ChickPix artwork disabled.");
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
    ui_->instances->clear();
    QString error;
    if (!instances_.initialize(&error)) {
        ui_->addInstance->setEnabled(false);
        statusBar()->showMessage(error);
        return;
    }
    QStringList warnings;
    const auto records = instances_.load(&warnings);
    for (const auto& record : records) appendInstance(record);
    if (ui_->instances->count() > 0) ui_->instances->setCurrentRow(0);
    if (!warnings.isEmpty()) statusBar()->showMessage(warnings.join("\n"));
}

void LauncherWindow::appendInstance(const InstanceRecord& record) {
    auto* item = new QListWidgetItem(
        instanceIcon(), record.name + (record.modded ? "\nModded" : "\nVanilla"), ui_->instances);
    item->setSizeHint(QSize(INSTANCE_WIDTH, INSTANCE_HEIGHT));
    item->setData(InstanceNameRole, record.name);
    item->setData(InstanceModdedRole, record.modded);
    item->setData(InstanceDirectoryRole, record.directory);
}

void LauncherWindow::reloadChickPix() {
    const ChickPixImages images(config_.filePath());
    const auto selection = images.resolveSelection(config_.farmPackImage(), images.catalog());
    auto image = images.load(selection);
    if (image.isNull()) image = images.load(ChickPixImages::defaultImage());
    farmPackImage_ = QPixmap::fromImage(image);
    farmPackLoaded_ = !farmPackImage_.isNull();
    farmPackEnabled_ = config_.farmPackEnabled();
    const QSignalBlocker toggleBlocker(ui_->farmPackToggle);
    ui_->farmPackToggle->setChecked(farmPackEnabled_);
    ui_->farmPackToggle->setEnabled(farmPackLoaded_);
    farmPackOpacityEffect_->setOpacity(config_.farmPackOpacityPercent() / 100.0);
    refreshFarmPackOverlay();
}

void LauncherWindow::refreshFarmPackOverlay() {
    if (!farmPackLoaded_ || !farmPackEnabled_) {
        farmPackOverlay_->hide();
        return;
    }

    const auto* central = ui_->centralWidget;
    const int targetWidth = qBound(220, central->width() * 30 / 100, 420);
    const int targetHeight = qMax(220, central->height() * 55 / 100);
    const auto image = farmPackImage_.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio,
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
    auto* errorLabel = new QLabel(&dialog);
    errorLabel->setObjectName("instanceError");
    errorLabel->setTextFormat(Qt::PlainText);
    errorLabel->setWordWrap(true);
    dialogUi.formLayout->insertRow(3, errorLabel);
    dialogUi.buttons->button(QDialogButtonBox::Ok)->setText("Create instance");
    dialogUi.buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(!dialogUi.name->text().trimmed().isEmpty());
    connect(dialogUi.name, &QLineEdit::textChanged, &dialog, [&dialogUi](const QString& value) {
        dialogUi.buttons->button(QDialogButtonBox::Ok)->setEnabled(!value.trimmed().isEmpty());
    });
    // The generated dialog accepts automatically; wait for a successful filesystem commit instead.
    disconnect(dialogUi.buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    InstanceRecord created;
    connect(dialogUi.buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        QString error;
        if (!instances_.create(dialogUi.name->text(), dialogUi.mode->currentIndex() == 1, &created,
                               &error)) {
            errorLabel->setText(error);
            return;
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted || created.name.isEmpty()) return;
    appendInstance(created);
    ui_->instanceSearch->clear();
    filterInstances({});
    ui_->instances->setCurrentRow(ui_->instances->count() - 1);
    openInstanceWindow();
}

void LauncherWindow::openInstanceWindow(int tab) {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }

    const auto name = item->data(InstanceNameRole).toString();
    const auto modded = item->data(InstanceModdedRole).toBool();
    if (!instanceWindow_) {
        instanceWindow_ = new InstanceWindow(name, modded, &config_, this);
        connect(instanceWindow_, &InstanceWindow::settingsRequested, this,
                &LauncherWindow::openSettingsWindow);
        connect(instanceWindow_, &InstanceWindow::launchModeChanged, this, [this](bool enabled) {
            const auto editedName = instanceWindow_->instanceName();
            for (int row = 0; row < ui_->instances->count(); ++row) {
                auto* edited = ui_->instances->item(row);
                if (edited->data(InstanceNameRole).toString() != editedName) continue;
                QString error;
                if (!instances_.saveMode(editedName, enabled, &error)) {
                    instanceWindow_->setInstance(editedName,
                                                 edited->data(InstanceModdedRole).toBool());
                    statusBar()->showMessage(error);
                    instanceWindow_->statusBar()->showMessage(error);
                    return;
                }
                edited->setData(InstanceModdedRole, enabled);
                edited->setText(editedName + (enabled ? "\nModded" : "\nVanilla"));
                filterInstances(ui_->instanceSearch->text());
                return;
            }
        });
    } else {
        instanceWindow_->setInstance(name, modded);
    }
    instanceWindow_->setInstanceDirectory(item->data(InstanceDirectoryRole).toString());
    instanceWindow_->selectTab(tab);
    instanceWindow_->show();
    instanceWindow_->raise();
    instanceWindow_->activateWindow();
}

void LauncherWindow::openSettingsWindow() {
    if (!settingsWindow_) {
        settingsWindow_ = new settings(&config_, this);
        connect(settingsWindow_, &settings::chickPixChanged, this, &LauncherWindow::reloadChickPix);
        connect(settingsWindow_, &settings::gameDirectoryChanged, this, [this] {
            if (instanceWindow_) instanceWindow_->refreshGameDirectory();
            statusBar()->showMessage("Game path saved.");
        });
        connect(settingsWindow_, &settings::backToLibraryRequested, this, [this] {
            if (instanceWindow_) instanceWindow_->hide();
            show();
            raise();
            activateWindow();
            ui_->settings->setFocus();
        });
    }
    if (!settingsWindow_->isVisible()) settingsWindow_->reload();
    settingsWindow_->show();
    settingsWindow_->raise();
    settingsWindow_->activateWindow();
}

void LauncherWindow::launchGame() {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }
    openInstanceWindow(1);
    instanceWindow_->launchGame();
}

bool LauncherWindow::eventFilter(QObject* watched, QEvent* event) {
    if (farmPackOverlay_ && watched == ui_->centralWidget &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        // The library gets its actual geometry after the main window's resize event.
        refreshFarmPackOverlay();
    }
    return QMainWindow::eventFilter(watched, event);
}
