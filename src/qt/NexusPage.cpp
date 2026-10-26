#include "NexusPage.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include "ModBrowser.h"

NexusPage::NexusPage(QWidget* parent, bool showBack) : QWidget(parent) {
    setObjectName("nexusPage");
    const auto apiKey = qEnvironmentVariable("NEXUS_API_KEY");
    const bool missingKey = apiKey.trimmed().isEmpty();
    client_ = new NexusModClient(apiKey, this);
    client_->setObjectName("nexusListClient");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(16, showBack ? 8 : 0, 16, 0);
    auto* back = new QPushButton("←  Back to library");
    back->setObjectName("backFromNexus");
    back->setVisible(showBack);
    toolbar->addWidget(back);
    toolbar->addStretch();
    layout->addLayout(toolbar);
    browser_ = new ModBrowser;
    layout->addWidget(browser_, 1);
    connect(back, &QPushButton::clicked, this, &NexusPage::backRequested);
    connect(browser_, &ModBrowser::refreshRequested, this, &NexusPage::fetchMods);
    connect(client_, &NexusModClient::modListFetched, this,
            [this](const QVector<NexusModSummary>& mods) {
                loading_ = false;
                browser_->setMods(mods);
            });
    connect(
        client_, &NexusModClient::requestFailed, this,
        [this, missingKey](const NexusApiError& error) {
            loading_ = false;
            if (missingKey && error.httpStatus == 0) {
                browser_->setListError(
                    "Set NEXUS_API_KEY in the launcher's environment, then restart the launcher.");
            } else if (error.httpStatus > 0) {
                browser_->setListError(
                    QString("Unable to load mods (HTTP %1). Check your API key or try again later.")
                        .arg(error.httpStatus));
            } else {
                browser_->setListError(
                    "Unable to load mods. Check your connection and API key, then refresh.");
            }
        });
    connect(browser_, &ModBrowser::modSelected, browser_,
            [browser = browser_, apiKey,
             detailsClient = QPointer<NexusModClient>()](qint64 id) mutable {
                // A new selection or refresh cancels the previous detail request.
                delete detailsClient.data();
                detailsClient = nullptr;
                if (id <= 0) return;
                detailsClient = new NexusModClient(apiKey, browser);
                connect(detailsClient, &NexusModClient::modDetailsFetched, browser,
                        &ModBrowser::setModDetails);
                connect(detailsClient, &NexusModClient::requestFailed, browser,
                        [browser](const NexusApiError& error) {
                            browser->setDetailsError(error.message);
                        });
                detailsClient->fetchModDetails(id);
            });
}

void NexusPage::fetchMods() {
    if (loading_) return;
    loading_ = true;
    browser_->beginLoading();
    client_->fetchModList();
}
