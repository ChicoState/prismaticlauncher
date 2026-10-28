#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QtTest>

#include "InstanceWindow.h"
#include "LauncherConfig.h"
#include "LauncherWindow.h"
#include "LogWindow.h"

class LogWindowTests : public QObject {
    Q_OBJECT
   private slots:
    /// @brief Applies the application's resource theme to console verification.
    void initTestCase() {
        Q_INIT_RESOURCE(resources);
        QFile style(":/ui/style.qss");
        QVERIFY(style.open(QIODevice::ReadOnly));
        qApp->setStyle("Fusion");
        qApp->setStyleSheet(style.readAll());
    }

    /// @brief Every threshold hides lower levels and restores retained messages.
    void minimumSeverityFiltersRetainedMessages() {
        LogWindow window;
        auto* slider = window.findChild<QSlider*>("severitySlider");
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(slider && logs);
        QCOMPARE(slider->minimum(), 0);
        QCOMPARE(slider->maximum(), 6);
        QCOMPARE(slider->value(), 0);
        for (int level = 0; level <= 6; ++level) {
            window.appendMessage(QString("marker-%1-end").arg(level),
                                 static_cast<LogWindow::Severity>(level), "Example Mod");
        }
        for (int threshold = 0; threshold <= 6; ++threshold) {
            slider->setValue(threshold);
            for (int level = 0; level <= 6; ++level) {
                QCOMPARE(logs->toPlainText().contains(QString("marker-%1-end").arg(level)),
                         level >= threshold);
            }
        }
        slider->setValue(0);
        QVERIFY(logs->toPlainText().contains("marker-0-end"));
        QVERIFY(logs->toPlainText().contains("[Example Mod]"));
    }

