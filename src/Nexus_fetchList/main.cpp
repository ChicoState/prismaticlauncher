#include "Nexus_fetchList.hpp"

#include <QCoreApplication>
#include <QDebug>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>

namespace {

void printModDetails(const NexusModDetails& mod)
{
    qInfo() << "Mod" << mod.id << ':' << mod.name;
    qInfo() << "  Summary:" << mod.summary;
    qInfo() << "  Version:" << mod.version << "Author:" << mod.author;
    qInfo() << "  Uploaded by:" << mod.uploadedBy << "Category:" << mod.category;
    qInfo() << "  Endorsements:" << mod.endorsementCount << "Downloads:" << mod.downloadCount
            << "Unique downloads:" << mod.uniqueDownloadCount;
    qInfo() << "  Created:" << mod.createdAt << "Updated:" << mod.updatedAt;
    qInfo() << "  Adult content:" << mod.adultContent << "Available:" << mod.available;
    qInfo() << "  Page URL:" << mod.pageUrl << "Picture URL:" << mod.pictureUrl;
    qInfo() << "  Description:" << mod.description;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    NexusModClient client(qEnvironmentVariable("NEXUS_API_KEY"));
    int exitCode = EXIT_SUCCESS;
    auto detailIds = std::make_shared<QVector<qint64>>();
    auto nextDetailIndex = std::make_shared<qsizetype>(0);
    auto fetchNextDetail = std::make_shared<std::function<void()>>();

    *fetchNextDetail = [&application, &client, detailIds, nextDetailIndex, fetchNextDetail] {
        if (*nextDetailIndex >= detailIds->size()) {
            application.quit();
            return;
        }

        client.fetchModDetails(detailIds->at(*nextDetailIndex));
    };

    QObject::connect(&client,
                     &NexusModClient::modListFetched,
                     &application,
                     [&application, detailIds, fetchNextDetail](
                         const QVector<NexusModSummary>& mods) {
                         qInfo() << "Fetched" << mods.size() << "mods.";

                         const qsizetype detailCount = std::min<qsizetype>(mods.size(), 10);
                         detailIds->reserve(detailCount);
                         for (qsizetype index = 0; index < detailCount; ++index) {
                             detailIds->append(mods.at(index).id);
                         }

                         if (detailIds->isEmpty()) {
                             application.quit();
                             return;
                         }

                         qInfo() << "Fetching details for" << detailIds->size() << "mods.";
                         (*fetchNextDetail)();
                     });
    QObject::connect(&client,
                     &NexusModClient::modDetailsFetched,
                     &application,
                     [nextDetailIndex, fetchNextDetail](const NexusModDetails& mod) {
                         printModDetails(mod);
                         ++*nextDetailIndex;
                         (*fetchNextDetail)();
                     });
    QObject::connect(&client,
                     &NexusModClient::requestFailed,
                     &application,
                     [&application, &exitCode](const NexusApiError& error) {
                         qCritical() << "Failed to fetch Nexus data:" << error.message
                                     << "(HTTP status" << error.httpStatus << ")";
                         exitCode = EXIT_FAILURE;
                         application.quit();
                     });

    client.fetchModList();
    if (exitCode != EXIT_SUCCESS) {
        return exitCode;
    }

    const int eventLoopExitCode = application.exec();
    return exitCode == EXIT_SUCCESS ? eventLoopExitCode : exitCode;
}
