#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTime>
#include <QtTest>

#include "../smapi/SmapiFixture.h"
#include "InstanceWindow.h"
#include "LauncherConfig.h"
#include "LauncherWindow.h"
#include "LogWindow.h"

namespace {
/// @brief Reconstructs the visible level/source/message text for retained console assertions.
QString consoleText(const QTableWidget* logs) {
    QStringList lines;
    for (int row = 0; row < logs->rowCount(); ++row) {
        if (logs->isRowHidden(row)) continue;
        lines << QString("[%1] [%2] %3")
                     .arg(logs->item(row, 1)->text(), logs->item(row, 2)->text(),
                          logs->item(row, 3)->text());
    }
    return lines.join('\n');
}
}  // namespace

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

    /// @brief The console exposes separated timestamp, level, source, and literal message cells.
    void consoleHasTimestampedColumns() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY2(logs, "Console output must be a table named logs.");
        QCOMPARE(logs->columnCount(), 4);
        const QStringList headers{"Timestamp", "Level", "Source", "Message"};
        for (int column = 0; column < headers.size(); ++column) {
            QCOMPARE(logs->horizontalHeaderItem(column)->text(), headers.at(column));
        }
        QVERIFY(logs->showGrid());
        window.appendOutput("[09:02:03.125 ERROR Farm Mod] <b>literal</b> & text\n");
        QCOMPARE(logs->rowCount(), 1);
        QCOMPARE(QTime::fromString(logs->item(0, 0)->text(), "HH:mm:ss.zzz"), QTime(9, 2, 3, 125));
        QCOMPARE(logs->item(0, 1)->text(), QString("ERROR"));
        QCOMPARE(logs->item(0, 2)->text(), QString("Farm Mod"));
        QCOMPARE(logs->item(0, 3)->text(), QString("<b>literal</b> & text"));
        QVERIFY(!(logs->item(0, 3)->flags() & Qt::ItemIsEditable));
    }

    /// @brief Header clicks cycle text sorting through ascending, descending, and chronological
    /// reset.
    void textHeaderSortCycleRestoresChronology() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY2(logs, "Console output must expose sortable table headers.");
        window.resize(1000, 600);
        window.show();
        window.appendOutput(
            "[12:00:03 INFO Beta] alpha\n"
            "[12:00:01 WARN Alpha] zulu\n"
            "[12:00:02 INFO Beta] middle\n");
        QApplication::processEvents();
        const auto messages = [logs] {
            QStringList result;
            for (int row = 0; row < logs->rowCount(); ++row) result << logs->item(row, 3)->text();
            return result;
        };
        const auto clickHeader = [logs](int column) {
            auto* header = logs->horizontalHeader();
            QTest::mouseClick(
                header->viewport(), Qt::LeftButton, Qt::NoModifier,
                QPoint(header->sectionViewportPosition(column) + header->sectionSize(column) / 2,
                       header->height() / 2));
        };
        QCOMPARE(messages(), QStringList({"zulu", "middle", "alpha"}));
        clickHeader(3);
        QCOMPARE(messages(), QStringList({"alpha", "middle", "zulu"}));
        clickHeader(3);
        QCOMPARE(messages(), QStringList({"zulu", "middle", "alpha"}));
        clickHeader(3);
        QCOMPARE(messages(), QStringList({"zulu", "middle", "alpha"}));
        clickHeader(2);
        QCOMPARE(logs->item(0, 2)->text(), QString("Alpha"));
        clickHeader(2);
        QCOMPARE(messages(), QStringList({"alpha", "middle", "zulu"}));
        clickHeader(2);
        QCOMPARE(messages(), QStringList({"zulu", "middle", "alpha"}));
    }

    /// @brief Level headers sort severity in both directions before resetting chronological order.
    void levelHeaderSortCycleUsesSeverityOrder() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.show();
        window.appendOutput(
            "[12:00:03 ERROR Mod] error-first\n"
            "[12:00:01 WARN Mod] warning\n"
            "[12:00:02 DEBUG Mod] debug\n"
            "[12:00:04 ERROR Mod] error-second\n");
        QApplication::processEvents();
        auto* header = logs->horizontalHeader();
        const QPoint levelHeader(header->sectionViewportPosition(1) + header->sectionSize(1) / 2,
                                 header->height() / 2);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, levelHeader);
        QCOMPARE(logs->item(0, 3)->text(), QString("debug"));
        QCOMPARE(logs->item(1, 3)->text(), QString("warning"));
        QCOMPARE(logs->item(2, 3)->text(), QString("error-first"));
        QCOMPARE(logs->item(3, 3)->text(), QString("error-second"));
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, levelHeader);
        QCOMPARE(logs->item(0, 3)->text(), QString("error-first"));
        QCOMPARE(logs->item(1, 3)->text(), QString("error-second"));
        QCOMPARE(logs->item(2, 3)->text(), QString("warning"));
        QCOMPARE(logs->item(3, 3)->text(), QString("debug"));
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, levelHeader);
        QCOMPARE(logs->item(0, 3)->text(), QString("warning"));
        QCOMPARE(logs->item(1, 3)->text(), QString("debug"));
        QCOMPARE(logs->item(2, 3)->text(), QString("error-first"));
        QCOMPARE(logs->item(3, 3)->text(), QString("error-second"));
    }

    /// @brief Timestamp sorting compares numeric times and preserves arrival order for equal times.
    void timestampHeaderSortsChronologicallyWithStableTies() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY2(logs, "Console output must expose timestamp sorting.");
        window.show();
        window.appendOutput(
            "[12:00:00.9 INFO Mod] late\n"
            "[12:00:00.10 INFO Mod] first-tie\n"
            "[12:00:00.100 INFO Mod] second-tie\n"
            "[09:00:00 INFO Mod] early\n");
        QApplication::processEvents();
        QCOMPARE(logs->item(0, 3)->text(), QString("early"));
        QCOMPARE(logs->item(1, 3)->text(), QString("first-tie"));
        QCOMPARE(logs->item(2, 3)->text(), QString("second-tie"));
        auto* header = logs->horizontalHeader();
        const QPoint timestampHeader(header->sectionSize(0) / 2, header->height() / 2);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, timestampHeader);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, timestampHeader);
        QCOMPARE(logs->item(0, 3)->text(), QString("late"));
        QCOMPARE(logs->item(1, 3)->text(), QString("first-tie"));
        QCOMPARE(logs->item(2, 3)->text(), QString("second-tie"));
        QCOMPARE(logs->item(3, 3)->text(), QString("early"));
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, timestampHeader);
        QCOMPARE(logs->item(0, 3)->text(), QString("early"));
    }

    /// @brief Untimestamped game and launcher rows receive a valid time captured at receipt.
    void untimestampedRowsReceiveReceiptTime() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY2(logs, "Every console row must expose its timestamp.");
        const QTime before = QTime::currentTime();
        window.appendOutput("plain game output\n[99:99:99 INFO Mod] invalid time\n");
        window.appendMessage("launcher message");
        const QTime after = QTime::currentTime();
        QCOMPARE(logs->rowCount(), 3);
        for (int row = 0; row < logs->rowCount(); ++row) {
            const QTime timestamp = QTime::fromString(logs->item(row, 0)->text(), "HH:mm:ss.zzz");
            QVERIFY(timestamp.isValid());
            if (before <= after) QVERIFY(timestamp >= before && timestamp <= after);
        }
    }

    /// @brief Selecting all and copying exports visible literal cells without changing retained
    /// output.
    void selectedRowsCopyAsPlainText() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.appendMessage("<b>first</b> & text", LogWindow::Severity::Warn, "Farm Mod");
        window.appendMessage("second-line", LogWindow::Severity::Error, "Other Mod");
        window.show();
        logs->setFocus();
        QApplication::processEvents();
        QApplication::clipboard()->clear();
        QTest::keySequence(logs, QKeySequence::SelectAll);
        QVERIFY(!logs->selectedItems().isEmpty());
        QTest::keySequence(logs, QKeySequence::Copy);
        const QString copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains("<b>first</b> & text"));
        QVERIFY(copied.contains("second-line"));
        QVERIFY(copied.contains("Farm Mod"));
        QVERIFY(copied.contains("WARN"));
        QVERIFY(copied.contains(logs->item(0, 0)->text()));
        QCOMPARE(copied.count("second-line"), 1);
        QCOMPARE(logs->rowCount(), 2);
        window.findChild<QLineEdit*>("logFilter")->setText("first");
        QApplication::clipboard()->clear();
        QTest::keySequence(logs, QKeySequence::SelectAll);
        QTest::keySequence(logs, QKeySequence::Copy);
        QVERIFY(QApplication::clipboard()->text().contains("<b>first</b> & text"));
        QVERIFY(!QApplication::clipboard()->text().contains("second-line"));
    }

    /// @brief Copying one selected row excludes unselected output and keeps literal text.
    void copyingSelectedRowExcludesOtherRows() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.appendMessage("selected literal <tag>");
        window.appendMessage("unselected output");
        window.show();
        logs->setFocus();
        QApplication::processEvents();
        logs->selectRow(0);
        QApplication::clipboard()->clear();
        QTest::keySequence(logs, QKeySequence::Copy);
        const QString copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains("selected literal <tag>"));
        QVERIFY(!copied.contains("unselected output"));
        QCOMPARE(logs->rowCount(), 2);
    }

    /// @brief Appending and filtering keep the active message sort and restore retained rows.
    void filteringAndAppendingPreserveActiveSort() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* filter = window.findChild<QLineEdit*>("logFilter");
        auto* severity = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && filter && severity);
        window.show();
        window.appendOutput(
            "[12:00:03 INFO Farm] zulu\n"
            "[12:00:01 ERROR Farm] bravo\n"
            "[12:00:02 ERROR Weather] alpha\n");
        QApplication::processEvents();
        auto* header = logs->horizontalHeader();
        const QPoint messageHeader(header->sectionViewportPosition(3) + header->sectionSize(3) / 2,
                                   header->height() / 2);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, messageHeader);
        filter->setText("FARM");
        severity->setValue(static_cast<int>(LogWindow::Severity::Error));
        QCOMPARE(consoleText(logs), QString("[ERROR] [Farm] bravo"));
        window.appendOutput("[12:00:04 ERROR Farm] aardvark\n");
        QCOMPARE(consoleText(logs), QString("[ERROR] [Farm] aardvark\n[ERROR] [Farm] bravo"));
        filter->clear();
        severity->setValue(0);
        QCOMPARE(consoleText(logs), QString("[ERROR] [Farm] aardvark\n[ERROR] [Weather] alpha\n"
                                            "[ERROR] [Farm] bravo\n[INFO] [Farm] zulu"));
    }

    /// @brief Every threshold hides lower levels and restores retained messages.
    void minimumSeverityFiltersRetainedMessages() {
        LogWindow window;
        auto* slider = window.findChild<QSlider*>("severitySlider");
        auto* logs = window.findChild<QTableWidget*>("logs");
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
                QCOMPARE(consoleText(logs).contains(QString("marker-%1-end").arg(level)),
                         level >= threshold);
            }
        }
        slider->setValue(0);
        QVERIFY(consoleText(logs).contains("marker-0-end"));
        QVERIFY(consoleText(logs).contains("[Example Mod]"));
    }

    /// @brief Split SMAPI records preserve Unicode, CRLF handling, and stack attribution.
    void streamedRecordsPreserveSourceAndSeverity() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput("[12:34:56 TRA");
        QVERIFY(consoleText(logs).isEmpty());
        window.appendOutput(QString::fromUtf8("CE Mód Name] first-message\r"));
        window.appendOutput("\n    stack-detail\r\n[12:34:57 ERROR Other Mod] failure-message\n");
        const auto lines = consoleText(logs).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(consoleText(logs).contains(QString::fromUtf8("[Mód Name] first-message")));
        QVERIFY(consoleText(logs).contains(QString::fromUtf8("[Mód Name]     stack-detail")));
        QVERIFY(!consoleText(logs).contains('\r'));
        slider->setValue(static_cast<int>(LogWindow::Severity::Error));
        QVERIFY(!consoleText(logs).contains("first-message"));
        QVERIFY(!consoleText(logs).contains("stack-detail"));
        QVERIFY(consoleText(logs).contains("failure-message"));
    }

    /// @brief Console and BepInEx prefixes retain source names and explicit severity.
    void consoleFormatsIdentifySources() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput(
            "initial-game-output\n[Farm Mod] console-info\n[Warning: Weather Mod] "
            "weather-warning\n");
        const auto lines = consoleText(logs).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.at(0).contains("[Game]"));
        QVERIFY(lines.at(1).contains("[Farm Mod]"));
        QVERIFY(lines.at(2).contains("[Weather Mod]"));
        slider->setValue(static_cast<int>(LogWindow::Severity::Warn));
        QVERIFY(!consoleText(logs).contains("initial-game-output"));
        QVERIFY(!consoleText(logs).contains("console-info"));
        QVERIFY(consoleText(logs).contains("weather-warning"));
    }

    /// @brief Flushing emits an unfinished plain-text message once, including literal markup.
    void flushingTailDisplaysLiteralTextOnce() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.appendOutput("[Markup Mod] <b>literal</b> & text");
        QVERIFY(consoleText(logs).isEmpty());
        window.flushOutput();
        QVERIFY(consoleText(logs).contains("[Markup Mod]"));
        QVERIFY(consoleText(logs).contains("<b>literal</b> & text"));
        const auto flushed = consoleText(logs);
        window.flushOutput();
        QCOMPARE(consoleText(logs), flushed);
    }

    /// @brief Search and severity combine reversibly across retained records.
    void searchAndSeverityFiltersAreReversible() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* filter = window.findChild<QLineEdit*>("logFilter");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && filter && slider);
        window.appendMessage("first-info", LogWindow::Severity::Info, "Farm Mod");
        window.appendMessage("second-error", LogWindow::Severity::Error, "Farm Mod");
        window.appendMessage("other-error", LogWindow::Severity::Error, "Weather Mod");
        filter->setText("farm");
        slider->setValue(static_cast<int>(LogWindow::Severity::Error));
        QVERIFY(consoleText(logs).contains("second-error"));
        QVERIFY(!consoleText(logs).contains("first-info"));
        QVERIFY(!consoleText(logs).contains("other-error"));
        filter->clear();
        slider->setValue(0);
        QVERIFY(consoleText(logs).contains("first-info"));
        QVERIFY(consoleText(logs).contains("other-error"));
    }

    /// @brief Clearing discards pending fragments and resets inherited metadata.
    void clearResetsHistoryAndStreamContext() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* clear = window.findChild<QPushButton*>("clearLogs");
        QVERIFY(logs && clear);
        window.appendOutput("[12:34:56 ERROR Old Mod] old-message\npending-old-tail");
        clear->click();
        QVERIFY(consoleText(logs).isEmpty());
        window.appendOutput("new-game-message\n");
        window.flushOutput();
        QVERIFY(consoleText(logs).contains("[Game]"));
        QVERIFY(!consoleText(logs).contains("Old Mod"));
        QVERIFY(!consoleText(logs).contains("pending-old-tail"));
    }

    /// @brief History evicts old records even while the severity filter hides them.
    void historyEvictsOldestRecords() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.show();
        QApplication::processEvents();
        auto* header = logs->horizontalHeader();
        const QPoint messageHeader(header->sectionViewportPosition(3) + header->sectionSize(3) / 2,
                                   header->height() / 2);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, messageHeader);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, messageHeader);
        slider->setValue(static_cast<int>(LogWindow::Severity::Fatal));
        for (int index = 0; index < 5002; ++index) {
            window.appendMessage(QString("record-%1-end").arg(index));
        }
        slider->setValue(0);
        const auto text = consoleText(logs);
        QVERIFY(!text.contains("record-0-end"));
        QVERIFY(!text.contains("record-1-end"));
        QVERIFY(text.contains("record-2-end"));
        QVERIFY(text.contains("record-5001-end"));
        QCOMPARE(text.split('\n', Qt::SkipEmptyParts).size(), 5000);
    }
    /// @brief Native SMAPI aliases and split-screen stamps preserve the actual mod name.
    void nativeSmapiStamps() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        auto* slider = window.findChild<QSlider*>("severitySlider");
        QVERIFY(logs && slider);
        window.appendOutput(
            "[12:00:00 SUCCESS screen_1 Farm Mod] success-marker\n"
            "[12:00:01 CRITICAL screen_2 Farm Mod] fatal-marker\n");
        QVERIFY(consoleText(logs).contains("[INFO] [Farm Mod] success-marker"));
        QVERIFY(consoleText(logs).contains("[FATAL] [Farm Mod] fatal-marker"));
        slider->setValue(static_cast<int>(LogWindow::Severity::Fatal));
        QVERIFY(!consoleText(logs).contains("success-marker"));
        QVERIFY(consoleText(logs).contains("fatal-marker"));
    }

    /// @brief A new process resets attribution and CRLF state while retaining history.
    void newSessionResetsAttribution() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.appendOutput("[12:00:00 ERROR Old Mod] retained\r");
        window.beginSession();
        window.appendOutput("fresh-message\n");
        QVERIFY(consoleText(logs).contains("[ERROR] [Old Mod] retained"));
        QVERIFY(consoleText(logs).contains("[INFO] [Game] fresh-message"));
        window.appendOutput("[Old Mod] pending-tail");
        window.beginSession();
        QVERIFY(consoleText(logs).contains("[Old Mod] pending-tail"));
    }

    /// @brief Long streamed lines keep complete Unicode characters at the split boundary.
    void longLinesPreserveUnicode() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        const QString prefix = "[12:00:00 WARN Farm Mod] ";
        const QString padding(16383 - prefix.size(), QLatin1Char('x'));
        const QString emoji = QString::fromUtf8("🌱");
        window.appendOutput(prefix + padding + emoji + "tail\n");
        const auto lines = consoleText(logs).split('\n');
        QCOMPARE(lines.size(), 2);
        QVERIFY(lines.at(0).startsWith("[WARN] [Farm Mod] "));
        QVERIFY(lines.at(1).startsWith("[WARN] [Farm Mod] "));
        QVERIFY(consoleText(logs).contains(emoji));
        QVERIFY(lines.at(0).endsWith("tail") || lines.at(1).endsWith("tail"));
        window.beginSession();
        window.appendOutput("fresh-after-fragment\n");
        QVERIFY(consoleText(logs).contains("[INFO] [Game] fresh-after-fragment"));
    }

    /// @brief Visible history eviction removes old lines and reopening retains new output.
    void visibleEvictionAndReopen() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        for (int index = 0; index < 5002; ++index) {
            window.appendMessage(QString("visible-%1-end").arg(index));
        }
        QCOMPARE(logs->rowCount(), 5000);
        QVERIFY(!consoleText(logs).contains("visible-0-end"));
        QVERIFY(!consoleText(logs).contains("visible-1-end"));
        QVERIFY(consoleText(logs).contains("visible-2-end"));
        window.show();
        window.close();
        window.appendMessage("while-closed");
        window.show();
        QVERIFY(consoleText(logs).contains("while-closed"));
    }

    /// @brief New output preserves the scroll position when reviewing earlier messages.
    void appendingPreservesScrollPosition() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.show();
        for (int index = 0; index < 150; ++index) window.appendMessage(QString::number(index));
        QApplication::processEvents();
        auto* scroll = logs->verticalScrollBar();
        QVERIFY(scroll->maximum() > 0);
        scroll->setValue(5);
        window.appendMessage("new-line");
        QApplication::processEvents();
        QCOMPARE(scroll->value(), 5);
        scroll->setValue(scroll->maximum());
        window.appendMessage("follow-latest-line");
        QApplication::processEvents();
        QCOMPARE(scroll->value(), scroll->maximum());
    }

    /// @brief Inserting a newest row above a descending viewport preserves the reviewed message.
    void sortedInsertionPreservesViewedMessage() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.show();
        for (int index = 0; index < 150; ++index) {
            window.appendOutput(QString("[%1 INFO Mod] review-%2\n")
                                    .arg(QTime(12, 0).addSecs(index).toString("HH:mm:ss"))
                                    .arg(index));
        }
        QApplication::processEvents();
        auto* header = logs->horizontalHeader();
        const QPoint timestampHeader(header->sectionSize(0) / 2, header->height() / 2);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, timestampHeader);
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, timestampHeader);
        logs->scrollToItem(logs->item(50, 3), QAbstractItemView::PositionAtTop);
        QApplication::processEvents();
        const int topRowBefore = logs->rowAt(1);
        QVERIFY(topRowBefore >= 0);
        const QString reviewedMessage = logs->item(topRowBefore, 3)->text();
        QCOMPARE(reviewedMessage, QString("review-99"));
        window.appendOutput("[12:03:00 INFO Mod] newest-inserted-first\n");
        QApplication::processEvents();
        const int topRowAfter = logs->rowAt(1);
        QVERIFY(topRowAfter >= 0);
        QCOMPARE(logs->item(topRowAfter, 3)->text(), reviewedMessage);
    }

    /// @brief Evicting a retained row before the viewport preserves the reviewed message.
    void historyEvictionPreservesViewedMessage() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.show();
        for (int index = 0; index < 5000; ++index) {
            window.appendOutput(QString("[%1 INFO Mod] retained-%2\n")
                                    .arg(QTime(12, 0).addSecs(index).toString("HH:mm:ss"))
                                    .arg(index));
        }
        QApplication::processEvents();
        logs->scrollToItem(logs->item(50, 3), QAbstractItemView::PositionAtTop);
        QApplication::processEvents();
        const int topRowBefore = logs->rowAt(1);
        QVERIFY(topRowBefore >= 0);
        const QString reviewedMessage = logs->item(topRowBefore, 3)->text();
        QCOMPARE(reviewedMessage, QString("retained-50"));
        window.appendOutput("[14:00:00 INFO Mod] newest-after-eviction\n");
        QApplication::processEvents();
        QCOMPARE(logs->rowCount(), 5000);
        const int topRowAfter = logs->rowAt(1);
        QVERIFY(topRowAfter >= 0);
        QCOMPARE(logs->item(topRowAfter, 3)->text(), reviewedMessage);
        QVERIFY(!consoleText(logs).contains("[Mod] retained-0\n"));
    }

    /// @brief Header sort cycles preserve the diagnostic selection, current cell, and copied row.
    void sortingPreservesSelectedDiagnostic() {
        LogWindow window;
        auto* logs = window.findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        window.show();
        window.appendOutput(
            "[12:00:01 ERROR Mod] selected-diagnostic\n"
            "[12:00:02 INFO Mod] alpha-other\n"
            "[12:00:03 WARN Mod] zulu-other\n");
        QApplication::processEvents();
        logs->selectRow(0);
        logs->setCurrentCell(0, 3, QItemSelectionModel::NoUpdate);
        QCOMPARE(logs->currentItem()->text(), QString("selected-diagnostic"));
        QCOMPARE(logs->selectedItems().size(), 4);
        auto* header = logs->horizontalHeader();
        const QPoint messageHeader(header->sectionViewportPosition(3) + header->sectionSize(3) / 2,
                                   header->height() / 2);
        for (int click = 0; click < 3; ++click) {
            QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, messageHeader);
            QApplication::processEvents();
            QVERIFY(logs->currentItem());
            QCOMPARE(logs->currentItem()->column(), 3);
            QCOMPARE(logs->currentItem()->text(), QString("selected-diagnostic"));
            const auto selected = logs->selectedItems();
            QCOMPARE(selected.size(), 4);
            for (const auto* cell : selected) {
                QCOMPARE(logs->item(cell->row(), 3)->text(), QString("selected-diagnostic"));
            }
            logs->setFocus();
            QApplication::clipboard()->clear();
            QTest::keySequence(logs, QKeySequence::Copy);
            const QString copied = QApplication::clipboard()->text();
            QVERIFY(copied.contains("selected-diagnostic"));
            QVERIFY(!copied.contains("alpha-other"));
            QVERIFY(!copied.contains("zulu-other"));
        }
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
        auto* logs = console->findChild<QTableWidget*>("logs");
        QVERIFY(logs);
        QTRY_VERIFY(consoleText(logs).contains("Game exited with code 0."));
        QVERIFY(consoleText(logs).contains(QString::fromUtf8("[ERROR] [Fixture Mod] café")));
        QVERIFY(consoleText(logs).contains("[WARN] [Fixture Mod] stderr-marker"));
        QVERIFY(consoleText(logs).contains("[INFO] [Fixture Mod] final-tail"));
        // The instance Play control uses the same capture path after the first process exits.
        instance->findChild<QPushButton*>("playDetailInstance")->click();
        QTRY_COMPARE(consoleText(logs).count("Game exited with code 0."), 2);
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

    /// @brief Captures the populated Console tab inside its instance when requested by the runner.
    void consolePreview() {
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (screenshots.isEmpty()) return;
        QVERIFY(QDir().mkpath(screenshots));
        QTemporaryDir temp;
        LauncherConfig config(temp.filePath("launcher.ini"));
        InstanceWindow window("Console fixture", false, &config);
        auto* navigation = window.findChild<QListWidget*>("navigation");
        QVERIFY(navigation);
        const auto consoleItems = navigation->findItems("Console", Qt::MatchExactly);
        QCOMPARE(consoleItems.size(), 1);
        navigation->setCurrentItem(consoleItems.first());
        auto* console = window.findChild<LogWindow*>();
        QVERIFY(console);
        console->appendMessage("Launching Stardew Valley");
        console->appendOutput(
            "[12:00:00 INFO SMAPI] Loaded 2 mods\n"
            "[12:00:01 DEBUG Farm Mod] Preparing farm textures\n"
            "[12:00:02 WARN Weather Mod] Using default weather configuration\n"
            "[12:00:03 ERROR Farm Mod] Example diagnostic message\n");
        window.show();
        QApplication::processEvents();
        QVERIFY(window.grab().save(screenshots + "/log-window.png"));
        window.resize(820, 600);
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
        if (!SmapiTestFixtures::createSmapiFixture(root, {4, 5, 2}, {4, 5, 2})) return false;
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
