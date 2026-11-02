#include "ModBrowser.h"
#include "NexusPage.h"
#include "../TestNexusNetwork.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QtTest>

class ModPaginationTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QFile style(":/ui/style.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyle("Fusion");
        qApp->setStyleSheet(style.readAll());
    }
    void pagedBrowsingKeepsResultsOnFailure() {
        qputenv("NEXUS_API_KEY", "fixture-key");
        const auto restoreKey = qScopeGuard([] { qunsetenv("NEXUS_API_KEY"); });
        FixtureNetwork network;
        auto response = [](int offset, int size, int total) {
            QJsonArray nodes;
            for (int index = 0; index < size; ++index) {
                nodes.append(QJsonObject{{"modId", offset + index + 1},
                                         {"name", QString("Mod %1").arg(offset + index + 1)}});
            }
            return QJsonDocument(QJsonObject{{"data", QJsonObject{{"mods", QJsonObject{
                {"nodes", nodes}, {"totalCount", total}}}}}}).toJson();
        };
        network.responses.append({response(0, 50, 105)});
        NexusPage page(nullptr, true, &network);
        page.resize(820, 600);
        page.show();
        auto* browser = page.findChild<ModBrowser*>();
        auto* list = browser->findChild<QListWidget*>("modList");
        auto* previous = browser->findChild<QPushButton*>("modPrevious");
        auto* next = browser->findChild<QPushButton*>("modNext");
        auto* refresh = browser->findChild<QPushButton*>("modRefresh");
        auto* pageCount = browser->findChild<QLabel*>("modPageCount");
        QVERIFY(previous && next && pageCount);
        QVERIFY(!previous->isEnabled());
        QVERIFY(!next->isEnabled());
        page.fetchMods();
        QVERIFY(!next->isEnabled());
        QTRY_COMPARE(list->count(), 50);
        QCOMPARE(list->item(0)->text(), QString("Mod 1"));
        QVERIFY(!previous->isEnabled());
        QVERIFY(next->isEnabled());
        QVERIFY(pageCount->text().contains("105"));
        network.responses.append({response(50, 50, 105)});
        next->click();
        QVERIFY(!next->isEnabled());
        QVERIFY(!previous->isEnabled());
        page.fetchMods(); // A second request during loading must be ignored.
        QCOMPARE(network.requests.size(), 2);
        QTRY_COMPARE(list->item(0)->text(), QString("Mod 51"));
        QVERIFY(previous->isEnabled());
        QVERIFY(next->isEnabled());
        network.responses.append({R"({"mod_id":51,"name":"Mod 51","description":"Details","available":true})"});
        list->setCurrentRow(0);
        auto* download = browser->findChild<QPushButton*>("modDownload");
        auto* description = browser->findChild<QLabel*>("modDescription");
        QTRY_COMPARE(description->text(), QString("Details"));
        QVERIFY(download->isEnabled());
        network.responses.append({"{}", 429});
        next->click();
        QTRY_VERIFY(next->isEnabled());
        QCOMPARE(list->count(), 50);
        QCOMPARE(list->item(0)->text(), QString("Mod 51"));
        QVERIFY(browser->findChild<QLabel*>("modListStatus")->text().contains("429"));
        QCOMPARE(list->currentRow(), 0);
        QCOMPARE(description->text(), QString("Details"));
        QVERIFY(download->isEnabled());
        network.responses.append({response(100, 5, 105)});
        next->click();
        QTRY_COMPARE(list->count(), 5);
        QCOMPARE(list->item(0)->text(), QString("Mod 101"));
        QCOMPARE(list->currentRow(), -1);
        QVERIFY(!download->isEnabled());
        QVERIFY(!next->isEnabled());
        QVERIFY(previous->isEnabled());
        network.responses.append({response(50, 50, 105)});
        previous->click();
        QTRY_COMPARE(list->count(), 50);
        QCOMPARE(list->item(0)->text(), QString("Mod 51"));
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QVERIFY(page.grab().save(screenshots + "/mods-pagination-sample.png"));
        }
        network.responses.append({response(0, 50, 105)});
        refresh->click();
        QTRY_COMPARE(list->item(0)->text(), QString("Mod 1"));
        QVERIFY(!previous->isEnabled());
        const QList<int> expectedOffsets{0, 50, 100, 100, 50, 0};
        QList<int> actualOffsets;
        for (int index = 0; index < network.bodies.size(); ++index) {
            if (network.operations[index] == QNetworkAccessManager::PostOperation) {
                actualOffsets.append(QJsonDocument::fromJson(network.bodies[index]).object()["variables"]
                                         .toObject()["offset"].toInt());
            }
        }
        QCOMPARE(actualOffsets, expectedOffsets);
        auto* search = browser->findChild<QLineEdit*>("modSearch");
        search->setText("no matches on this page");
        QCOMPARE(list->count(), 0);
        QVERIFY(next->isEnabled());
        QVERIFY(browser->findChild<QLabel*>("modListStatus")->text().contains("on this page"));
        search->clear();
        QCOMPARE(list->count(), 50);
        network.responses.append({R"({"mod_id":1,"name":"Mod 1","description":"First page details","available":true})"});
        list->setCurrentRow(0);
        QTRY_COMPARE(description->text(), QString("First page details"));
        network.responses.append({response(0, 50, 105)});
        refresh->click();
        QTRY_VERIFY(refresh->isEnabled());
        QCOMPARE(list->currentRow(), -1);
        QVERIFY(!download->isEnabled());
    }

};

QTEST_MAIN(ModPaginationTests)
#include "ModPaginationTests.moc"
