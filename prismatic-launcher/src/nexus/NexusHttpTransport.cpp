#include "nexus/NexusHttpTransport.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>

namespace {
int statusCode(const QNetworkReply& reply) {
    return reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

void waitForReply(QNetworkReply& reply, const std::chrono::milliseconds timeout, bool* timedOut) {
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(&reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &eventLoop, [&reply, timedOut]() {
        *timedOut = true;
        reply.abort();
    });

    timer.start(timeout);
    if (!reply.isFinished()) {
        eventLoop.exec();
    }
}

NexusHttpResponse failedResponse(const QString& message) {
    return {
        .errorMessage = message,
    };
}
}  // namespace

bool NexusHttpResponse::succeeded() const {
    return statusCode >= 200 && statusCode < 300 && errorMessage.isEmpty();
}

QtNexusHttpTransport::QtNexusHttpTransport(const std::chrono::milliseconds timeout)
    : timeout_(timeout) {}

NexusHttpResponse QtNexusHttpTransport::get(const QNetworkRequest& request) {
    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(request);
    bool timedOut = false;
    waitForReply(*reply, timeout_, &timedOut);

    NexusHttpResponse response{
        .statusCode = statusCode(*reply),
        .body = reply->readAll(),
    };
    if (timedOut) {
        response.errorMessage = "The Nexus API request timed out.";
    } else if (reply->error() != QNetworkReply::NoError) {
        response.errorMessage = "The Nexus API request failed.";
    }
    reply->deleteLater();
    return response;
}

NexusHttpResponse QtNexusHttpTransport::download(const QNetworkRequest& request,
                                                  const QString& destinationFile,
                                                  const qint64 maximumBytes) {
    QSaveFile output(destinationFile);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        return failedResponse("The output file could not be opened safely.");
    }

    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(request);
    qint64 bytesWritten = 0;
    bool exceededLimit = false;
    bool writeFailed = false;
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty()) {
            return;
        }
        if (bytesWritten > maximumBytes - chunk.size()) {
            exceededLimit = true;
            reply->abort();
            return;
        }
        if (output.write(chunk) != chunk.size()) {
            writeFailed = true;
            reply->abort();
            return;
        }
        bytesWritten += chunk.size();
    });

    bool timedOut = false;
    waitForReply(*reply, timeout_, &timedOut);
    if (!exceededLimit && !writeFailed) {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty()) {
            if (bytesWritten > maximumBytes - remaining.size()) {
                exceededLimit = true;
            } else if (output.write(remaining) != remaining.size()) {
                writeFailed = true;
            } else {
                bytesWritten += remaining.size();
            }
        }
    }

    NexusHttpResponse response{
        .statusCode = statusCode(*reply),
        .bytesWritten = bytesWritten,
    };
    if (timedOut) {
        response.errorMessage = "The archive transfer timed out.";
    } else if (exceededLimit) {
        response.errorMessage = "The archive exceeds the configured size limit.";
    } else if (writeFailed) {
        response.errorMessage = "Writing the archive failed.";
    } else if (reply->error() != QNetworkReply::NoError) {
        response.errorMessage = "The archive transfer failed.";
    } else if (response.statusCode < 200 || response.statusCode >= 300) {
        response.errorMessage = "The archive server returned an unsuccessful response.";
    } else if (!output.commit()) {
        response.errorMessage = "The archive could not be saved safely.";
    }

    if (!response.succeeded()) {
        output.cancelWriting();
    }
    reply->deleteLater();
    return response;
}
