#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <utility>

// An asynchronous transport fixture: requests never leave the process.
class FixtureReply : public QNetworkReply {
public:
    FixtureReply(const QNetworkRequest& request, QByteArray body, int status, QObject* parent)
        : QNetworkReply(parent), body_(std::move(body)) {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400) setError(QNetworkReply::UnknownServerError, "Fixture HTTP error");
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            setFinished(true);
            emit readyRead();
            emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return body_.size() - position_ + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 maxSize) override {
        const auto size = std::min(maxSize, body_.size() - position_);
        if (size <= 0) return -1;
        std::memcpy(data, body_.constData() + position_, static_cast<std::size_t>(size));
        position_ += size;
        return size;
    }
private:
    QByteArray body_;
    qint64 position_ = 0;
};

class FixtureNetwork : public QNetworkAccessManager {
public:
    struct Response { QByteArray body; int status = 200; };
    QList<Response> responses;
    QList<QNetworkRequest> requests;
    QList<QByteArray> bodies;
    QList<Operation> operations;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoing) override {
        requests.append(request);
        bodies.append(outgoing ? outgoing->readAll() : QByteArray());
        operations.append(operation);
        const auto response = responses.isEmpty() ? Response{"{}", 500} : responses.takeFirst();
        return new FixtureReply(request, response.body, response.status, this);
    }
};
