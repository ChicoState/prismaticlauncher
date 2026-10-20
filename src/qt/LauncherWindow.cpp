#include "LauncherWindow.h"

#include "ui_AddInstanceDialog.h"
#include "ui_LauncherWindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QTextCursor>

namespace {
QIcon instanceIcon() {
    QPixmap art(160, 160);
    art.fill(QColor("#253e39"));
    QPainter painter(&art);
    painter.setRenderHint(QPainter::Antialiasing);
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
    ui_->setupUi(this);
    resize(1120, 760);
    setWindowIcon(instanceIcon());
    statusBar()->showMessage("Local launcher  /  UI preview");

    ui_->configLocation->setText("Configuration file: " + config_.filePath());
    ui_->logs->setPlainText("Prismatic Launcher ready.\nConfiguration: " + config_.filePath());

    connectSignals();
    populateInstances();
    refreshPathStatus();
}

LauncherWindow::~LauncherWindow() = default;

void LauncherWindow::connectSignals() {
    connect(ui_->headerSettings, &QPushButton::clicked, this,
            [this] { openInstance(2); });
    connect(ui_->instanceSearch, &QLineEdit::textChanged, this,
            &LauncherWindow::filterInstances);
    connect(ui_->addInstance, &QPushButton::clicked, this, &LauncherWindow::addInstance);
    connect(ui_->editInstance, &QPushButton::clicked, this,
            [this] { openInstance(); });
    connect(ui_->playInstance, &QPushButton::clicked, this, &LauncherWindow::launchGame);
    connect(ui_->playDetailInstance, &QPushButton::clicked, this,
            &LauncherWindow::launchGame);
    connect(ui_->backToLibrary, &QPushButton::clicked, this,
            [this] { ui_->pages->setCurrentIndex(0); });
    connect(ui_->instances, &QListWidget::itemActivated, this,
            [this] { openInstance(); });
    connect(ui_->instances, &QListWidget::itemDoubleClicked, this,
            [this] { openInstance(); });
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
        if (!config_.saveGameDirectory(ui_->gamePath->text())) {
            ui_->pathStatus->setText(
                "Could not save. Use an absolute path and a writable config location.");
            return;
        }
        refreshPathStatus();
        ui_->logs->appendPlainText("Saved game directory: " + config_.gameDirectory());
        statusBar()->showMessage("Game path saved.");
    });
    connect(ui_->launchMode, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (auto* item = ui_->instances->currentItem()) {
            item->setData(Qt::UserRole + 1, index == 1);
            item->setText(item->data(Qt::UserRole).toString() +
                          (index == 1 ? "\nModded" : "\nVanilla"));
        }
    });
}

void LauncherWindow::populateInstances() {
    auto* item = new QListWidgetItem(instanceIcon(), "Stardew Valley\nVanilla", ui_->instances);
    item->setSizeHint(QSize(192, 224));
    item->setData(Qt::UserRole, "Stardew Valley");
    item->setData(Qt::UserRole + 1, false);
    ui_->instances->setCurrentItem(item);
}

void LauncherWindow::filterInstances(const QString& query) {
    int visible = 0;
    for (int index = 0; index < ui_->instances->count(); ++index) {
        auto* item = ui_->instances->item(index);
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
        if (!item->isHidden()) ++visible;
    }
}

void LauncherWindow::addInstance() {
    QDialog dialog(this);
    Ui::AddInstanceDialog dialogUi;
    dialogUi.setupUi(&dialog);
    dialogUi.buttons->button(QDialogButtonBox::Ok)->setText("Create instance");
    dialogUi.buttons->button(QDialogButtonBox::Ok)->setEnabled(
        !dialogUi.name->text().trimmed().isEmpty());
    connect(dialogUi.name, &QLineEdit::textChanged, &dialog,
            [&dialogUi](const QString& value) {
                dialogUi.buttons->button(QDialogButtonBox::Ok)->setEnabled(
                    !value.trimmed().isEmpty());
            });

    if (dialog.exec() != QDialog::Accepted) return;

    const bool modded = dialogUi.mode->currentIndex() == 1;
    auto* item = new QListWidgetItem(
        instanceIcon(), dialogUi.name->text().trimmed() + (modded ? "\nModded" : "\nVanilla"),
        ui_->instances);
    item->setSizeHint(QSize(192, 224));
    item->setData(Qt::UserRole, dialogUi.name->text().trimmed());
    item->setData(Qt::UserRole + 1, modded);
    ui_->instanceSearch->clear();
    filterInstances({});
    ui_->instances->setCurrentItem(item);
    openInstance();
}

void LauncherWindow::openInstance(int tab) {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }
    ui_->detailTitle->setText(item->data(Qt::UserRole).toString());
    ui_->launchMode->setCurrentIndex(item->data(Qt::UserRole + 1).toBool() ? 1 : 0);
    ui_->navigation->setCurrentRow(tab);
    ui_->pages->setCurrentIndex(1);
}

void LauncherWindow::refreshPathStatus() {
    const auto directory = config_.gameDirectory();
    ui_->gamePath->setText(directory);
    ui_->pathStatus->setText(
        QFileInfo(directory).isDir()
            ? "Game folder exists. Play checks the selected executable."
            : "Game folder not found. Set your installation path in Settings.");
}

void LauncherWindow::launchGame() {
    auto* item = ui_->instances->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }
    const auto directory = config_.gameDirectory();
    const bool modded = item->data(Qt::UserRole + 1).toBool();
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
    // A program and argument list avoid passing user paths through a shell.
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