    /// @brief Split SMAPI records preserve Unicode, CRLF handling, and stack attribution.
    void streamedRecordsPreserveSourceAndSeverity() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput("[12:34:56 TRA");
        QVERIFY(logs->toPlainText().isEmpty());
        window.appendOutput(QString::fromUtf8("CE Mód Name] first-message\r"));
        window.appendOutput("\n    stack-detail\r\n[12:34:57 ERROR Other Mod] failure-message\n");
        const auto lines = logs->toPlainText().split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).contains(QString::fromUtf8("[Mód Name]")));
        QVERIFY(lines.at(1).contains(QString::fromUtf8("[Mód Name]")));
        QVERIFY(!logs->toPlainText().contains('\r'));
        slider->setValue(static_cast<int>(LogWindow::Severity::Error));
        QVERIFY(!logs->toPlainText().contains("first-message"));
        QVERIFY(!logs->toPlainText().contains("stack-detail"));
        QVERIFY(logs->toPlainText().contains("failure-message"));
    }

    /// @brief Console and BepInEx prefixes retain source names and explicit severity.
    void consoleFormatsIdentifySources() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput(
            "initial-game-output\n[Farm Mod] console-info\n[Warning: Weather Mod] "
            "weather-warning\n");
        const auto lines = logs->toPlainText().split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).contains("[Game]"));
        QVERIFY(lines.at(1).contains("[Farm Mod]"));
        QVERIFY(lines.at(2).contains("[Weather Mod]"));
        slider->setValue(static_cast<int>(LogWindow::Severity::Warn));
        QVERIFY(!logs->toPlainText().contains("initial-game-output"));
        QVERIFY(!logs->toPlainText().contains("console-info"));
        QVERIFY(logs->toPlainText().contains("weather-warning"));
    }

    /// @brief Flushing emits an unfinished plain-text message once, including literal markup.
    void flushingTailDisplaysLiteralTextOnce() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        window.appendOutput("[Markup Mod] <b>literal</b> & text");
        QVERIFY(logs->toPlainText().isEmpty());
        window.flushOutput();
        QVERIFY(logs->toPlainText().contains("[Markup Mod]"));
        QVERIFY(logs->toPlainText().contains("<b>literal</b> & text"));
        const auto flushed = logs->toPlainText();
        window.flushOutput();
        QCOMPARE(logs->toPlainText(), flushed);
    }

    /// @brief Search and severity combine reversibly across retained records.
    void searchAndSeverityFiltersAreReversible() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* filter = window.findChild<QLineEdit*>("logFilter");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && filter && slider);
        window.appendMessage("first-info", LogWindow::Severity::Info, "Farm Mod");
        window.appendMessage("second-error", LogWindow::Severity::Error, "Farm Mod");
        window.appendMessage("other-error", LogWindow::Severity::Error, "Weather Mod");
        filter->setText("farm");
        slider->setValue(static_cast<int>(LogWindow::Severity::Error));
        QVERIFY(logs->toPlainText().contains("second-error"));
        QVERIFY(!logs->toPlainText().contains("first-info"));
        QVERIFY(!logs->toPlainText().contains("other-error"));
        filter->clear();
        slider->setValue(0);
        QVERIFY(logs->toPlainText().contains("first-info"));
        QVERIFY(logs->toPlainText().contains("other-error"));
    }

    /// @brief Clearing discards pending fragments and resets inherited metadata.
    void clearResetsHistoryAndStreamContext() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* clear = window.findChild<QPushButton*>("clearLogs");
        QVERIFY(logs && clear);
        window.appendOutput("[12:34:56 ERROR Old Mod] old-message\npending-old-tail");
        clear->click();
        QVERIFY(logs->toPlainText().isEmpty());
        window.appendOutput("new-game-message\n");
        window.flushOutput();
        QVERIFY(logs->toPlainText().contains("[Game]"));
        QVERIFY(!logs->toPlainText().contains("Old Mod"));
        QVERIFY(!logs->toPlainText().contains("pending-old-tail"));
    }

    /// @brief History evicts old records even while the severity filter hides them.
    void historyEvictsOldestRecords() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        slider->setValue(static_cast<int>(LogWindow::Severity::Fatal));
        for (int index = 0; index < 5002; ++index) {
            window.appendMessage(QString("record-%1-end").arg(index));
        }
        slider->setValue(0);
        const auto text = logs->toPlainText();
        QVERIFY(!text.contains("record-0-end"));
        QVERIFY(!text.contains("record-1-end"));
        QVERIFY(text.contains("record-2-end"));
        QVERIFY(text.contains("record-5001-end"));
        QCOMPARE(text.split('\n', Qt::SkipEmptyParts).size(), 5000);
    }
    /// @brief Native SMAPI aliases and split-screen stamps preserve the actual mod name.
    void nativeSmapiStamps() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput(
            "[12:00:00 SUCCESS screen_1 Farm Mod] success-marker\n"
            "[12:00:01 CRITICAL screen_2 Farm Mod] fatal-marker\n");
        QVERIFY(logs->toPlainText().contains("[INFO] [Farm Mod] success-marker"));
        QVERIFY(logs->toPlainText().contains("[FATAL] [Farm Mod] fatal-marker"));
        slider->setValue(static_cast<int>(LogWindow::Severity::Fatal));
        QVERIFY(!logs->toPlainText().contains("success-marker"));
        QVERIFY(logs->toPlainText().contains("fatal-marker"));
    }

    /// @brief A new process resets attribution and CRLF state while retaining history.
    void newSessionResetsAttribution() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        window.appendOutput("[12:00:00 ERROR Old Mod] retained\r");
        window.beginSession();
        window.appendOutput("fresh-message\n");
        QVERIFY(logs->toPlainText().contains("[ERROR] [Old Mod] retained"));
        QVERIFY(logs->toPlainText().contains("[INFO] [Game] fresh-message"));
        window.appendOutput("[Old Mod] pending-tail");
        window.beginSession();
        QVERIFY(logs->toPlainText().contains("[Old Mod] pending-tail"));
    }

    /// @brief Long streamed lines keep complete Unicode characters at the split boundary.
    void longLinesPreserveUnicode() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        const QString prefix = "[12:00:00 WARN Farm Mod] ";
        const QString padding(16383 - prefix.size(), QLatin1Char('x'));
        const QString emoji = QString::fromUtf8("🌱");
        window.appendOutput(prefix + padding + emoji + "tail\n");
        const auto lines = logs->toPlainText().split('\n');
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines.at(0).startsWith("[WARN] [Farm Mod] "));
        QVERIFY(lines.at(1).startsWith("[WARN] [Farm Mod] "));
        QVERIFY(logs->toPlainText().contains(emoji));
        QVERIFY(logs->toPlainText().endsWith("tail"));
        window.beginSession();
        window.appendOutput("fresh-after-fragment\n");
        QVERIFY(logs->toPlainText().contains("[INFO] [Game] fresh-after-fragment"));
    }

    /// @brief Visible history eviction removes old lines and reopening retains new output.
    void visibleEvictionAndReopen() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        for (int index = 0; index < 5002; ++index) {
            window.appendMessage(QString("visible-%1-end").arg(index));
        }
        QCOMPARE(logs->document()->blockCount(), 5000);
        QVERIFY(!logs->toPlainText().contains("visible-0-end"));
        QVERIFY(!logs->toPlainText().contains("visible-1-end"));
        QVERIFY(logs->toPlainText().contains("visible-2-end"));
        window.show();
        window.close();
        window.appendMessage("while-closed");
        window.show();
        QVERIFY(logs->toPlainText().contains("while-closed"));
    }

    /// @brief New output preserves the scroll position when reviewing earlier messages.
    void appendingPreservesScrollPosition() {
        LogWindow window;
        auto* logs = window.findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        window.show();
        for (int index = 0; index < 150; ++index) window.appendMessage(QString::number(index));
        QApplication::processEvents();
        auto* scroll = logs->verticalScrollBar();
        QVERIFY(scroll->maximum() > 0);
        scroll->setValue(5);
        window.appendMessage("new-line");
        QCOMPARE(scroll->value(), 5);
    }

    /// @brief Both launch controls capture merged output, split UTF-8, and the final tail.
    void playCapturesProcessOutput() {
#if defined(Q_OS_UNIX)
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QVERIFY(writeFixture(
            temp.path(), QByteArray("#!/bin/sh\n"
                                    "test \"$1\" = --developer-mode || exit 8\n"
                                    "test \"$SMAPI_USE_CURRENT_SHELL\" = true || exit 9\n"
                                    "printf '[12:00:00 ERROR Fixture M'\n"
                                    "sleep 0.03\n"
                                    "printf 'od] caf\\303'\n"
                                    "sleep 0.03\n"
                                    "printf '\\251\\n'\n"
                                    "printf '[12:00:01 WARN Fixture Mod] stderr-marker\\n' >&2\n"
                                    "printf '[12:00:02 INFO Fixture Mod] final-tail'\n")));
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        LauncherWindow launcher(config.filePath());
        launcher.findChild<QListWidget*>("instances")
            ->currentItem()
            ->setData(Qt::UserRole + 1, true);
        launcher.findChild<QPushButton*>("playInstance")->click();
        auto* instance = launcher.findChild<InstanceWindow*>();
        QVERIFY(instance);
        auto* console = instance->findChild<LogWindow*>();
        QVERIFY(console);
        auto* logs = console->findChild<QPlainTextEdit*>("logs");
        QVERIFY(logs);
        QTRY_VERIFY(logs->toPlainText().contains("Game exited with code 0."));
        QVERIFY(logs->toPlainText().contains(QString::fromUtf8("[ERROR] [Fixture Mod] café")));
        QVERIFY(logs->toPlainText().contains("[WARN] [Fixture Mod] stderr-marker"));
        QVERIFY(logs->toPlainText().contains("[INFO] [Fixture Mod] final-tail"));
        // The instance Play control uses the same capture path after the first process exits.
        instance->findChild<QPushButton*>("playDetailInstance")->click();
        QTRY_COMPARE(logs->toPlainText().count("Game exited with code 0."), 2);
#else
        QSKIP("The executable fixture uses a Unix shell; native Windows launch is unverified.");
#endif
    }

    /// @brief Destroying the owning window leaves the captured process alive until exit.
    void destroyingWindowDoesNotKillGame() {
#if defined(Q_OS_UNIX)
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QVERIFY(writeFixture(temp.path(), "#!/bin/sh\nsleep 0.2\ntouch survived.marker\n"));
        LauncherConfig config(temp.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(temp.path()));
        auto* window = new InstanceWindow("Fixture", true, &config);
        window->launchGame();
        delete window;
        QTRY_VERIFY(QFileInfo::exists(temp.filePath("survived.marker")));
#else
        QSKIP("The executable fixture uses a Unix shell; native Windows lifetime is unverified.");
#endif
    }

    /// @brief Captures a populated console screenshot when explicitly requested by the runner.
    void consolePreview() {
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (screenshots.isEmpty()) return;
        QVERIFY(QDir().mkpath(screenshots));
        LogWindow window;
        window.appendMessage("Launching Stardew Valley");
        window.appendOutput(
            "[12:00:00 INFO SMAPI] Loaded 2 mods\n"
            "[12:00:01 DEBUG Farm Mod] Preparing farm textures\n"
            "[12:00:02 WARN Weather Mod] Using default weather configuration\n"
            "[12:00:03 ERROR Farm Mod] Example diagnostic message\n");
        window.show();
        QApplication::processEvents();
        QVERIFY(window.grab().save(screenshots + "/log-window.png"));
        window.resize(560, 320);
        QApplication::processEvents();
        QVERIFY(window.grab().save(screenshots + "/log-window-minimum.png"));
    }

   private:
    /// @brief Returns the executable directory for the platform's existing launch layout.
    static QString fixtureDirectory(const QString& root) {
#ifdef Q_OS_MACOS
        return root + "/Contents/MacOS";
#else
        return root;
#endif
    }

    /// @brief Creates a temporary executable fixture, returning false on any write failure.
    static bool writeFixture(const QString& root, const QByteArray& script) {
        const QString directory = fixtureDirectory(root);
        if (!QDir().mkpath(directory)) return false;
        QFile file(directory + "/StardewModdingAPI");
        if (!file.open(QIODevice::WriteOnly)) return false;
        if (file.write(script) != script.size()) return false;
        file.close();
        return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                   QFileDevice::ExeOwner);
    }
};

QTEST_MAIN(LogWindowTests)
#include "LogWindowTests.moc"