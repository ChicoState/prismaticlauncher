#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include "LauncherWindow.h"

namespace {
QLabel* label(const QString& text, const QString& role) {
    auto* result = new QLabel(text);
    result->setObjectName(role);
    result->setWordWrap(true);
    result->setTextFormat(Qt::PlainText);
    return result;
}

QWidget* emptyPage(const QString& title, const QString& description) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->addWidget(label(title, "sectionTitle"));
    layout->addWidget(label(description, "muted"));
    layout->addStretch();
    for (auto* message :
         {label("◇", "emptyIcon"), label("Room for your next adventure", "emptyTitle"),
          label("This feature is coming in a future build.", "muted")}) {
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(message);
    }
    layout->addStretch();
    return page;
}
}  // namespace

QWidget* LauncherWindow::createDetails() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(24);
    auto* heading = new QHBoxLayout;
    auto* back = new QPushButton("←  Library");
    back->setObjectName("backToLibrary");
    heading->addWidget(back);
    heading->addSpacing(16);
    detailTitle_ = label("Stardew Valley", "sectionTitle");
    heading->addWidget(detailTitle_);
    heading->addStretch();
    auto* play = new QPushButton("▶  Play instance");
    play->setObjectName("primary");
    heading->addWidget(play);
    layout->addLayout(heading);

    auto* body = new QHBoxLayout;
    body->setSpacing(16);
    auto* sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(176);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(12, 20, 12, 16);
    side->addWidget(label("INSTANCE", "eyebrow"));
    navigation_ = new QListWidget;
    navigation_->setObjectName("navigation");
    navigation_->setAccessibleName("Instance sections");
    navigation_->addItems({"Mods", "Logs", "Settings", "Modpacks"});
    side->addWidget(navigation_, 1);
    side->addWidget(label("STARDEW VALLEY\nLocal installation", "muted"));
    body->addWidget(sidebar);

    detailPages_ = new QStackedWidget;
    detailPages_->setObjectName("detailPages");
    detailPages_->addWidget(createModsPage());
    detailPages_->addWidget(createLogsPage());
    detailPages_->addWidget(createSettingsPage());
    detailPages_->addWidget(
        emptyPage("Modpacks", "Curated collections for a different kind of valley."));
    body->addWidget(detailPages_, 1);
    layout->addLayout(body, 1);
    connect(navigation_, &QListWidget::currentRowChanged, detailPages_,
            &QStackedWidget::setCurrentIndex);
    navigation_->setCurrentRow(0);
    connect(back, &QPushButton::clicked, this, [this] { pages_->setCurrentIndex(0); });
    connect(play, &QPushButton::clicked, this, &LauncherWindow::launchGame);
    return page;
}

QWidget* LauncherWindow::createModsPage() {
    auto* page = emptyPage("Your mods", "A home for the little things that change everything.");
    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    auto* tools = new QHBoxLayout;
    tools->addWidget(label("PREVIEW", "eyebrow"));
    tools->addStretch();
    for (const auto* text : {"Add mod", "Remove", "Update"}) {
        auto* button = new QPushButton(text);
        button->setEnabled(false);
        button->setToolTip("Mod installation and management are not implemented in this UI build.");
        tools->addWidget(button);
    }
    layout->insertLayout(2, tools);
    return page;
}

QWidget* LauncherWindow::createLogsPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addWidget(label("Launcher activity", "sectionTitle"));
    layout->addWidget(
        label("Activity from this session. Game and SMAPI logs are not captured yet.", "muted"));
    auto* filter = new QLineEdit;
    filter->setPlaceholderText("Find in activity…");
    filter->setAccessibleName("Find in launcher activity");
    layout->addWidget(filter);
    logs_ = new QPlainTextEdit;
    logs_->setReadOnly(true);
    logs_->setAccessibleName("Launcher activity log");
    logs_->setPlainText("Prismatic Launcher ready.\nConfiguration: " + config_.filePath());
    layout->addWidget(logs_, 1);
    connect(filter, &QLineEdit::textChanged, this, [this](const QString& query) {
        auto cursor = logs_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        logs_->setTextCursor(cursor);
        if (!query.isEmpty())
            logs_->find(query);
    });
    return page;
}

QWidget* LauncherWindow::createSettingsPage() {
    auto* page = new QWidget;
    page->setMinimumHeight(560);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);
    layout->addWidget(label("Instance settings", "sectionTitle"));
    layout->addWidget(label("Choose how you return to the valley.", "muted"));
    auto* form = new QFormLayout;
    launchMode_ = new QComboBox;
    launchMode_->addItems({"Vanilla", "Modded (SMAPI)"});
    form->addRow("Launch mode", launchMode_);
    gamePath_ = new QLineEdit(config_.gameDirectory());
    gamePath_->setObjectName("gamePath");
    gamePath_->setAccessibleName("Stardew Valley installation directory");
    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(gamePath_, 1);
    auto* browse = new QPushButton("Browse…");
    pathRow->addWidget(browse);
    form->addRow("Game directory", pathRow);
    layout->addLayout(form);
    pathStatus_ = label("", "muted");
    layout->addWidget(pathStatus_);
    auto* save = new QPushButton("Save game path");
    save->setObjectName("savePath");
    layout->addWidget(save, 0, Qt::AlignLeft);
    layout->addWidget(
        label("The game directory is shared by all instances. It starts with the Ubuntu "
              "Steam default; select a different location for other installations.",
              "muted"));
    layout->addWidget(label("SMAPI", "sectionTitle"));
    layout->addWidget(
        label("Modded mode launches an existing SMAPI installation. Automatic SMAPI "
              "installation is not connected in this UI build.",
              "muted"));
    layout->addStretch();
    auto* configLocation = label("Configuration file: " + config_.filePath(), "muted");
    configLocation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(configLocation);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto directory = QFileDialog::getExistingDirectory(
            this, "Select Stardew Valley folder", gamePath_->text());
        if (!directory.isEmpty())
            gamePath_->setText(directory);
    });
    connect(save, &QPushButton::clicked, this, [this] {
        if (!config_.saveGameDirectory(gamePath_->text())) {
            pathStatus_->setText(
                "Could not save. Use an absolute path and a writable config location.");
            return;
        }
        refreshPathStatus();
        logs_->appendPlainText("Saved game directory: " + config_.gameDirectory());
        statusBar()->showMessage("Game path saved.");
    });
    connect(launchMode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (auto* item = instances_->currentItem()) {
            item->setData(Qt::UserRole + 1, index == 1);
            item->setText(item->data(Qt::UserRole).toString() +
                          (index == 1 ? "\nModded" : "\nVanilla"));
        }
    });
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(page);
    return scroll;
}
