#include "ModBrowser.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextDocument>
#include <QTextDocumentFragment>
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

QString readableDescription(const QString& text) {
    // Nexus uses BBCode as well as HTML. Match known tags so mod names such as [CP] survive.
    static const QRegularExpression listTags(R"(\[/?list(?:=[^\]\r\n]*)?\])",
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression itemTag(R"(\n?\[\*\])");
    static const QRegularExpression formattingTags(
        R"(\[/?(?:b|i|u|s|strike|size|font|color|center|left|right|justify|url|img|quote|code|spoiler|heading|youtube|video|table|tr|td|th)(?:=[^\]\r\n]*)?\])",
        QRegularExpression::CaseInsensitiveOption);
    // Qt's heuristic can stop at a newline before HTML following a BBCode heading.
    static const QRegularExpression htmlTag(R"(<[A-Za-z][^>]*>)");
    auto plain = (Qt::mightBeRichText(text) || text.contains(htmlTag))
                     ? QTextDocumentFragment::fromHtml(text).toPlainText()
                     : text;
    plain.replace(listTags, "\n");
    plain.replace(itemTag, "\n• ");
    plain.remove(formattingTags);
    return plain;
}
}  // namespace

ModBrowser::ModBrowser(QWidget* parent) : QWidget(parent) {
    setObjectName("modBrowser");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
    auto* heading = new QHBoxLayout;
    auto* headingText = new QVBoxLayout;
    headingText->setSpacing(4);
    headingText->addWidget(textLabel("Nexus mods", "sectionTitle"));
    headingText->addWidget(textLabel("Recently updated on Nexus Mods · Stardew Valley", "muted"));
    heading->addLayout(headingText, 1);
    refresh_ = new QPushButton("Refresh");
    refresh_->setObjectName("modRefresh");
    refresh_->setToolTip("Reload the first page of recently updated Stardew Valley mods.");
    heading->addWidget(refresh_);
    layout->addLayout(heading);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName("modSplitter");
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(8);
    auto* master = new QWidget;
    master->setMinimumWidth(176);
    auto* masterLayout = new QVBoxLayout(master);
    masterLayout->setContentsMargins(0, 0, 0, 0);
    masterLayout->setSpacing(12);
    search_ = new QLineEdit;
    search_->setObjectName("modSearch");
    search_->setPlaceholderText("Filter this page…");
    search_->setAccessibleName("Filter this page by name, author or summary");
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
    pageCount_ = textLabel("", "modPageCount");
    pageCount_->setAccessibleName("Mod page results");
    masterLayout->addWidget(pageCount_);
    auto* pagination = new QHBoxLayout;
    pagination->setSpacing(8);
    previous_ = new QPushButton("Previous");
    previous_->setObjectName("modPrevious");
    previous_->setAccessibleName("Previous page of mods");
    next_ = new QPushButton("Next");
    next_->setObjectName("modNext");
    next_->setAccessibleName("Next page of mods");
    pagination->addWidget(previous_);
    pagination->addWidget(next_);
    masterLayout->addLayout(pagination);
    setPagination(0, 0, 0, false);
    status_ = textLabel("", "modListStatus");
    masterLayout->addWidget(status_);
    splitter->addWidget(master);

    auto* detailPanel = new QWidget;
    detailPanel->setObjectName("modDetailPanel");
    detailPanel->setMinimumWidth(232);
    auto* detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(16, 16, 16, 16);
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

    connect(previous_, &QPushButton::clicked, this, &ModBrowser::previousPageRequested);
    connect(next_, &QPushButton::clicked, this, &ModBrowser::nextPageRequested);
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
    title_->setText("No mod selected");
    author_->clear();
    metadata_->clear();
    summary_->setText("Select a mod to view its description and download page.");
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

void ModBrowser::setPagination(int offset, qint64 total, int count, bool loading) {
    previous_->setEnabled(!loading && offset > 0);
    next_->setEnabled(!loading && count > 0 && offset + count < total);
    pageCount_->setText(count > 0
        ? QString("%1–%2 of %3 mods").arg(QLocale().toString(offset + 1),
              QLocale().toString(offset + count), QLocale().toString(total))
        : QString("0 mods on this page"));
}

void ModBrowser::beginPageLoading() {
    refresh_->setEnabled(false);
    refresh_->setText("Loading…");
    search_->setEnabled(false);
    previous_->setEnabled(false);
    next_->setEnabled(false);
    status_->setText("Loading mod page…");
}

void ModBrowser::setPageError(const QString& message) {
    refresh_->setEnabled(true);
    refresh_->setText("Refresh");
    search_->setEnabled(true);
    status_->setText(message);
}

void ModBrowser::setMods(const QVector<NexusModSummary>& mods) {
    clearDetails();
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
    title_->setText("Mod list unavailable");
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
                     : filtered.isEmpty() ? "No matching mods on this page. Try a different filter."
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
    // Strip API HTML without enabling rich text or links in the label.
    const auto description = readableDescription(mod.description);
    description_->setText(description.isEmpty() ? "No additional description provided."
                                              : description);
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
