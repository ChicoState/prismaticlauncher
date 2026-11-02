#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSlider>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "InstanceWindow.h"
#include "LauncherConfig.h"
#include "LauncherWindow.h"
#include "SmapiWindow.h"
#include "ModBrowser.h"

namespace {
QVector<NexusModSummary> sampleMods() {
    return {{2400,
             "Seasonal farm buildings",
             "A fresh look for every season.",
             "2.4.0",
             "Valley Workshop",
             {},
             {},
             1420},
            {2401,
             "Better artisan goods",
             "Thoughtful details for your farm's produce.",
             "1.8.2",
             "Juniper",
             {},
             {},
             892},
            {2402,
             "Quiet mornings",
             "A slower start to life in the valley.",
             "1.2.0",
             "Fern",
             {},
             {},
             312}};
}
}  // namespace

class UrlReceiver : public QObject {
    Q_OBJECT
   public:
    QUrl url;
   public slots:
    void open(const QUrl& value) { url = value; }
};

class LauncherTests : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase() {
        qunsetenv("NEXUS_API_KEY");  // Tests never make live Nexus requests.
        QFile style(":/ui/style.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyle("Fusion");
        qApp->setStyleSheet(style.readAll());
    }

    void browseNexusMods() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        auto* browse = window.findChild<QPushButton*>("browseNexusMods");
        QVERIFY(browse);
        browse->click();
        auto* pages = window.findChild<QStackedWidget*>("pages");
        auto* page = pages->currentWidget();
        QCOMPARE(page->objectName(), QString("nexusPage"));
        auto* browser = page->findChild<ModBrowser*>();
        QVERIFY(browser);
        auto* status = browser->findChild<QLabel*>("modListStatus");
        auto* refresh = browser->findChild<QPushButton*>("modRefresh");
        auto* list = browser->findChild<QListWidget*>("modList");
        auto* download = browser->findChild<QPushButton*>("modDownload");
        auto* client = page->findChild<NexusModClient*>("nexusListClient");
        QVERIFY(status && refresh && list && download && client);
        QVERIFY(status->text().contains("NEXUS_API_KEY"));
        QVERIFY(refresh->isEnabled());
        QVERIFY(!download->isEnabled());
        const auto mods = NexusModClient::parseModListResponse(
            R"([{"mod_id":42,"name":"<b>Valley Expanded</b>","author":"A farmer","version":"1.2","summary":"More places to explore."}])");
        QVERIFY(mods.has_value());
        emit client->modPageFetched({*mods, mods->size(), 0});
        QCOMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QString("<b>Valley Expanded</b>"));
        list->setCurrentRow(0);
        QCOMPARE(browser->findChild<QLabel*>("modTitle")->text(), list->item(0)->text());
        QVERIFY(browser->findChild<QLabel*>("modAuthor")->text().contains("A farmer"));
        QVERIFY(browser->findChild<QLabel*>("modMetadata")->text().contains("1.2"));
        QCOMPARE(browser->findChild<QLabel*>("modSummary")->text(),
                 QString("More places to explore."));
        QVERIFY(download->isEnabled());
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QVERIFY(window.grab().save(screenshots + "/nexus-selected-sample.png"));
        }
        window.resize(820, 600);
        QTest::qWait(40);
        QCOMPARE(window.size(), QSize(820, 600));
        QVERIFY(download->isVisible());
        QVERIFY(window.rect().contains(
            QRect(download->mapTo(&window, QPoint(0, 0)), download->size())));
        if (!screenshots.isEmpty()) {
            QVERIFY(window.grab().save(screenshots + "/nexus-minimum-sample.png"));
        }
        emit client->modPageFetched({{}, 0, 0});
        QCOMPARE(list->count(), 0);
        QVERIFY(status->text().contains("No mods"));
        QVERIFY(!download->isEnabled());
        emit client->requestFailed({"Unavailable", 503});
        QVERIFY(status->text().contains("503"));
        QVERIFY(refresh->isEnabled());
        refresh->click();
        QVERIFY(status->text().contains("NEXUS_API_KEY"));
        page->findChild<QPushButton*>("backFromNexus")->click();
        QCOMPARE(pages->currentIndex(), 0);
        QCOMPARE(window.focusWidget(), browse);
        browse->click();
        QCOMPARE(pages->currentWidget(), page);
        QVERIFY(status->text().contains("NEXUS_API_KEY"));
    }

    void modSelectionAndDownload() {
        ModBrowser browser;
        browser.resize(900, 560);
        browser.show();
        browser.setMods(sampleMods());
        auto* list = browser.findChild<QListWidget*>("modList");
        auto* title = browser.findChild<QLabel*>("modTitle");
        auto* download = browser.findChild<QPushButton*>("modDownload");
        QVERIFY(list && title && download);
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->item(0)->text(), QString("Seasonal farm buildings"));
        QVERIFY(!download->isEnabled());
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(1)).center());
        QCOMPARE(title->text(), QString("Better artisan goods"));
        QVERIFY(download->isEnabled());
        UrlReceiver receiver;
        QDesktopServices::setUrlHandler("https", &receiver, "open");
        download->click();
        QDesktopServices::unsetUrlHandler("https");
        QCOMPARE(receiver.url, QUrl("https://www.nexusmods.com/stardewvalley/mods/2401?tab=files"));
        list->setFocus();
        QTest::keyClick(list, Qt::Key_Down);
        QCOMPARE(title->text(), QString("Quiet mornings"));
    }

    void modSearchClearsHiddenSelection() {
        ModBrowser browser;
        browser.setMods(sampleMods());
        auto* list = browser.findChild<QListWidget*>("modList");
        auto* search = browser.findChild<QLineEdit*>("modSearch");
        auto* download = browser.findChild<QPushButton*>("modDownload");
        list->setCurrentRow(0);
        search->setText("Juniper");
        QCOMPARE(list->count(), 1);
        QVERIFY(!download->isEnabled());
        list->setCurrentRow(0);
        search->setText("no matches");
        QCOMPARE(list->count(), 0);
        QVERIFY(!download->isEnabled());
        QVERIFY(browser.findChild<QLabel*>("modListStatus")->text().contains("No matching"));
        search->clear();
        QCOMPARE(list->count(), 3);
        QVERIFY(!download->isEnabled());
    }

    void modLoadingErrorAndEmptyStates() {
        ModBrowser browser;
        browser.setMods(sampleMods());
        browser.findChild<QListWidget*>("modList")->setCurrentRow(0);
        browser.beginLoading();
        QVERIFY(!browser.findChild<QPushButton*>("modDownload")->isEnabled());
        QVERIFY(!browser.findChild<QPushButton*>("modRefresh")->isEnabled());
        browser.setListError("Connection unavailable. Try again.");
        QVERIFY(browser.findChild<QLabel*>("modListStatus")->text().contains("Connection"));
        QVERIFY(!browser.findChild<QLineEdit*>("modSearch")->isEnabled());
        QVERIFY(browser.findChild<QPushButton*>("modRefresh")->isEnabled());
        browser.setMods({});
        QVERIFY(browser.findChild<QLabel*>("modListStatus")->text().contains("No mods"));
        QVERIFY(!browser.findChild<QPushButton*>("modDownload")->isEnabled());
    }

    void modDescriptionReadableText_data() {
        QTest::addColumn<QString>("description");
        QTest::addColumn<QString>("expected");
        QTest::newRow("html")
            << "<p><b>Seasonal</b> &amp; cozy</p><p>Install:<br>Copy to Mods.</p>"
            << "Seasonal & cozy\nInstall:\nCopy to Mods.";
        QTest::newRow("plain") << "First line\n\nSecond line: 1 < 2 & 3 > 2."
                               << "First line\n\nSecond line: 1 < 2 & 3 > 2.";
        QTest::newRow("escaped-tags") << "<p>Use &lt;b&gt; for bold.</p>"
                                      << "Use <b> for bold.";
        QTest::newRow("empty") << "" << "No additional description provided.";
        QTest::newRow("bbcode")
            << "[center][size=5][b]Seasonal farm[/b][/size][/center]\n"
               "[color=#ffffff]Install[/color]\n[list][*]Copy to Mods.[*]Launch.[/list]\n"
               "[url=https://example.com]Requirements[/url]"
            << "Seasonal farm\nInstall\n\n• Copy to Mods.\n• Launch.\n\nRequirements";
        QTest::newRow("literal-brackets") << "Requires [CP] and [some mod]."
                                          << "Requires [CP] and [some mod].";
        QTest::newRow("mixed-nexus-markup")
            << "[size=4][b]Description[/b][/size]\n"
               "<br />I didn't see a recolor for the Update Tracker Mod to match the Overgrown "
               "Flowery Interface mod so this is what this does! I have Recolored the Update "
               "Tracker buttons to match Overgrown Flowery Interface!\n<br />"
            << "Description \nI didn't see a recolor for the Update Tracker Mod to match the "
               "Overgrown Flowery Interface mod so this is what this does! I have Recolored "
               "the Update Tracker buttons to match Overgrown Flowery Interface! \n";
    }

    void modDescriptionReadableText() {
        QFETCH(QString, description);
        QFETCH(QString, expected);
        ModBrowser browser;
        browser.setMods(sampleMods());
        browser.findChild<QListWidget*>("modList")->setCurrentRow(0);
        NexusModDetails details;
        static_cast<NexusModSummary&>(details) = sampleMods().first();
        details.description = description;
        browser.setModDetails(details);
        auto* label = browser.findChild<QLabel*>("modDescription");
        QCOMPARE(label->text(), expected);
        QCOMPARE(label->textFormat(), Qt::PlainText);
    }

    void staleDetailsAndUnavailableMods() {
        ModBrowser browser;
        browser.setMods(sampleMods());
        auto* list = browser.findChild<QListWidget*>("modList");
        list->setCurrentRow(0);
        list->setCurrentRow(1);
        NexusModDetails stale;
        static_cast<NexusModSummary&>(stale) = sampleMods().at(0);
        stale.description = "Old response";
        stale.available = true;
        browser.setModDetails(stale);
        QCOMPARE(browser.findChild<QLabel*>("modTitle")->text(), QString("Better artisan goods"));
        NexusModDetails current;
        static_cast<NexusModSummary&>(current) = sampleMods().at(1);
        current.description = "<b>Plain text, not HTML</b>";
        current.available = false;
        browser.setModDetails(current);
        QVERIFY(!browser.findChild<QPushButton*>("modDownload")->isEnabled());
        QCOMPARE(browser.findChild<QLabel*>("modDescription")->textFormat(), Qt::PlainText);
        browser.findChild<QLineEdit*>("modSearch")->setText("artisan");
        QCOMPARE(list->count(), 1);
        QCOMPARE(browser.findChild<QLabel*>("modTitle")->text(), QString("Better artisan goods"));
        QVERIFY(!browser.findChild<QPushButton*>("modDownload")->isEnabled());
    }

    void populatedModPreviewAndNavigation() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        auto* browser = instanceWindow->findChild<ModBrowser*>();
        QVERIFY(browser);
        browser->setMods(sampleMods());
        browser->setPagination(0, 3, 3, false);
        browser->findChild<QListWidget*>("modList")->setCurrentRow(0);
        NexusModDetails details;
        static_cast<NexusModSummary&>(details) = sampleMods().first();
        details.description =
            "Give your farm a new look throughout the year. This sample mod "
            "pairs seasonal textures with thoughtful details for a cozy valley.\n\n"
            "Visit the Files tab on Nexus Mods for requirements, compatibility "
            "notes and installation instructions.";
        details.available = true;
        details.downloadCount = 28540;
        browser->setModDetails(details);
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QVERIFY(instanceWindow->grab().save(screenshots + "/mods-selected-sample.png"));
        }
        window.findChild<QPushButton*>("backToLibrary")->click();
        window.findChild<QPushButton*>("editInstance")->click();
        QCOMPARE(browser->findChild<QLabel*>("modTitle")->text(),
                 QString("Seasonal farm buildings"));
        instanceWindow->resize(820, 600);
        QTest::qWait(40);
        QCOMPARE(instanceWindow->size(), QSize(820, 600));
        auto* download = browser->findChild<QPushButton*>("modDownload");
        QVERIFY(download->isVisible());
        const auto buttonRect = QRect(download->mapTo(instanceWindow, QPoint(0, 0)), download->size());
        QVERIFY(instanceWindow->rect().contains(buttonRect));
        if (!screenshots.isEmpty()) {
            QVERIFY(instanceWindow->grab().save(screenshots + "/mods-minimum-sample.png"));
        }
    }

    void changingModCancelsPreviousDetailsRequest() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.findChild<QPushButton*>("editInstance")->click();
        const auto browsers = window.findChildren<ModBrowser*>();
        QCOMPARE(browsers.size(), 2);
        for (auto* browser : browsers) {
            const auto initialClients =
                browser->findChildren<NexusModClient*>(QString(), Qt::FindDirectChildrenOnly)
                    .size();
            browser->setMods(sampleMods());
            browser->setPagination(0, 3, 3, false);
            auto* list = browser->findChild<QListWidget*>("modList");
            list->setCurrentRow(0);
            auto clients =
                browser->findChildren<NexusModClient*>(QString(), Qt::FindDirectChildrenOnly);
            QCOMPARE(clients.size(), initialClients + 1);
            QPointer<NexusModClient> previousDetails = clients.last();
            list->setCurrentRow(1);
            QVERIFY(previousDetails.isNull());
            QCOMPARE(browser->findChildren<NexusModClient*>(QString(), Qt::FindDirectChildrenOnly)
                         .size(),
                     initialClients + 1);
            browser->beginLoading();
            QCOMPARE(browser->findChildren<NexusModClient*>(QString(), Qt::FindDirectChildrenOnly)
                         .size(),
                     initialClients);
        }
    }
    void defaultPath() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QCOMPARE(config.gameDirectory(),
                 QDir::homePath() + "/.local/share/Steam/steamapps/common/Stardew Valley");
        QVERIFY(!QFileInfo::exists(config.filePath()));
        QVERIFY(config.farmPackEnabled());
    }

    void persistPath() {
        QTemporaryDir temp;
        const auto file = temp.filePath("nested/launcher.ini");
        LauncherConfig config(file);
        QVERIFY(!config.saveGameDirectory("   "));
        QVERIFY(!config.saveGameDirectory("relative/game"));
        QVERIFY(config.saveGameDirectory(temp.filePath("Game with spaces")));
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.filePath("Game with spaces"));
        QCOMPARE(LauncherConfig::expandHome("~/Games"), QDir::homePath() + "/Games");
    }

    void persistFarmPackToggle() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveFarmPackEnabled(false));
        QVERIFY(!LauncherConfig(config.filePath()).farmPackEnabled());
        QVERIFY(config.saveFarmPackEnabled(true));
        QVERIFY(LauncherConfig(config.filePath()).farmPackEnabled());
    }

    void persistFarmPackOpacity() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QCOMPARE(config.farmPackOpacityPercent(), 75);
        QVERIFY(config.saveFarmPackOpacityPercent(42));
        QCOMPARE(LauncherConfig(config.filePath()).farmPackOpacityPercent(), 42);
    }

    void invalidConfigDoesNotUseWorkingDirectory() {
        QTemporaryDir temp;
        const auto file = temp.filePath("launcher.ini");
        QSettings settings(file, QSettings::IniFormat);
        settings.setValue("game/directory", "relative/game");
        settings.sync();
        QCOMPARE(LauncherConfig(file).gameDirectory(),
                 QDir::homePath() + "/.local/share/Steam/steamapps/common/Stardew Valley");
    }

    void createSessionInstance() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        QTimer::singleShot(0, &window, [] {
            if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                dialog->findChild<QLineEdit*>()->setText("Test farm");
                dialog->accept();
            }
        });
        auto* addButton = window.findChild<QPushButton*>("addInstance");
        QVERIFY(addButton);
        addButton->click();
        auto* instances = window.findChild<QListWidget*>("instances");
        QCOMPARE(instances->count(), 2);
        QCOMPARE(instances->currentItem()->data(Qt::UserRole).toString(), QString("Test farm"));
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        QVERIFY(instanceWindow->isVisible());
        QCOMPARE(instanceWindow->findChild<QLabel*>("detailTitle")->text(), QString("Test farm"));
    }

    void navigateAndFilter() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        auto* instances = window.findChild<QListWidget*>("instances");
        auto* search = window.findChild<QLineEdit*>("instanceSearch");
        QVERIFY(instances && search);
        QCOMPARE(instances->count(), 1);
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QTest::qWait(40);
            QApplication::processEvents();
            QVERIFY(window.grab().save(screenshots + "/library.png"));
        }
        search->setText("does not exist");
        QVERIFY(instances->item(0)->isHidden());
        search->clear();
        QVERIFY(!instances->item(0)->isHidden());
        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        QVERIFY(instanceWindow->isVisible());
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QApplication::processEvents();
            QVERIFY(instanceWindow->grab().save(screenshots + "/mods.png"));
        }
        instanceWindow->findChild<QPushButton*>("backToLibrary")->click();
        QVERIFY(!instanceWindow->isVisible());
    }

    void farmPackToggleIsAvailableAndPersistent() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        window.show();
        QApplication::processEvents();
        auto* toggle = window.findChild<QPushButton*>("farmPackToggle");
        auto* opacity = window.findChild<QSlider*>("farmPackOpacity");
        auto* overlay = window.findChild<QLabel*>("farmPackOverlay");
        QVERIFY(toggle);
        QVERIFY(opacity);
        QVERIFY(overlay);
        QVERIFY(toggle->isCheckable());
        QVERIFY(toggle->isEnabled());
        QVERIFY(toggle->isChecked());
        QCOMPARE(opacity->value(), 75);
        QVERIFY(!overlay->pixmap().isNull());
        QVERIFY(overlay->isVisible());

        toggle->click();

        QVERIFY(!toggle->isChecked());
        QVERIFY(!overlay->isVisible());
        QVERIFY(!LauncherConfig(temp.filePath("launcher.ini")).farmPackEnabled());

        toggle->click();
        QVERIFY(overlay->isVisible());
        opacity->setValue(42);
        QCOMPARE(LauncherConfig(temp.filePath("launcher.ini")).farmPackOpacityPercent(), 42);
    }

    void instanceWindowUpdatesLibraryMode() {
        QTemporaryDir temp;
        LauncherWindow window(temp.filePath("launcher.ini"));
        auto* item = window.findChild<QListWidget*>("instances")->currentItem();

        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        instanceWindow->findChild<QComboBox*>("launchMode")->setCurrentIndex(1);

        QCOMPARE(item->data(Qt::UserRole + 1).toBool(), true);
        QCOMPARE(item->text(), QString("Stardew Valley\nModded"));
    }

    void smapiButtonOpensSmapiWindow() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow window(config.filePath());

        auto* smapiButton = window.findChild<QPushButton*>("smapi");
        QVERIFY(smapiButton);
        smapiButton->click();
        auto* smapiWindow = window.findChild<SmapiWindow*>();
        QVERIFY(smapiWindow);
        QVERIFY(smapiWindow->isVisible());
        QCOMPARE(smapiWindow->windowTitle(), QString("SMAPI Settings"));
        QVERIFY(smapiWindow->findChild<QLabel*>("stateLabel")
                    ->text()
                    .contains("not installed", Qt::CaseInsensitive));
    }

    void missingExecutable() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow window(config.filePath());
        bool warningShown = false;
        QTimer::singleShot(0, &window, [&warningShown] {
            if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                warningShown = warning->text().contains("Executable not found");
                warning->accept();
            }
        });
        for (auto* button : window.findChildren<QPushButton*>()) {
            if (button->text() == "▶  Play") button->click();
        }
        QVERIFY(warningShown);
    }

    void moddedLibraryLaunchRequiresSmapi() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow window(config.filePath());
        auto* item = window.findChild<QListWidget*>("instances")->currentItem();
        item->setData(Qt::UserRole + 1, true);

        bool warningShown = false;
        QTimer::singleShot(0, &window, [&warningShown] {
            if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                warningShown = warning->text().contains("SMAPI is not installed");
                warning->accept();
            }
        });
        for (auto* button : window.findChildren<QPushButton*>()) {
            if (button->text() == "▶  Play") button->click();
        }
        QVERIFY(warningShown);
    }

    void moddedInstanceLaunchRequiresSmapi() {
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow window(config.filePath());
        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        instanceWindow->findChild<QComboBox*>("launchMode")->setCurrentIndex(1);

        bool warningShown = false;
        QTimer::singleShot(0, instanceWindow, [&warningShown] {
            if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                warningShown = warning->text().contains("SMAPI is not installed");
                warning->accept();
            }
        });
        instanceWindow->findChild<QPushButton*>("playDetailInstance")->click();
        QVERIFY(warningShown);
    }

    void saveSettings() {
        QTemporaryDir temp;
        const auto file = temp.filePath("launcher.ini");
        LauncherWindow window(file);
        window.findChild<QPushButton*>("editInstance")->click();
        auto* instanceWindow = window.findChild<InstanceWindow*>();
        QVERIFY(instanceWindow);
        auto* path = instanceWindow->findChild<QLineEdit*>("gamePath");
        QVERIFY(path);
        path->setText(temp.path());
        instanceWindow->findChild<QPushButton*>("savePath")->click();
        QCOMPARE(LauncherConfig(file).gameDirectory(), temp.path());
        instanceWindow->findChild<QListWidget*>("navigation")->setCurrentRow(2);
        instanceWindow->resize(820, 600);
        instanceWindow->show();
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QTest::qWait(40);
            QVERIFY(instanceWindow->grab().save(screenshots + "/settings-minimum.png"));
        }
    }
};

QTEST_MAIN(LauncherTests)
#include "LauncherTests.moc"
