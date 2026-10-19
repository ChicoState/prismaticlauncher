#pragma once

#include <chrono>

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>

struct NexusHttpResponse {
    int statusCode = -1;
    QByteArray body;
    qint64 bytesWritten = 0;
    QString errorMessage;

    [[nodiscard]] bool succeeded() const;
};

class NexusHttpTransport {
public:
    virtual ~NexusHttpTransport() = default;

    virtual NexusHttpResponse get(const QNetworkRequest& request) = 0;
    virtual NexusHttpResponse download(const QNetworkRequest& request, const QString& destinationFile,
                                       qint64 maximumBytes) = 0;
};

class QtNexusHttpTransport final : public NexusHttpTransport {
public:
    explicit QtNexusHttpTransport(
        std::chrono::milliseconds timeout = std::chrono::seconds(60));

    NexusHttpResponse get(const QNetworkRequest& request) override;
    NexusHttpResponse download(const QNetworkRequest& request, const QString& destinationFile,
                               qint64 maximumBytes) override;

private:
    std::chrono::milliseconds timeout_;
};
