#include "nexus/NexusModDownloader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr auto NexusApiBaseUrl = "https://api.nexusmods.com";
const QRegularExpression GameDomainPattern("^[a-z0-9][a-z0-9_-]{0,99}$");

NexusDownloadResult failure(const NexusDownloadStatus status, const QString& message) {
    return {
        .status = status,
        .message = message,
    };
}

bool hasValidRequestFields(const NexusDownloadRequest& request) {
    return GameDomainPattern.match(request.gameDomain).hasMatch() && request.modId > 0 &&
           request.fileId > 0 && !request.destinationFile.isEmpty() && request.maximumBytes > 0 &&
           (!request.downloadKey.has_value() == !request.downloadKeyExpiresUnixSeconds.has_value()) &&
           (!request.downloadKey.has_value() ||
            (!request.downloadKey->isEmpty() && *request.downloadKeyExpiresUnixSeconds > 0));
}

QNetworkRequest makeRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Application-Version", "0.0.0");
    request.setRawHeader("User-Agent", "PrismaticLauncher/0.0.0");
    return request;
}

std::optional<QUrl> downloadUrlFromResponse(const QByteArray& body) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return std::nullopt;
    }

    for (const QJsonValue& candidate : document.array()) {
        if (!candidate.isObject()) {
            continue;
        }
        const QJsonValue uri = candidate.toObject().value("URI");
        if (!uri.isString()) {
            continue;
        }
        const QUrl url(uri.toString());
        if (url.isValid() && url.scheme() == "https" && !url.host().isEmpty() &&
            url.userInfo().isEmpty()) {
            return url;
        }
    }
    return std::nullopt;
}

bool containsUriField(const QByteArray& body) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray()) {
        return false;
    }
    for (const QJsonValue& candidate : document.array()) {
        if (candidate.isObject() && candidate.toObject().value("URI").isString()) {
            return true;
        }
    }
    return false;
}
}  // namespace

bool NexusDownloadResult::succeeded() const {
    return status == NexusDownloadStatus::Success;
}

NexusModDownloader::NexusModDownloader(NexusHttpTransport& transport) : transport_(transport) {}

NexusDownloadResult NexusModDownloader::download(const NexusDownloadRequest& request) {
    if (!hasValidRequestFields(request)) {
        return failure(NexusDownloadStatus::InvalidRequest, "The download request is invalid.");
    }
    if (request.apiKey.isEmpty()) {
        return failure(NexusDownloadStatus::MissingApiKey,
                       "NEXUSMODS_API_KEY must contain a user-owned Nexus API key.");
    }

    QUrl endpoint(QString("%1/v1/games/%2/mods/%3/files/%4/download_link.json")
                      .arg(NexusApiBaseUrl, request.gameDomain)
                      .arg(request.modId)
                      .arg(request.fileId));
    QUrlQuery query;
    if (request.downloadKey.has_value()) {
        query.addQueryItem("key", *request.downloadKey);
        query.addQueryItem("expires", QString::number(*request.downloadKeyExpiresUnixSeconds));
    }
    endpoint.setQuery(query);

    QNetworkRequest linkRequest = makeRequest(endpoint);
    linkRequest.setRawHeader("apikey", request.apiKey.toUtf8());
    const NexusHttpResponse linkResponse = transport_.get(linkRequest);
    if (!linkResponse.succeeded()) {
        return failure(NexusDownloadStatus::LinkRequestFailed,
                       "Nexus Mods could not provide a download link.");
    }

    const std::optional<QUrl> archiveUrl = downloadUrlFromResponse(linkResponse.body);
    if (!archiveUrl.has_value()) {
        return failure(containsUriField(linkResponse.body) ? NexusDownloadStatus::UnsafeDownloadUrl
                                                            : NexusDownloadStatus::InvalidLinkResponse,
                       "Nexus Mods returned an invalid download link.");
    }

    QNetworkRequest archiveRequest = makeRequest(*archiveUrl);
    archiveRequest.setRawHeader("Accept", "application/octet-stream");
    const NexusHttpResponse archiveResponse =
        transport_.download(archiveRequest, request.destinationFile, request.maximumBytes);
    if (!archiveResponse.succeeded()) {
        return failure(NexusDownloadStatus::DownloadFailed,
                       "The mod archive could not be downloaded safely.");
    }
    return {
        .status = NexusDownloadStatus::Success,
        .message = "The mod archive was downloaded.",
        .bytesWritten = archiveResponse.bytesWritten,
    };
}
