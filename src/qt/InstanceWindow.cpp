#include "InstanceWindow.h"

#include <QComboBox>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringDecoder>

#include "LogWindow.h"
#include "NexusPage.h"
#include "SmapiDetector.h"
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
    mods_ = new NexusPage(ui_->modsPage, false);
    ui_->modsLayout->addWidget(mods_);
    // Keep the sidebar compact so both mod panes fit at the minimum window size.
    ui_->navigation->setFixedWidth(120);
    ui_->bodyLayout->setStretch(1, 1);
    connectSignals();
    setInstance(name, modded);
}

InstanceWindow::~InstanceWindow() = default;

void InstanceWindow::connectSignals() {
    connect(ui_->navigation, &QListWidget::currentRowChanged, ui_->detailPages,
            &QStackedWidget::setCurrentIndex);
    connect(ui_->navigation, &QListWidget::currentRowChanged, this, [this](int tab) {
        if (tab == 0 && !modsLoaded_) {
            modsLoaded_ = true;
            mods_->fetchMods();
        }
    });
    connect(ui_->backToLibrary, &QPushButton::clicked, this, &QWidget::close);
    connect(ui_->playDetailInstance, &QPushButton::clicked, this, &InstanceWindow::launchGame);
    connect(ui_->openLogs, &QPushButton::clicked, logWindow_, [this] {
        logWindow_->show();
        logWindow_->raise();
        logWindow_->activateWindow();
    });
    connect(ui_->openSettings, &QPushButton::clicked, this, &InstanceWindow::settingsRequested);
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
    if (tab == 0 && !modsLoaded_) {
        modsLoaded_ = true;
        mods_->fetchMods();
    }
}

void InstanceWindow::refreshPathStatus() {
    const auto directory = config_->gameDirectory();
    const auto message =
        QFileInfo(directory).isDir()
            ? QString("Game folder exists. Play checks the selected executable.")
            : QString("Game folder not found. Set your installation path in launcher settings.");
    ui_->pathStatus->setText(directory + "\n" + message);
}

void InstanceWindow::refreshGameDirectory() {
    refreshPathStatus();
    logWindow_->appendMessage("Saved game directory: " + config_->gameDirectory());
    statusBar()->showMessage("Game path saved.");
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
    if (modded) {
        const auto smapi = SmapiDetector().inspect(directory);
        if (!smapi.isInstalled()) {
            const auto message = "SMAPI is not installed or incomplete.\n" + smapi.diagnostic;
            logWindow_->appendMessage(message, LogWindow::Severity::Error);
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
