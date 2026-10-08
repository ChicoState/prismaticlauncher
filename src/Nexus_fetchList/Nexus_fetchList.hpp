#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringView>
#include <QUrl>
#include <QVector>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

struct NexusModSummary {
    qint64 id{};
    QString name;
    QString summary;
    QString version;
    QString author;
    QUrl pageUrl;
    QUrl pictureUrl;
    qint64 endorsementCount{};

    bool operator==(const NexusModSummary&) const = default;
};

struct NexusModDetails : NexusModSummary {
    QString description;
    QString uploadedBy;
    QString category;
    QDateTime createdAt;
    QDateTime updatedAt;
    qint64 downloadCount{};
    qint64 uniqueDownloadCount{};
    bool adultContent{};
    bool available{};
};

struct NexusApiError {
    QString message;
    int httpStatus{};
};

Q_DECLARE_METATYPE(NexusModSummary)
Q_DECLARE_METATYPE(QVector<NexusModSummary>)
Q_DECLARE_METATYPE(NexusModDetails)
Q_DECLARE_METATYPE(NexusApiError)

// Read-only Nexus Mods client for the Stardew Valley game domain.
// The caller owns API-key storage and must never log the supplied key.
class NexusModClient final : public QObject {
    Q_OBJECT

public:
    explicit NexusModClient(QString apiKey, QObject* parent = nullptr);

    // Fetches the legacy API's ten most recently updated Stardew Valley mods.
    // The period-based updated.json endpoint returns only IDs and timestamps;
    // this endpoint returns the complete summaries required by NexusModSummary.
    QNetworkReply* fetchModList();
    QNetworkReply* fetchModDetails(qint64 modId);

    // Filters an already-fetched list by name, summary, or author.
    [[nodiscard]] static QVector<NexusModSummary> searchMods(const QVector<NexusModSummary>& mods,
                                                              QStringView query);

    // Parsing is public so callers can test and cache API responses without network I/O.
    [[nodiscard]] static std::optional<QVector<NexusModSummary>> parseModListResponse(
        const QByteArray& response, QString* error = nullptr);
    [[nodiscard]] static std::optional<NexusModDetails> parseModDetailsResponse(
        const QByteArray& response, QString* error = nullptr);

signals:
    void modListFetched(QVector<NexusModSummary> mods);
    void modDetailsFetched(NexusModDetails mod);
    void requestFailed(NexusApiError error);

private:
    [[nodiscard]] QNetworkReply* get(const QUrl& url);
    void handleModListReply(QNetworkReply* reply);
    void handleModDetailsReply(QNetworkReply* reply);

    QNetworkAccessManager* networkManager_;
    QString apiKey_;
};
