#include "Nexus_fetchList.hpp"
#include "TestNexusNetwork.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

class NexusModPagesTest : public QObject {
    Q_OBJECT
private slots:
    void requestsPagedResults() {
        FixtureNetwork network;
        network.responses.append({R"({"data":{"mods":{"totalCount":100,"nodes":[{"modId":42,"name":"Test"}]}}})"});
        NexusModClient client("fixture-key", nullptr, &network);
        QSignalSpy fetched(&client, &NexusModClient::modPageFetched);
        client.fetchModPage(50);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(network.requests.first().url(), QUrl("https://api.nexusmods.com/v2/graphql"));
        QCOMPARE(network.operations.first(), QNetworkAccessManager::PostOperation);
        QCOMPARE(network.requests.first().rawHeader("apikey"), QByteArray("fixture-key"));
        const auto body = QJsonDocument::fromJson(network.bodies.first()).object();
        QCOMPARE(body["variables"].toObject()["offset"].toInt(), 50);
        QCOMPARE(body["variables"].toObject()["count"].toInt(), 50);
        QVERIFY(body["query"].toString().contains("stardewvalley"));
        QVERIFY(body["query"].toString().contains("updatedAt"));
        QTRY_COMPARE(fetched.size(), 1);
        QCOMPARE(qvariant_cast<NexusModPage>(fetched.first().first()).offset, 50);
    }
    void pageNetworkErrorsAndInvalidOffsets() {
        FixtureNetwork network;
        NexusModClient client("fixture-key", nullptr, &network);
        QSignalSpy failed(&client, &NexusModClient::requestFailed);
        QSignalSpy fetched(&client, &NexusModClient::modPageFetched);
        QVERIFY(!client.fetchModPage(-1));
        QCOMPARE(failed.size(), 1);
        QVERIFY(network.requests.isEmpty());
        NexusModClient missingKey("", nullptr, &network);
        QSignalSpy missingKeyError(&missingKey, &NexusModClient::requestFailed);
        QVERIFY(!missingKey.fetchModPage());
        QCOMPARE(missingKeyError.size(), 1);
        QVERIFY(network.requests.isEmpty());
        network.responses.append({"{}", 429});
        client.fetchModPage();
        QTRY_COMPARE(failed.size(), 2);
        QCOMPARE(qvariant_cast<NexusApiError>(failed.last().first()).httpStatus, 429);
        network.responses.append({R"({"errors":[{"message":"sensitive remote error"}]})"});
        client.fetchModPage();
        QTRY_COMPARE(failed.size(), 3);
        QVERIFY(!qvariant_cast<NexusApiError>(failed.last().first()).message.contains("sensitive"));
        QVERIFY(fetched.isEmpty());
    }
    void parsesPagedResponse() {
        QString error;
        const auto page = NexusModClient::parseModPageResponse(
            R"({"data":{"mods":{"totalCount":120,"nodes":[{"modId":456,"name":"Page two","author":null,"endorsements":4}]}}})",
            50, &error);
        QVERIFY2(page.has_value(), qPrintable(error));
        QCOMPARE(page->offset, 50);
        QCOMPARE(page->totalCount, 120);
        QCOMPARE(page->mods.size(), 1);
        QCOMPARE(page->mods.first().id, 456);
        QCOMPARE(page->mods.first().endorsementCount, 4);
        QCOMPARE(page->mods.first().pageUrl.path(), QString("/stardewvalley/mods/456"));
    }
    void rejectsInvalidPages_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("invalid json") << QByteArray("{");
        QTest::newRow("graphql failure") << QByteArray(R"({"errors":[{"message":"Denied"}],"data":{"mods":{"nodes":[],"totalCount":0}}})");
        QTest::newRow("missing count") << QByteArray(R"({"data":{"mods":{"nodes":[]}}})");
        QTest::newRow("wrong count type") << QByteArray(R"({"data":{"mods":{"nodes":[],"totalCount":"50"}}})");
        QTest::newRow("negative count") << QByteArray(R"({"data":{"mods":{"nodes":[],"totalCount":-1}}})");
        QTest::newRow("missing nodes") << QByteArray(R"({"data":{"mods":{"totalCount":50}}})");
        QTest::newRow("invalid id") << QByteArray(R"({"data":{"mods":{"totalCount":1,"nodes":[{"modId":0}]}}})");
        QTest::newRow("invalid name") << QByteArray(R"({"data":{"mods":{"totalCount":1,"nodes":[{"modId":1,"name":true}]}}})");
    }
    void rejectsInvalidPages() {
        QFETCH(QByteArray, response);
        QString error;
        QVERIFY(!NexusModClient::parseModPageResponse(response, 0, &error));
        QVERIFY(!error.isEmpty());
    }
    void parsesEmptyPage() {
        const auto page = NexusModClient::parseModPageResponse(
            R"({"data":{"mods":{"totalCount":0,"nodes":[]}}})");
        QVERIFY(page);
        QVERIFY(page->mods.isEmpty());
        QCOMPARE(page->totalCount, 0);
    }
};

QTEST_GUILESS_MAIN(NexusModPagesTest)
#include "nexus_mod_pages_test.moc"
