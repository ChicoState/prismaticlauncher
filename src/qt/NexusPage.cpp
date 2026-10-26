#include "NexusPage.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include "ModBrowser.h"

NexusPage::NexusPage(QWidget* parent, bool showBack, QNetworkAccessManager* networkManager)
    : QWidget(parent) {
    setObjectName("nexusPage");
    const auto apiKey = qEnvironmentVariable("NEXUS_API_KEY");
    const bool missingKey = apiKey.trimmed().isEmpty();
    client_ = new NexusModClient(apiKey, this, networkManager);
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
    connect(browser_, &ModBrowser::previousPageRequested, this, [this] {
        if (offset_ > 0) requestPage(qMax(0, offset_ - NexusModClient::pageSize));
    });
    connect(browser_, &ModBrowser::nextPageRequested, this, [this] {
        if (returnedCount_ > 0 && offset_ + returnedCount_ < totalCount_)
            requestPage(offset_ + returnedCount_);
    });
    connect(client_, &NexusModClient::modPageFetched, this, [this](const NexusModPage& page) {
        if (page.offset != requestedOffset_) return;
        loading_ = false;
        hasPage_ = true;
        offset_ = page.offset;
        returnedCount_ = static_cast<int>(page.mods.size());
        totalCount_ = page.totalCount;
        browser_->setMods(page.mods);
        updatePagination();
    });
    connect(
        client_, &NexusModClient::requestFailed, this,
        [this, missingKey](const NexusApiError& error) {
            loading_ = false;
            QString message;
            if (missingKey && error.httpStatus == 0) {
                message = "Set NEXUS_API_KEY in the launcher's environment, then restart the launcher.";
            } else if (error.httpStatus >= 400) {
                message = QString("Unable to load mod page (HTTP %1). Check your API key or retry.")
                              .arg(error.httpStatus);
            } else {
                message = error.message + " Check your connection and API key, then retry.";
            }
            if (hasPage_) browser_->setPageError(message + " Your current page is still available.");
            else browser_->setListError(message);
            updatePagination();
        });
    connect(browser_, &ModBrowser::modSelected, browser_,
            [browser = browser_, apiKey, networkManager,
             detailsClient = QPointer<NexusModClient>()](qint64 id) mutable {
                // A new selection or refresh cancels the previous detail request.
                auto* previousDetails = detailsClient.data();
                detailsClient.clear();
                delete previousDetails;
                if (id <= 0) return;
                detailsClient = new NexusModClient(apiKey, browser, networkManager);
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
    requestPage(0);
}

void NexusPage::requestPage(int offset) {
    if (loading_) return;
    loading_ = true;
    requestedOffset_ = offset;
    if (hasPage_) browser_->beginPageLoading();
    else browser_->beginLoading();
    updatePagination();
    client_->fetchModPage(offset);
}

void NexusPage::updatePagination() {
    browser_->setPagination(offset_, totalCount_, returnedCount_, loading_);
}
