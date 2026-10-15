#include "LauncherWindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

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
    : QMainWindow(parent), config_(configFile) {
    setWindowTitle("Prismatic Launcher");
    resize(1120, 760);
    setMinimumSize(820, 600);
    setWindowIcon(instanceIcon());
    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget;
    header->setObjectName("header");
    auto* bar = new QHBoxLayout(header);
    bar->setContentsMargins(24, 16, 24, 16);
    auto* mark = new QLabel("◈");
    mark->setObjectName("brandMark");
    auto* brand = new QLabel("PRISMATIC");
    brand->setObjectName("brand");
    bar->addWidget(mark);
    bar->addWidget(brand);
    bar->addStretch();
    auto* settings = new QPushButton("Settings");
    settings->setAccessibleName("Launcher settings");
    bar->addWidget(settings);
    layout->addWidget(header);

    pages_ = new QStackedWidget;
    pages_->setObjectName("pages");
    pages_->addWidget(createLibrary());
    pages_->addWidget(createDetails());
    layout->addWidget(pages_, 1);
    setCentralWidget(root);
    statusBar()->showMessage("Local launcher  /  UI preview");
    connect(settings, &QPushButton::clicked, this, [this] { openInstance(2); });
    refreshPathStatus();
}

QWidget* LauncherWindow::createLibrary() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 24);
    layout->setSpacing(16);
    auto* eyebrow = new QLabel("YOUR COLLECTION");
    eyebrow->setObjectName("eyebrow");
    layout->addWidget(eyebrow);
    auto* heading = new QLabel("Make the valley yours.");
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto* description = new QLabel("Your Stardew Valley instances, all in one place.");
    description->setObjectName("muted");
    layout->addWidget(description);

    auto* toolbar = new QHBoxLayout;
    auto* add = new QPushButton("+  Add instance");
    add->setObjectName("primary");
    auto* play = new QPushButton("▶  Play");
    auto* edit = new QPushButton("Edit instance");
    edit->setObjectName("editInstance");
    toolbar->addWidget(add);
    toolbar->addWidget(play);
    toolbar->addWidget(edit);
    toolbar->addStretch();
    search_ = new QLineEdit;
    search_->setObjectName("instanceSearch");
    search_->setPlaceholderText("Search instances…");
    search_->setAccessibleName("Search instances");
    search_->setMaximumWidth(240);
    toolbar->addWidget(search_);
    layout->addLayout(toolbar);

    instanceCount_ = new QLabel("01 INSTANCE");
    instanceCount_->setObjectName("eyebrow");
    layout->addWidget(instanceCount_);
    instances_ = new QListWidget;
    instances_->setObjectName("instances");
    instances_->setAccessibleName("Game instances. Double click to open details.");
    instances_->setViewMode(QListView::IconMode);
    instances_->setMovement(QListView::Static);
    instances_->setResizeMode(QListView::Adjust);
    instances_->setWordWrap(true);
    instances_->setTextElideMode(Qt::ElideNone);
    instances_->setIconSize(QSize(144, 144));
    instances_->setGridSize(QSize(208, 224));
    instances_->setSpacing(12);
    auto* item = new QListWidgetItem(instanceIcon(), "Stardew Valley\nVanilla", instances_);
    item->setSizeHint(QSize(192, 224));
    item->setData(Qt::UserRole, "Stardew Valley");
    item->setData(Qt::UserRole + 1, false);
    instances_->setCurrentItem(item);
    layout->addWidget(instances_, 1);
    auto* hint =
        new QLabel("A fresh start, or a new adventure. Double-click an instance to explore.");
    hint->setObjectName("muted");
    layout->addWidget(hint);

    connect(search_, &QLineEdit::textChanged, this, &LauncherWindow::filterInstances);
    connect(add, &QPushButton::clicked, this, &LauncherWindow::addInstance);
    connect(edit, &QPushButton::clicked, this, [this] { openInstance(); });
    connect(play, &QPushButton::clicked, this, &LauncherWindow::launchGame);
    connect(instances_, &QListWidget::itemActivated, this, [this] { openInstance(); });
    connect(instances_, &QListWidget::itemDoubleClicked, this, [this] { openInstance(); });
    return page;
}

void LauncherWindow::filterInstances(const QString& query) {
    int visible = 0;
    for (int index = 0; index < instances_->count(); ++index) {
        auto* item = instances_->item(index);
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
        if (!item->isHidden())
            ++visible;
    }
    instanceCount_->setText(visible == 0 ? "NO MATCHING INSTANCES"
                                         : QString("%1 INSTANCE(S)").arg(visible));
}

void LauncherWindow::addInstance() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add instance");
    dialog.setMinimumWidth(420);
    QFormLayout form(&dialog);
    QLineEdit name("My Stardew Valley");
    name.setMaxLength(64);
    QComboBox mode;
    mode.addItems({"Vanilla", "Modded (requires installed SMAPI)"});
    QLabel note(
        "Instances are session-only UI entries and share the configured game folder.\n"
        "Creating one does not copy game files or install SMAPI.");
    note.setWordWrap(true);
    form.addRow("Name", &name);
    form.addRow("Launch mode", &mode);
    form.addRow(&note);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Create instance");
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(&name, &QLineEdit::textChanged, &dialog, [&buttons](const QString& value) {
        buttons.button(QDialogButtonBox::Ok)->setEnabled(!value.trimmed().isEmpty());
    });
    if (dialog.exec() != QDialog::Accepted)
        return;
    const bool modded = mode.currentIndex() == 1;
    auto* item = new QListWidgetItem(
        instanceIcon(), name.text().trimmed() + (modded ? "\nModded" : "\nVanilla"), instances_);
    item->setSizeHint(QSize(192, 224));
    item->setData(Qt::UserRole, name.text().trimmed());
    item->setData(Qt::UserRole + 1, modded);
    search_->clear();
    filterInstances({});
    instances_->setCurrentItem(item);
    openInstance();
}

void LauncherWindow::openInstance(int tab) {
    auto* item = instances_->currentItem();
    if (!item || item->isHidden()) {
        statusBar()->showMessage("Select an instance first.");
        return;
    }
    detailTitle_->setText(item->data(Qt::UserRole).toString());
    launchMode_->setCurrentIndex(item->data(Qt::UserRole + 1).toBool() ? 1 : 0);
    navigation_->setCurrentRow(tab);
    pages_->setCurrentIndex(1);
}

void LauncherWindow::refreshPathStatus() {
    const auto directory = config_.gameDirectory();
    gamePath_->setText(directory);
    pathStatus_->setText(QFileInfo(directory).isDir()
                             ? "Game folder exists. Play checks the selected executable."
                             : "Game folder not found. Set your installation path in Settings.");
}

void LauncherWindow::launchGame() {
    auto* item = instances_->currentItem();
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
        logs_->appendPlainText(message);
        QMessageBox::warning(this, "Unable to launch",
                             message + "\n\nCheck your game path in Settings.");
        return;
    }
    // A program and argument list avoid passing user paths through a shell.
    QProcess process;
    process.setProgram(executable);
    process.setWorkingDirectory(directory);
    if (!process.startDetached()) {
        logs_->appendPlainText("Launch failed: " + process.errorString());
        QMessageBox::warning(this, "Unable to launch", process.errorString());
        return;
    }
    logs_->appendPlainText("Started " + executable);
    statusBar()->showMessage("Game launched.");
}
