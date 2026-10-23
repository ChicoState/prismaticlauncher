#include "ModBrowser.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
QLabel* textLabel(const QString& text, const QString& name) {
    auto* label = new QLabel(text);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
}  // namespace

ModBrowser::ModBrowser(QWidget* parent) : QWidget(parent) {
    setObjectName("modBrowser");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto* heading = new QHBoxLayout;
    auto* headingText = new QVBoxLayout;
    headingText->setSpacing(4);
    headingText->addWidget(textLabel("Discover mods", "sectionTitle"));
    headingText->addWidget(textLabel("Recently updated on Nexus Mods · Stardew Valley", "muted"));
    heading->addLayout(headingText, 1);
    refresh_ = new QPushButton("Refresh");
    refresh_->setObjectName("modRefresh");
    refresh_->setToolTip("Fetch the ten most recently updated Stardew Valley mods.");
    heading->addWidget(refresh_);
    layout->addLayout(heading);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName("modSplitter");
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);
    auto* master = new QWidget;
    master->setMinimumWidth(176);
    auto* masterLayout = new QVBoxLayout(master);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(12);
    search_ = new QLineEdit;
    search_->setObjectName("modSearch");
    search_->setPlaceholderText("Filter these mods…");
    search_->setAccessibleName("Filter recently updated mods by name, author or summary");
    search_->setClearButtonEnabled(true);
    masterLayout->addWidget(search_);
    count_ = textLabel("RECENTLY UPDATED", "eyebrow");
    masterLayout->addWidget(count_);
    list_ = new QListWidget;
    list_->setObjectName("modList");
    list_->setAccessibleName("Mods. Select a name to view details.");
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setTextElideMode(Qt::ElideRight);
    list_->setMinimumHeight(120);
    masterLayout->addWidget(list_, 1);
    status_ = textLabel("", "modListStatus");
    masterLayout->addWidget(status_);
    splitter->addWidget(master);

    auto* detailPanel = new QWidget;
    detailPanel->setObjectName("modDetailPanel");
    detailPanel->setMinimumWidth(232);
    auto* detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    detailLayout->setSpacing(16);
    details_ = new QScrollArea;
    details_->setObjectName("modDetailScroll");
    details_->setWidgetResizable(true);
    details_->setFrameShape(QFrame::NoFrame);
    details_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;
    content->setObjectName("modDetailContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 8, 0);
    contentLayout->setSpacing(16);
    contentLayout->addWidget(textLabel("MOD DETAILS", "eyebrow"));
    title_ = textLabel("", "modTitle");
    author_ = textLabel("", "modAuthor");
    metadata_ = textLabel("", "modMetadata");
    summary_ = textLabel("", "modSummary");
    description_ = textLabel("", "modDescription");
    detailStatus_ = textLabel("", "modDetailStatus");
    for (auto* label : {title_, author_, metadata_, summary_, description_, detailStatus_}) {
        contentLayout->addWidget(label);
    }
    contentLayout->addStretch();
    details_->setWidget(content);
    detailLayout->addWidget(details_, 1);
    auto* footer = new QVBoxLayout;
    footer->setSpacing(12);
    footer->addWidget(
        textLabel("Downloads open in your browser. Nothing is installed automatically.", "muted"));
    download_ = new QPushButton("Open download page  ↗");
    download_->setObjectName("modDownload");
    download_->setAccessibleName("Open selected mod's download page on Nexus Mods");
    footer->addWidget(download_, 0, Qt::AlignRight);
    detailLayout->addLayout(footer);
    splitter->addWidget(detailPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({280, 440});
    layout->addWidget(splitter, 1);

    connect(refresh_, &QPushButton::clicked, this, &ModBrowser::refreshRequested);
    connect(search_, &QLineEdit::textChanged, this, &ModBrowser::filterMods);
    connect(list_, &QListWidget::currentRowChanged, this, &ModBrowser::selectMod);
    connect(download_, &QPushButton::clicked, this, [this] {
        if (selectedId_ <= 0 || !download_->isEnabled()) return;
        // Build an HTTPS destination from the ID, never from untrusted API text.
        const QUrl url(
            QString("https://www.nexusmods.com/stardewvalley/mods/%1?tab=files").arg(selectedId_));
        if (!QDesktopServices::openUrl(url)) {
            detailStatus_->setText("Could not open your browser. Please try again.");
            detailStatus_->show();
        }
    });
    clearDetails();
}

void ModBrowser::clearDetails() {
    selectedId_ = 0;
    title_->setText("Find your next favorite.");
    author_->clear();
    metadata_->clear();
    summary_->setText("Select a mod on the left to see what it adds to your valley.");
    description_->clear();
    detailStatus_->clear();
    for (auto* optional : {author_, metadata_, description_, detailStatus_}) optional->hide();
    download_->setEnabled(false);
    download_->setToolTip("Select a mod to open its download page.");
    emit modSelected(0);
}

void ModBrowser::beginLoading() {
    mods_.clear();
    list_->clear();
    clearDetails();
    refresh_->setEnabled(false);
    refresh_->setText("Refreshing…");
    search_->setEnabled(false);
    count_->setText("RECENTLY UPDATED");
    status_->setText("Loading recently updated mods…");
}

void ModBrowser::setMods(const QVector<NexusModSummary>& mods) {
    mods_ = mods;
    refresh_->setEnabled(true);
    refresh_->setText("Refresh");
    search_->setEnabled(true);
    filterMods();
}

void ModBrowser::setListError(const QString& message) {
    setMods({});
    // Keep actionable API/connection guidance visible until the user retries.
    search_->setEnabled(false);
    status_->setText(message);
    title_->setText("Your next adventure can wait.");
    summary_->setText("Once the mod list is available, select a name to explore it here.");
}

void ModBrowser::filterMods() {
    const auto previousId = selectedId_;
    const auto filtered = NexusModClient::searchMods(mods_, search_->text());
    {
        const QSignalBlocker blocker(list_);
        list_->clear();
        for (const auto& mod : filtered) {
            auto* item = new QListWidgetItem(mod.name, list_);
            item->setData(Qt::UserRole, mod.id);
            item->setToolTip(mod.name);
            if (mod.id == previousId && mod.id > 0) list_->setCurrentItem(item);
        }
    }
    count_->setText(QString("%1 OF %2 MODS").arg(filtered.size()).arg(mods_.size()));
    status_->setText(mods_.isEmpty()      ? "No mods returned. Try refreshing in a moment."
                     : filtered.isEmpty() ? "No matching mods. Try a different filter."
                                          : "Select a name to explore.");
    if (!list_->currentItem()) clearDetails();
}

void ModBrowser::showSummary(const NexusModSummary& mod) {
    author_->show();
    metadata_->show();
    title_->setText(mod.name);
    author_->setText(mod.author.isEmpty() ? "Author not listed" : "By " + mod.author);
    metadata_->setText(QString("Version %1  ·  %2 endorsements")
                           .arg(mod.version.isEmpty() ? "not listed" : mod.version,
                                QLocale().toString(mod.endorsementCount)));
    summary_->setText(mod.summary.isEmpty() ? "No summary provided." : mod.summary);
}

void ModBrowser::selectMod() {
    const auto* item = list_->currentItem();
    if (!item) {
        clearDetails();
        return;
    }
    selectedId_ = item->data(Qt::UserRole).toLongLong();
    for (const auto& mod : mods_) {
        if (mod.id != selectedId_) continue;
        showSummary(mod);
        description_->clear();
        description_->hide();
        detailStatus_->setText("Loading full details…");
        detailStatus_->show();
        download_->setEnabled(selectedId_ > 0);
        download_->setToolTip("View this mod's Files tab on Nexus Mods.");
        details_->verticalScrollBar()->setValue(0);
        emit modSelected(selectedId_);
        return;
    }
    clearDetails();
}

void ModBrowser::setModDetails(const NexusModDetails& mod) {
    if (selectedId_ <= 0 || mod.id != selectedId_) return;
    showSummary(mod);
    description_->setText(mod.description.isEmpty() ? "No additional description provided."
                                                    : mod.description);
    description_->show();
    auto metadata = metadata_->text();
    metadata += QString("\n%1 downloads").arg(QLocale().toString(mod.downloadCount));
    if (mod.updatedAt.isValid()) {
        metadata +=
            "  ·  Updated " + QLocale().toString(mod.updatedAt.date(), QLocale::ShortFormat);
    }
    metadata_->setText(metadata);
    detailStatus_->setText(mod.available ? "" : "This mod is currently unavailable on Nexus Mods.");
    detailStatus_->setVisible(!mod.available);
    download_->setEnabled(mod.available);
    if (!mod.available) download_->setToolTip("This mod is currently unavailable.");
}

void ModBrowser::setDetailsError(const QString& message) {
    if (selectedId_ <= 0) return;
    detailStatus_->show();
    detailStatus_->setText("Full details could not be loaded. " + message +
                           " You can still visit the download page.");
}
