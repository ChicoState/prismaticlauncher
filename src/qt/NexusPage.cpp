#include "NexusPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Nexus_fetchList.hpp"

NexusPage::NexusPage(QWidget* parent) : QWidget(parent) {
    setObjectName("nexusPage");
    const auto apiKey = qEnvironmentVariable("NEXUS_API_KEY");
    const bool missingKey = apiKey.trimmed().isEmpty();
    client_ = new NexusModClient(apiKey, this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 24);
    layout->setSpacing(16);
    auto* toolbar = new QHBoxLayout;
    auto* back = new QPushButton("←  Back to library");
    back->setObjectName("backFromNexus");
    refresh_ = new QPushButton("Refresh mods");
    refresh_->setObjectName("refreshNexusMods");
    toolbar->addWidget(back);
    toolbar->addStretch();
    toolbar->addWidget(refresh_);
    layout->addLayout(toolbar);
    auto* heading = new QLabel("Nexus mods");
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto* description = new QLabel("The ten most recently updated Stardew Valley mods on Nexus Mods.");
    description->setObjectName("muted");
    description->setWordWrap(true);
    layout->addWidget(description);
    status_ = new QLabel;
    status_->setObjectName("nexusStatus");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    mods_ = new QListWidget;
    mods_->setObjectName("nexusMods");
    mods_->setAccessibleName("Recently updated Nexus mods");
    mods_->setWordWrap(true);
    mods_->setSpacing(8);
    layout->addWidget(mods_, 1);
    connect(back, &QPushButton::clicked, this, &NexusPage::backRequested);
    connect(refresh_, &QPushButton::clicked, this, &NexusPage::fetchMods);
    connect(client_, &NexusModClient::modListFetched, this,
            [this](const QVector<NexusModSummary>& mods) {
                loading_ = false;
                refresh_->setEnabled(true);
                mods_->clear();
                for (const auto& mod : mods) {
                    // QListWidget's standard delegate renders API content as plain text.
                    mods_->addItem(QString("%1\nBy %2 · Version %3 · %4 endorsements\n%5")
                                       .arg(mod.name, mod.author, mod.version)
                                       .arg(mod.endorsementCount)
                                       .arg(mod.summary));
                }
                status_->setText(mods.isEmpty() ? "No mods returned by Nexus Mods."
                                               : QString("Mods loaded: %1").arg(mods.size()));
            });
    connect(client_, &NexusModClient::requestFailed, this, [this, missingKey](const NexusApiError& error) {
        loading_ = false;
        refresh_->setEnabled(true);
        if (missingKey && error.httpStatus == 0) {
            status_->setText("Set NEXUS_API_KEY in the launcher's environment, then restart the launcher.");
        } else if (error.httpStatus > 0) {
            status_->setText(QString("Unable to load mods (HTTP %1). Check your API key or try again later.")
                                 .arg(error.httpStatus));
        } else {
            status_->setText("Unable to load mods. Check your connection and API key, then refresh.");
        }
    });
}

void NexusPage::fetchMods() {
    if (loading_) return;
    loading_ = true;
    refresh_->setEnabled(false);
    mods_->clear();
    status_->setText("Loading mods from Nexus Mods…");
    client_->fetchModList();
}
