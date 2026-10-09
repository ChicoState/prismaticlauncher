#include <QtTest>

#include <QFile>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>

#include <chrono>

#include "nexus/NexusModDownloader.h"

class FakeNexusHttpTransport final : public NexusHttpTransport {
public:
    NexusHttpResponse linkResponse;
    NexusHttpResponse fileResponse;
    int requestCount = 0;
    int downloadCount = 0;

    NexusHttpResponse get(const QNetworkRequest& request) override {
        ++requestCount;
        lastLinkRequest = request;
        return linkResponse;
    }

    NexusHttpResponse download(const QNetworkRequest& request, const QString& destination,
                               qint64 maximumBytes) override {
        ++downloadCount;
        lastDownloadRequest = request;
        lastDestination = destination;
        lastMaximumBytes = maximumBytes;
        return fileResponse;
    }

    QNetworkRequest lastLinkRequest;
    QNetworkRequest lastDownloadRequest;
    QString lastDestination;
    qint64 lastMaximumBytes = 0;
};

class NexusModDownloaderTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMissingApiKey();
    void rejectsInvalidGameDomain();
    void rejectsNonHttpsDownloadUrl();
    void reportsLinkRequestFailure();
    void resolvesAndDownloadsHttpsArchive();
    void streamsArchiveToAnAtomicOutputFile();
    void discardsOutputWhenArchiveExceedsLimit();
};

namespace {
NexusDownloadRequest validRequest() {
    return {
        .gameDomain = "stardewvalley",
        .modId = 2400,
        .fileId = 12345,
        .apiKey = "test-api-key",
        .destinationFile = "/tmp/example-mod.zip",
        .maximumBytes = 1024,
    };
}

void serveArchive(QTcpServer& server, const QByteArray& archive) {
    QObject::connect(&server, &QTcpServer::newConnection, [&server, archive]() {
        QTcpSocket* client = server.nextPendingConnection();
        QObject::connect(client, &QTcpSocket::readyRead, [client, archive]() {
            client->readAll();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: " +
                                        QByteArray::number(archive.size()) +
                                        "\r\nConnection: close\r\n\r\n" + archive;
            client->write(response);
            client->disconnectFromHost();
        });
    });
}
}  // namespace

void NexusModDownloaderTest::rejectsMissingApiKey() {
    FakeNexusHttpTransport transport;
    NexusModDownloader downloader(transport);
    NexusDownloadRequest request = validRequest();
    request.apiKey.clear();

    const NexusDownloadResult result = downloader.download(request);

    QCOMPARE(result.status, NexusDownloadStatus::MissingApiKey);
    QCOMPARE(transport.requestCount, 0);
}

void NexusModDownloaderTest::rejectsInvalidGameDomain() {
    FakeNexusHttpTransport transport;
    NexusModDownloader downloader(transport);
    NexusDownloadRequest request = validRequest();
    request.gameDomain = "stardewvalley/../../other";

    const NexusDownloadResult result = downloader.download(request);

    QCOMPARE(result.status, NexusDownloadStatus::InvalidRequest);
    QCOMPARE(transport.requestCount, 0);
}

void NexusModDownloaderTest::rejectsNonHttpsDownloadUrl() {
    FakeNexusHttpTransport transport;
    transport.linkResponse = {
        .statusCode = 200,
        .body = R"([{"URI":"http://files.example.invalid/mod.zip"}])",
    };
    NexusModDownloader downloader(transport);

    const NexusDownloadResult result = downloader.download(validRequest());

    QCOMPARE(result.status, NexusDownloadStatus::UnsafeDownloadUrl);
    QCOMPARE(transport.downloadCount, 0);
}

void NexusModDownloaderTest::reportsLinkRequestFailure() {
    FakeNexusHttpTransport transport;
    transport.linkResponse = {
        .statusCode = 429,
        .errorMessage = "rate limited",
    };
    NexusModDownloader downloader(transport);

    const NexusDownloadResult result = downloader.download(validRequest());

    QCOMPARE(result.status, NexusDownloadStatus::LinkRequestFailed);
    QCOMPARE(transport.downloadCount, 0);
}

void NexusModDownloaderTest::resolvesAndDownloadsHttpsArchive() {
    FakeNexusHttpTransport transport;
    transport.linkResponse = {
        .statusCode = 200,
        .body = R"([{"URI":"https://files.nexus-cdn.com/mod.zip"}])",
    };
    transport.fileResponse = {
        .statusCode = 200,
        .bytesWritten = 512,
    };
    NexusModDownloader downloader(transport);
    NexusDownloadRequest request = validRequest();
    request.downloadKey = "key from nxm link";
    request.downloadKeyExpiresUnixSeconds = 1'800'000'000;

    const NexusDownloadResult result = downloader.download(request);

    QCOMPARE(result.status, NexusDownloadStatus::Success);
    QCOMPARE(result.bytesWritten, 512);
    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.downloadCount, 1);
    QCOMPARE(transport.lastDestination, request.destinationFile);
    QCOMPARE(transport.lastMaximumBytes, request.maximumBytes);
    QCOMPARE(transport.lastDownloadRequest.url().scheme(), "https");
    QCOMPARE(transport.lastLinkRequest.rawHeader("Application-Version"), QByteArray("0.0.0"));
    QCOMPARE(transport.lastLinkRequest.url().path(),
             "/v1/games/stardewvalley/mods/2400/files/12345/download_link.json");
    const QUrlQuery query(transport.lastLinkRequest.url());
    QCOMPARE(query.queryItemValue("key"), request.downloadKey);
    QCOMPARE(query.queryItemValue("expires"), QString::number(*request.downloadKeyExpiresUnixSeconds));
}

void NexusModDownloaderTest::streamsArchiveToAnAtomicOutputFile() {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) {
        QSKIP("The current sandbox does not permit loopback listener sockets.");
    }
    serveArchive(server, "mod-data");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QtNexusHttpTransport transport(std::chrono::seconds(1));
    const QString destination = temporaryDirectory.filePath("mod.zip");
    const QNetworkRequest request(
        QUrl(QString("http://127.0.0.1:%1/mod.zip").arg(server.serverPort())));
    const NexusHttpResponse response = transport.download(request, destination, 1024);

    QVERIFY(response.succeeded());
    QCOMPARE(response.bytesWritten, 8);
    QFile archive(destination);
    QVERIFY(archive.open(QIODevice::ReadOnly));
    QCOMPARE(archive.readAll(), QByteArray("mod-data"));
}

void NexusModDownloaderTest::discardsOutputWhenArchiveExceedsLimit() {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost)) {
        QSKIP("The current sandbox does not permit loopback listener sockets.");
    }
    serveArchive(server, "oversized");
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QtNexusHttpTransport transport(std::chrono::seconds(1));
    const QString destination = temporaryDirectory.filePath("mod.zip");
    const QNetworkRequest request(
        QUrl(QString("http://127.0.0.1:%1/mod.zip").arg(server.serverPort())));
    const NexusHttpResponse response = transport.download(request, destination, 4);

    QVERIFY(!response.succeeded());
    QVERIFY(!QFile::exists(destination));
}

QTEST_MAIN(NexusModDownloaderTest)

#include "NexusModDownloaderTest.moc"
