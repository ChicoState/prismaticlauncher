#include "InstanceWindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringDecoder>

#include "LogWindow.h"
#include "ui_InstanceWindow.h"

InstanceWindow::InstanceWindow(const QString& name, bool modded, LauncherConfig* config,
                               QWidget* parent)
    : QMainWindow(parent),
      config_(config),
      ui_(std::make_unique<Ui::InstanceWindow>()),
      logWindow_(new LogWindow(this)) {
    ui_->setupUi(this);
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
    connect(ui_->playDetailInstance, &QPushButton::clicked, this, &InstanceWindow::launchGame);
    connect(ui_->openLogs, &QPushButton::clicked, logWindow_, [this] {
        logWindow_->show();
        logWindow_->raise();
        logWindow_->activateWindow();
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
        logWindow_->appendMessage("Saved game directory: " + config_->gameDirectory());
        statusBar()->showMessage("Game path saved.");
    });
    connect(ui_->launchMode, &QComboBox::currentIndexChanged, this,
            [this](int index) { emit launchModeChanged(index == 1); });
}

void InstanceWindow::setInstance(const QString& name, bool modded) {
    ui_->detailTitle->setText(name);
    setWindowTitle(name + " — Instance");
    {
        const QSignalBlocker blocker(ui_->launchMode);
        ui_->launchMode->setCurrentIndex(modded ? 1 : 0);
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
    if (process_ && process_->state() != QProcess::NotRunning) {
        logWindow_->appendMessage("A game is already running from this launcher.",
                                  LogWindow::Severity::Warn);
        logWindow_->show();
        return;
    }
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
        logWindow_->appendMessage(message, LogWindow::Severity::Error);
        QMessageBox::warning(this, "Unable to launch",
                             message + "\n\nCheck your game path in Settings.");
        return;
    }

    // Match the experiment's ownership: destroying a window must not kill the game.
    // The process drains its pipes independently and deletes itself when it exits.
    auto* child = new QProcess;
    process_ = child;
    child->setProgram(executable);
    child->setWorkingDirectory(directory);
    child->setProcessChannelMode(QProcess::MergedChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    if (modded) {
        environment.insert("SMAPI_USE_CURRENT_SHELL", "true");
        // SMAPI's supported developer output includes severity and trace records.
        // Ordinary console prefixes contain only the mod name, losing severity in pipes.
        child->setArguments({"--developer-mode"});
    }
    child->setProcessEnvironment(environment);

    const QPointer<LogWindow> console(logWindow_);
    const auto decoder = std::make_shared<QStringDecoder>(QStringDecoder::Utf8);
    const auto drainOutput = [child, console, decoder] {
        // Keep UTF-8 decoding state across fixed-size reads and readyRead events.
        constexpr qint64 outputChunkBytes = 16384;
        while (child->bytesAvailable() > 0) {
            const QByteArray bytes = child->read(outputChunkBytes);
            if (bytes.isEmpty()) break;
            const QString text = (*decoder)(bytes);
            if (console) console->appendOutput(text);
        }
    };
    connect(child, &QProcess::readyReadStandardOutput, child, drainOutput);
    connect(child, &QProcess::started, this, [this, console, executable] {
        if (console) console->appendMessage("Started " + executable);
        statusBar()->showMessage("Game launched.");
    });
    connect(child, &QProcess::errorOccurred, child, [child, console](QProcess::ProcessError error) {
        if (console) {
            console->flushOutput();
            console->appendMessage("Game process: " + child->errorString(),
                                   LogWindow::Severity::Error);
        }
        if (error == QProcess::FailedToStart) child->deleteLater();
    });
    connect(child, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), child,
            [child, console, drainOutput](int exitCode, QProcess::ExitStatus status) {
                drainOutput();
                if (console) {
                    console->flushOutput();
                    const auto severity = status == QProcess::NormalExit && exitCode == 0
                                              ? LogWindow::Severity::Info
                                              : LogWindow::Severity::Error;
                    console->appendMessage(QString("Game exited with code %1.").arg(exitCode),
                                           severity);
                }
                child->deleteLater();
            });
    logWindow_->beginSession();
    logWindow_->appendMessage("Launching " + executable);
    logWindow_->show();
    child->start(QIODevice::ReadOnly);
}