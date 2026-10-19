#pragma once

#include <optional>

#include <QString>

#include "nexus/NexusHttpTransport.h"

enum class NexusDownloadStatus {
    Success,
    InvalidRequest,
    MissingApiKey,
    LinkRequestFailed,
    InvalidLinkResponse,
    UnsafeDownloadUrl,
    DownloadFailed,
};

struct NexusDownloadRequest {
    QString gameDomain;
    qint64 modId = 0;
    qint64 fileId = 0;
    QString apiKey;
    QString destinationFile;
    std::optional<QString> downloadKey;
    std::optional<qint64> downloadKeyExpiresUnixSeconds;
    qint64 maximumBytes = 2LL * 1024 * 1024 * 1024;
};

struct NexusDownloadResult {
    NexusDownloadStatus status = NexusDownloadStatus::InvalidRequest;
    QString message;
    qint64 bytesWritten = 0;

    [[nodiscard]] bool succeeded() const;
};

class NexusModDownloader {
public:
    explicit NexusModDownloader(NexusHttpTransport& transport);

    [[nodiscard]] NexusDownloadResult download(const NexusDownloadRequest& request);

private:
    NexusHttpTransport& transport_;
};
