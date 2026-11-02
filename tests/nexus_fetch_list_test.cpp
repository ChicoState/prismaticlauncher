#include "Nexus_fetchList.hpp"

#include <QTest>

class NexusFetchListTest final : public QObject {
    Q_OBJECT

private slots:
    void filtersModListByNameSummaryAndAuthor();
    void parsesAValidLatestUpdatedResponse();
    void parsesAnUnavailableModWithoutAName();
    void parsesAValidModDetailsResponse();
    void parsesModDetailsWithNumericStringTimestamps();
    void parsesModDetailsWithIsoTimestamps();
    void rejectsMalformedModDetailsResponse();
};

void NexusFetchListTest::filtersModListByNameSummaryAndAuthor()
{
    const QVector<NexusModSummary> mods{
        {.id = 1,
         .name = "Stardew Valley Expanded",
         .summary = "An expansion",
         .author = "FlashShifter"},
        {.id = 2, .name = "Lookup Anything", .summary = "Inspect the world", .author = "Pathoschild"},
    };

    QVERIFY(NexusModClient::searchMods(mods, u"FLASH") == QVector<NexusModSummary>{mods.front()});
    QVERIFY(NexusModClient::searchMods(mods, u"world") == QVector<NexusModSummary>{mods.back()});
    QCOMPARE(NexusModClient::searchMods(mods, u"missing").size(), 0);
}

void NexusFetchListTest::parsesAValidLatestUpdatedResponse()
{
    const QByteArray response = R"([{
        "mod_id": 456,
        "name": "List Entry",
        "summary": "From the latest-updated endpoint",
        "version": "1.0.0",
        "author": "ExampleAuthor",
        "endorsement_count": 3
    }])";

    QString error;
    const auto mods = NexusModClient::parseModListResponse(response, &error);

    QVERIFY2(mods.has_value(), qPrintable(error));
    QCOMPARE(mods->size(), 1);
    QCOMPARE(mods->front().id, 456);
    QCOMPARE(mods->front().pageUrl.path(), QStringLiteral("/stardewvalley/mods/456"));
}

void NexusFetchListTest::parsesAnUnavailableModWithoutAName()
{
    const QByteArray response = R"([{
        "mod_id": 789,
        "name": null,
        "summary": null,
        "version": null,
        "author": null,
        "endorsement_count": 0
    }])";

    QString error;
    const auto mods = NexusModClient::parseModListResponse(response, &error);

    QVERIFY2(mods.has_value(), qPrintable(error));
    QCOMPARE(mods->size(), 1);
    QCOMPARE(mods->front().name, QStringLiteral("Unavailable mod #789"));
}

void NexusFetchListTest::parsesAValidModDetailsResponse()
{
    const QByteArray response = R"({
        "mod_id": 123,
        "name": "Example Mod",
        "summary": "A test mod",
        "description": "<p>Long description</p>",
        "version": "2.4.0",
        "author": "ExampleAuthor",
        "uploaded_by": "Uploader",
        "created_time": 1700000000,
        "updated_time": 1700000100,
        "mod_downloads": 42,
        "mod_unique_downloads": 12,
        "endorsement_count": 7,
        "adult_content": false,
        "available": true,
        "category_name": "Gameplay"
    })";

    QString error;
    const auto mod = NexusModClient::parseModDetailsResponse(response, &error);

    QVERIFY2(mod.has_value(), qPrintable(error));
    QCOMPARE(mod->id, 123);
    QCOMPARE(mod->description, "<p>Long description</p>");
    QCOMPARE(mod->version, "2.4.0");
    QCOMPARE(mod->updatedAt, QDateTime::fromSecsSinceEpoch(1700000100, Qt::UTC));
    QCOMPARE(mod->downloadCount, 42);
    QVERIFY(mod->available);
}

void NexusFetchListTest::parsesModDetailsWithNumericStringTimestamps()
{
    const QByteArray response = R"({
        "mod_id": 124,
        "name": "Timestamp Mod",
        "created_time": "1700000000",
        "updated_time": "1700000100"
    })";

    QString error;
    const auto mod = NexusModClient::parseModDetailsResponse(response, &error);

    QVERIFY2(mod.has_value(), qPrintable(error));
    QCOMPARE(mod->createdAt, QDateTime::fromSecsSinceEpoch(1700000000, Qt::UTC));
    QCOMPARE(mod->updatedAt, QDateTime::fromSecsSinceEpoch(1700000100, Qt::UTC));
}

void NexusFetchListTest::parsesModDetailsWithIsoTimestamps()
{
    const QByteArray response = R"({
        "mod_id": 125,
        "name": "ISO Timestamp Mod",
        "created_time": "2023-11-14T22:13:20Z",
        "updated_time": ""
    })";

    QString error;
    const auto mod = NexusModClient::parseModDetailsResponse(response, &error);

    QVERIFY2(mod.has_value(), qPrintable(error));
    QCOMPARE(mod->createdAt, QDateTime::fromSecsSinceEpoch(1700000000, Qt::UTC));
    QVERIFY(!mod->updatedAt.isValid());
}

void NexusFetchListTest::rejectsMalformedModDetailsResponse()
{
    QString error;
    const auto mod = NexusModClient::parseModDetailsResponse(R"({"name":"missing id"})", &error);

    QVERIFY(!mod.has_value());
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(NexusFetchListTest)

#include "nexus_fetch_list_test.moc"
