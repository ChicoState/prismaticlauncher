#include "ModInstallPage.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QTableWidget>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

#include "LauncherConfig.h"
#include "ModInstaller.h"

namespace {
bool localDrop(const QMimeData* mime) {
    const auto urls = mime->urls();
    return !urls.isEmpty() && std::all_of(urls.begin(), urls.end(), [](const QUrl& url) {
        return url.isLocalFile() && !url.toLocalFile().isEmpty();
    });
}

QLabel* plainLabel(const QString& text, const QString& name, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
}  // namespace

ModInstallPage::ModInstallPage(LauncherConfig* config, QWidget* parent)
    : QWidget(parent), config_(config) {
    setObjectName("modInstallPage");
    setAcceptDrops(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto* heading = new QHBoxLayout;
    heading->addWidget(plainLabel("Installed mods", "modsTitle", this), 1);
    refresh_ = new QPushButton("Refresh", this);
    refresh_->setObjectName("refreshInstalledMods");
    heading->addWidget(refresh_);
    layout->addLayout(heading);
    destination_ = plainLabel({}, "modInstallDestination", this);
    destination_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(destination_);

    dropZone_ = new QFrame(this);
    dropZone_->setObjectName("modDropZone");
    auto* dropLayout = new QVBoxLayout(dropZone_);
    dropLayout->setContentsMargins(16, 16, 16, 16);
    dropLayout->addWidget(plainLabel("Drop mod downloads here", "modDropTitle", dropZone_));
    dropLayout->addWidget(
        plainLabel("Folders, ZIP, 7z or RAR archives, or a file inside an extracted mod.",
                   "modDropHelp", dropZone_));
    auto* actions = new QHBoxLayout;
    files_ = new QPushButton("Choose files…", dropZone_);
    files_->setObjectName("installModFiles");
    folder_ = new QPushButton("Choose folder…", dropZone_);
    folder_->setObjectName("installModFolder");
    actions->addWidget(files_);
    actions->addWidget(folder_);
    actions->addStretch();
    dropLayout->addLayout(actions);
    layout->addWidget(dropZone_);

    status_ = plainLabel("Reading installed mods…", "modInstallStatus", this);
    layout->addWidget(status_);
    table_ = new QTableWidget(0, 3, this);
    table_->setObjectName("installedMods");
    table_->setAccessibleName("Installed Stardew Valley mods");
    table_->setHorizontalHeaderLabels({"Mod", "Version", "Unique ID"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setDragDropMode(QAbstractItemView::NoDragDrop);
    table_->setAcceptDrops(false);
    table_->viewport()->setAcceptDrops(false);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    layout->addWidget(table_, 1);
    result_ = new QPlainTextEdit(this);
    result_->setObjectName("modInstallResult");
    result_->setAccessibleName("Mod installation result");
    result_->setReadOnly(true);
    result_->setAcceptDrops(false);
    result_->viewport()->setAcceptDrops(false);
    result_->setMaximumHeight(84);
    result_->hide();
    layout->addWidget(result_);
    layout->addWidget(plainLabel(
        "Quit the game before installing. SMAPI and required mods (such as Content Patcher) "
        "must be installed separately. Existing mods are never replaced.",
        "modInstallHelp", this));
    connect(refresh_, &QPushButton::clicked, this, &ModInstallPage::refresh);
    connect(files_, &QPushButton::clicked, this, [this] {
        const auto paths = QFileDialog::getOpenFileNames(
            this, "Choose mod downloads", {}, "Mod archives (*.zip *.7z *.rar);;All files (*)");
        if (!paths.isEmpty()) installSources(paths);
    });
    connect(folder_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, "Choose extracted mod folder");
        if (!path.isEmpty()) installSources({path});
    });
}

ModInstallPage::~ModInstallPage() {
    // A close must not destroy a running filesystem transaction.
    if (worker_) worker_->wait();
}

void ModInstallPage::refresh() { startOperation({}, false); }
void ModInstallPage::installSources(const QStringList& sources) {
    if (!sources.isEmpty()) startOperation(sources, true);
}

void ModInstallPage::startOperation(const QStringList& sources, bool installing) {
    if (isBusy()) return;
    const auto directory = config_->gameDirectory();
    destination_->setText("Installing to: " + ModInstaller::modsDirectory(directory));
    status_->setText(installing ? "Validating and installing mods…" : "Reading installed mods…");
    files_->setEnabled(false);
    folder_->setEnabled(false);
    refresh_->setEnabled(false);
    struct Work {
        ModInstallResult install;
        ModInstallResult listing;
    };
    const auto work = std::make_shared<Work>();
    worker_ = QThread::create([sources, directory, installing, work] {
        if (installing) work->install = ModInstaller::install(sources, directory);
        work->listing = ModInstaller::installed(directory);
    });
    worker_->setParent(this);
    connect(worker_, &QThread::finished, this, [this, directory, installing, work] {
        worker_->deleteLater();
        worker_ = nullptr;
        files_->setEnabled(true);
        folder_->setEnabled(true);
        refresh_->setEnabled(true);
        if (installing) {
            QStringList names;
            for (const auto& mod : work->install.mods) names.append(mod.name);
            const auto message =
                work->install.succeeded()
                    ? QString("Installed %1 mod(s) into %2.\n%3")
                          .arg(work->install.mods.size())
                          .arg(ModInstaller::modsDirectory(directory), names.join(", "))
                    : "Installation failed: " + work->install.error;
            result_->setPlainText(message);
            result_->show();
            emit activity(message);
        }
        const bool changed = config_->gameDirectory() != directory;
        table_->setRowCount(0);
        if (!changed) {
            for (const auto& mod : work->listing.mods) {
                const int row = table_->rowCount();
                table_->insertRow(row);
                const QStringList values{mod.name, mod.version, mod.uniqueId};
                for (int column = 0; column < values.size(); ++column) {
                    auto* item = new QTableWidgetItem(values.at(column));
                    item->setToolTip(mod.directory);
                    table_->setItem(row, column, item);
                }
            }
            if (!work->listing.succeeded()) {
                status_->setText(work->listing.error);
            } else if (table_->rowCount() == 0) {
                status_->setText(
                    "No mods installed. Drop a download or choose files to get started.");
            } else {
                status_->setText(QString("%1 installed mod(s)").arg(table_->rowCount()));
            }
        }
        emit operationFinished((!installing || work->install.succeeded()) &&
                               work->listing.succeeded());
        if (changed) refresh();
    });
    worker_->start();
}

void ModInstallPage::setDropActive(bool active) {
    dropZone_->setProperty("dropActive", active);
    dropZone_->style()->unpolish(dropZone_);
    dropZone_->style()->polish(dropZone_);
    dropZone_->update();
}

void ModInstallPage::dragEnterEvent(QDragEnterEvent* event) {
    if (!isBusy() && localDrop(event->mimeData()) && (event->possibleActions() & Qt::CopyAction)) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        setDropActive(true);
    }
}
void ModInstallPage::dragMoveEvent(QDragMoveEvent* event) {
    if (!isBusy() && localDrop(event->mimeData()) && (event->possibleActions() & Qt::CopyAction)) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
}
void ModInstallPage::dragLeaveEvent(QDragLeaveEvent* event) {
    setDropActive(false);
    event->accept();
}
void ModInstallPage::dropEvent(QDropEvent* event) {
    setDropActive(false);
    if (isBusy() || !localDrop(event->mimeData()) || !(event->possibleActions() & Qt::CopyAction)) {
        event->ignore();
        return;
    }
    QStringList sources;
    for (const auto& url : event->mimeData()->urls()) sources.append(url.toLocalFile());
    event->setDropAction(Qt::CopyAction);
    event->accept();
    installSources(sources);
}
void ModInstallPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
}
