#include "Nexus_fetchList.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <nlohmann/json.hpp>

#include <utility>

namespace {

using Json = nlohmann::json;

constexpr auto kApiBaseUrl = "https://api.nexusmods.com/v1/games/stardewvalley/mods/";

void setError(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

bool readRequiredId(const Json& object, qint64* value, QString* error)
{
    const auto id = object.find("mod_id");
    if (id == object.end() || (!id->is_number_integer() && !id->is_number_unsigned())) {
        setError(error, QStringLiteral("Nexus response is missing an integer mod_id."));
        return false;
    }

    *value = id->get<qint64>();
    return true;
}

bool readOptionalString(const Json& object, const char* key, QString* value, QString* error)
{
    const auto field = object.find(key);
    if (field == object.end() || field->is_null()) {
        value->clear();
        return true;
    }
    if (!field->is_string()) {
        setError(error, QStringLiteral("Nexus response has a non-string %1.").arg(QString::fromUtf8(key)));
        return false;
    }

    *value = QString::fromStdString(field->get<std::string>());
    return true;
}

bool readOptionalInteger(const Json& object, const char* key, qint64* value, QString* error)
{
    const auto field = object.find(key);
    if (field == object.end() || field->is_null()) {
        *value = 0;
        return true;
    }
    if (field->is_string()) {
        bool parsed{};
        const QString text = QString::fromStdString(field->get<std::string>());
        const qint64 numericValue = text.toLongLong(&parsed);
        if (parsed) {
            *value = numericValue;
            return true;
        }
    }
    if (!field->is_number_integer() && !field->is_number_unsigned()) {
        setError(error, QStringLiteral("Nexus response has a non-integer %1.").arg(QString::fromUtf8(key)));
        return false;
    }

    *value = field->get<qint64>();
    return true;
}

bool readOptionalTimestamp(const Json& object, const char* key, qint64* value, QString* error)
{
    const auto field = object.find(key);
    if (field == object.end() || field->is_null()) {
        *value = 0;
        return true;
    }
    if (!field->is_string()) {
        return readOptionalInteger(object, key, value, error);
    }

    const QString text = QString::fromStdString(field->get<std::string>());
    if (text.isEmpty()) {
        *value = 0;
        return true;
    }

    bool parsed{};
    const qint64 numericValue = text.toLongLong(&parsed);
    if (parsed) {
        *value = numericValue;
        return true;
    }

    const QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (dateTime.isValid()) {
        *value = dateTime.toSecsSinceEpoch();
        return true;
    }

    setError(error,
             QStringLiteral("Nexus response has an invalid timestamp %1.").arg(QString::fromUtf8(key)));
    return false;
}

bool readOptionalBoolean(const Json& object, const char* key, bool* value, QString* error)
{
    const auto field = object.find(key);
    if (field == object.end() || field->is_null()) {
        *value = false;
        return true;
    }
    if (!field->is_boolean()) {
        setError(error, QStringLiteral("Nexus response has a non-boolean %1.").arg(QString::fromUtf8(key)));
        return false;
    }

    *value = field->get<bool>();
    return true;
}

bool parseSummary(const Json& object, NexusModSummary* summary, QString* error)
{
    if (!object.is_object() || !readRequiredId(object, &summary->id, error)
        || !readOptionalString(object, "name", &summary->name, error)
        || !readOptionalString(object, "summary", &summary->summary, error)
        || !readOptionalString(object, "version", &summary->version, error)
        || !readOptionalString(object, "author", &summary->author, error)
        || !readOptionalInteger(object, "endorsement_count", &summary->endorsementCount, error)) {
        return false;
    }

    if (summary->name.isEmpty()) {
        summary->name = QStringLiteral("Unavailable mod #%1").arg(summary->id);
    }

    QString pictureUrl;
    if (!readOptionalString(object, "picture_url", &pictureUrl, error)) {
        return false;
    }

    summary->pageUrl =
        QUrl(QStringLiteral("https://www.nexusmods.com/stardewvalley/mods/%1").arg(summary->id));
    summary->pictureUrl = QUrl(pictureUrl);
    return true;
}

} // namespace

NexusModClient::NexusModClient(QString apiKey, QObject* parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , apiKey_(std::move(apiKey))
{
    qRegisterMetaType<NexusModSummary>();
    qRegisterMetaType<QVector<NexusModSummary>>();
    qRegisterMetaType<NexusModDetails>();
    qRegisterMetaType<NexusApiError>();
}

QNetworkReply* NexusModClient::fetchModList()
{
    const QUrl url(QString::fromLatin1(kApiBaseUrl) + QStringLiteral("latest_updated.json"));
    QNetworkReply* reply = get(url);
    if (reply != nullptr) {
        connect(reply, &QNetworkReply::finished, this, [this, reply] { handleModListReply(reply); });
    }
    return reply;
}

QNetworkReply* NexusModClient::fetchModDetails(qint64 modId)
{
    if (modId <= 0) {
        emit requestFailed({QStringLiteral("A Nexus mod ID must be positive."), 0});
        return nullptr;
    }

    const QUrl url(QString::fromLatin1(kApiBaseUrl) + QString::number(modId) + QStringLiteral(".json"));
    QNetworkReply* reply = get(url);
    if (reply != nullptr) {
        connect(reply, &QNetworkReply::finished, this, [this, reply] { handleModDetailsReply(reply); });
    }
    return reply;
}

QVector<NexusModSummary> NexusModClient::searchMods(const QVector<NexusModSummary>& mods, QStringView query)
{
    const QString needle = query.trimmed().toString();
    if (needle.isEmpty()) {
        return mods;
    }

    QVector<NexusModSummary> matches;
    for (const NexusModSummary& mod : mods) {
        if (mod.name.contains(needle, Qt::CaseInsensitive)
            || mod.summary.contains(needle, Qt::CaseInsensitive)
            || mod.author.contains(needle, Qt::CaseInsensitive)) {
            matches.append(mod);
        }
    }
    return matches;
}

std::optional<QVector<NexusModSummary>> NexusModClient::parseModListResponse(const QByteArray& response,
                                                                               QString* error)
{
    try {
        const Json document = Json::parse(response.cbegin(), response.cend(), nullptr, false);
        if (document.is_discarded()) {
            setError(error, QStringLiteral("Nexus returned invalid JSON."));
            return std::nullopt;
        }

        const Json* mods = &document;
        if (document.is_object()) {
            const auto field = document.find("mods");
            if (field == document.end()) {
                setError(error, QStringLiteral("Nexus mod-list response is missing mods."));
                return std::nullopt;
            }
            mods = &*field;
        }
        if (!mods->is_array()) {
            setError(error, QStringLiteral("Nexus mod-list response is not an array."));
            return std::nullopt;
        }

        QVector<NexusModSummary> parsed;
        parsed.reserve(static_cast<qsizetype>(mods->size()));
        for (const Json& entry : *mods) {
            NexusModSummary mod;
            if (!parseSummary(entry, &mod, error)) {
                return std::nullopt;
            }
            parsed.append(std::move(mod));
        }
        return parsed;
    } catch (const Json::exception&) {
        setError(error, QStringLiteral("Nexus mod-list response has invalid field values."));
        return std::nullopt;
    }
}

std::optional<NexusModDetails> NexusModClient::parseModDetailsResponse(const QByteArray& response,
                                                                         QString* error)
{
    try {
        const Json document = Json::parse(response.cbegin(), response.cend(), nullptr, false);
        if (document.is_discarded() || !document.is_object()) {
            setError(error, QStringLiteral("Nexus returned invalid mod-details JSON."));
            return std::nullopt;
        }

        NexusModDetails details;
        if (!parseSummary(document, &details, error)
            || !readOptionalString(document, "description", &details.description, error)
            || !readOptionalString(document, "uploaded_by", &details.uploadedBy, error)
            || !readOptionalString(document, "category_name", &details.category, error)) {
            return std::nullopt;
        }

        qint64 createdTimestamp{};
        qint64 updatedTimestamp{};
        if (!readOptionalTimestamp(document, "created_time", &createdTimestamp, error)
            || !readOptionalTimestamp(document, "updated_time", &updatedTimestamp, error)
            || !readOptionalInteger(document, "mod_downloads", &details.downloadCount, error)
            || !readOptionalInteger(document, "mod_unique_downloads", &details.uniqueDownloadCount, error)
            || !readOptionalBoolean(document, "adult_content", &details.adultContent, error)
            || !readOptionalBoolean(document, "available", &details.available, error)) {
            return std::nullopt;
        }

        if (createdTimestamp > 0) {
            details.createdAt = QDateTime::fromSecsSinceEpoch(createdTimestamp, Qt::UTC);
        }
        if (updatedTimestamp > 0) {
            details.updatedAt = QDateTime::fromSecsSinceEpoch(updatedTimestamp, Qt::UTC);
        }
        return details;
    } catch (const Json::exception&) {
        setError(error, QStringLiteral("Nexus mod-details response has invalid field values."));
        return std::nullopt;
    }
}

QNetworkReply* NexusModClient::get(const QUrl& url)
{
    if (apiKey_.trimmed().isEmpty()) {
        emit requestFailed({QStringLiteral("A Nexus API key is required."), 0});
        return nullptr;
    }

    QNetworkRequest request(url);
    request.setRawHeader("apikey", apiKey_.toUtf8());
    request.setRawHeader("User-Agent", "PrismaticLauncher/0.0.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30'000);
    return networkManager_->get(request);
}

void NexusModClient::handleModListReply(QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        emit requestFailed({reply->errorString(), status});
    } else {
        QString error;
        const auto mods = parseModListResponse(reply->readAll(), &error);
        if (mods.has_value()) {
            emit modListFetched(*mods);
        } else {
            emit requestFailed({error, status});
        }
    }
    reply->deleteLater();
}

void NexusModClient::handleModDetailsReply(QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        emit requestFailed({reply->errorString(), status});
    } else {
        QString error;
        const auto mod = parseModDetailsResponse(reply->readAll(), &error);
        if (mod.has_value()) {
            emit modDetailsFetched(*mod);
        } else {
            emit requestFailed({error, status});
        }
    }
    reply->deleteLater();
}
