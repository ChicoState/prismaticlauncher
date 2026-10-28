#include "LogWindow.h"

#include <QColor>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSlider>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <utility>

LogWindow::LogWindow(QWidget* parent)
    : QDialog(parent),
      m_logs(new QPlainTextEdit(this)),
      m_severitySlider(new QSlider(Qt::Horizontal, this)),
      m_thresholdLabel(new QLabel(this)),
      m_logFilter(new QLineEdit(this)) {
    setWindowTitle(tr("Game log"));
    resize(980, 600);
    setMinimumSize(560, 320);

    m_severitySlider->setObjectName("severitySlider");
    m_severitySlider->setRange(static_cast<int>(Severity::Trace),
                               static_cast<int>(Severity::Fatal));
    m_severitySlider->setTickPosition(QSlider::TicksBelow);
    m_severitySlider->setTickInterval(1);
    m_severitySlider->setPageStep(1);
    m_severitySlider->setAccessibleName(tr("Minimum log severity"));
    m_severitySlider->setToolTip(tr("Trace → Debug → Info → Alert → Warn → Error → Fatal"));
    m_thresholdLabel->setMinimumWidth(175);
    m_thresholdLabel->setBuddy(m_severitySlider);
    updateThresholdLabel();

    m_logFilter->setObjectName("logFilter");
    m_logFilter->setPlaceholderText(tr("Search messages or mod names…"));
    m_logFilter->setClearButtonEnabled(true);
    m_logFilter->setAccessibleName(tr("Search log messages and mod names"));
    auto* clearButton = new QPushButton(tr("Clear"), this);
    clearButton->setObjectName("clearLogs");
    clearButton->setToolTip(tr("Clear the retained log history"));
    clearButton->setAutoDefault(false);

    auto* filters = new QHBoxLayout;
    filters->addWidget(m_thresholdLabel);
    filters->addWidget(m_severitySlider, 1);
    filters->addWidget(m_logFilter, 2);
    filters->addWidget(clearButton);

    m_logs->setObjectName("logs");
    m_logs->setReadOnly(true);
    m_logs->setUndoRedoEnabled(false);
    m_logs->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logs->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_logs->setAccessibleName(tr("Game and mod log output"));
    m_logs->setPlaceholderText(
        tr("Game and mod output will appear here when you launch an instance."));
    m_logs->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background-color: #15191f; color: #dce3eb; "
        "selection-background-color: #375a7f; border: 1px solid #424b57; padding: 8px; }"));
    m_logs->document()->setMaximumBlockCount(maximumEntries);

    auto* description =
        new QLabel(tr("Latest 5,000 lines retained • Filters apply to retained history"), this);
    description->setWordWrap(true);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(m_logs, 1);
    layout->addWidget(description);
    connect(m_severitySlider, &QSlider::valueChanged, this, [this] {
        updateThresholdLabel();
        rebuildVisibleEntries();
    });
    connect(m_logFilter, &QLineEdit::textChanged, this, [this] { rebuildVisibleEntries(); });
    connect(clearButton, &QPushButton::clicked, this, &LogWindow::clearHistory);
}

QString LogWindow::severityName(Severity severity) {
    switch (severity) {
        case Severity::Trace:
            return QStringLiteral("TRACE");
        case Severity::Debug:
            return QStringLiteral("DEBUG");
        case Severity::Info:
            return QStringLiteral("INFO");
        case Severity::Alert:
            return QStringLiteral("ALERT");
        case Severity::Warn:
            return QStringLiteral("WARN");
        case Severity::Error:
            return QStringLiteral("ERROR");
        case Severity::Fatal:
            return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

LogWindow::Severity LogWindow::parseSeverity(const QString& name) {
    const QString normalized = name.toUpper();
    if (normalized == "TRACE") return Severity::Trace;
    if (normalized == "DEBUG") return Severity::Debug;
    if (normalized == "ALERT") return Severity::Alert;
    if (normalized == "WARN" || normalized == "WARNING") return Severity::Warn;
    if (normalized == "ERROR") return Severity::Error;
    if (normalized == "FATAL" || normalized == "CRITICAL") return Severity::Fatal;
    return Severity::Info;
}

QColor LogWindow::severityColor(Severity severity) {
    switch (severity) {
        case Severity::Trace:
            return QColor("#a4aeba");
        case Severity::Debug:
            return QColor("#a7c6df");
        case Severity::Info:
            return QColor("#dce3eb");
        case Severity::Alert:
            return QColor("#c5adff");
        case Severity::Warn:
            return QColor("#f4d17b");
        case Severity::Error:
            return QColor("#ff9b94");
        case Severity::Fatal:
            return QColor("#ff8cc6");
    }
    return QColor("#dce3eb");
}

QString LogWindow::displayText(const Entry& entry) {
    return QStringLiteral("[%1] [%2] %3")
        .arg(severityName(entry.severity), entry.source, entry.message);
}

void LogWindow::appendOutput(const QString& output) {
    // Bound unfinished lines too, even when a process never emits a newline.
    for (const QChar character : output) {
        if (m_skipLineFeed) {
            m_skipLineFeed = false;
            if (character == QLatin1Char('\n')) continue;
        }
        const bool isCarriageReturn = character == QLatin1Char('\r');
        const bool isLineEnd = isCarriageReturn || character == QLatin1Char('\n') ||
                               character == QChar::LineSeparator ||
                               character == QChar::ParagraphSeparator;
        if (isLineEnd) {
            consumeLine(m_pendingOutput, !m_fragmentedLine);
            m_pendingOutput.clear();
            m_fragmentedLine = false;
            m_skipLineFeed = isCarriageReturn;
            continue;
        }
        if (m_pendingOutput.size() == maximumLineLength) {
            // Keep a UTF-16 surrogate pair together so parsing sees valid Unicode.
            QString carry;
            if (m_pendingOutput.back().isHighSurrogate() && character.isLowSurrogate()) {
                carry = m_pendingOutput.right(1);
                m_pendingOutput.chop(1);
            }
            consumeLine(m_pendingOutput, !m_fragmentedLine);
            m_pendingOutput = carry;
            m_fragmentedLine = true;
        }
        m_pendingOutput.append(character);
    }
}

void LogWindow::beginSession() {
    flushOutput();
    m_lastSource = QStringLiteral("Game");
    m_lastSeverity = Severity::Info;
    m_skipLineFeed = false;
    m_fragmentedLine = false;
}

void LogWindow::flushOutput() {
    if (!m_pendingOutput.isEmpty()) {
        consumeLine(m_pendingOutput, !m_fragmentedLine);
        m_pendingOutput.clear();
    }
    m_fragmentedLine = false;
}

void LogWindow::appendMessage(const QString& message, Severity severity, const QString& source) {
    QString safeSource = source;
    safeSource.replace(QLatin1Char('\r'), QLatin1Char(' '));
    safeSource.replace(QLatin1Char('\n'), QLatin1Char(' '));
    safeSource.replace(QChar::LineSeparator, QLatin1Char(' '));
    safeSource.replace(QChar::ParagraphSeparator, QLatin1Char(' '));
    if (safeSource.isEmpty()) safeSource = QStringLiteral("Launcher");
    safeSource = safeSource.left(maximumLineLength);

    QString line;
    bool skipLineFeed = false;
    for (const QChar character : message) {
        if (skipLineFeed) {
            skipLineFeed = false;
            if (character == QLatin1Char('\n')) continue;
        }
        const bool isCarriageReturn = character == QLatin1Char('\r');
        const bool isLineEnd = isCarriageReturn || character == QLatin1Char('\n') ||
                               character == QChar::LineSeparator ||
                               character == QChar::ParagraphSeparator;
        if (isLineEnd) {
            appendEntry({severity, safeSource, line});
            line.clear();
            skipLineFeed = isCarriageReturn;
            continue;
        }
        if (line.size() == maximumLineLength) {
            QString carry;
            if (line.back().isHighSurrogate() && character.isLowSurrogate()) {
                carry = line.right(1);
                line.chop(1);
            }
            appendEntry({severity, safeSource, line});
            line = carry;
        }
        line.append(character);
    }
    if (!line.isEmpty() || message.isEmpty()) appendEntry({severity, safeSource, line});
}

void LogWindow::consumeLine(const QString& line, bool parsePrefix) {
    static const QRegularExpression timestampPrefix(
        QStringLiteral(
            R"(^\[\d{1,2}:\d{2}:\d{2}(?:\.\d+)?\s+(TRACE|DEBUG|INFO|MESSAGE|ALERT|WARN|WARNING|ERROR|FATAL|CRITICAL|SUCCESS)\s+(?:screen_\d+\s+)?([^\]]+)\] ?(.*)$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bepinexPrefix(
        QStringLiteral(
            R"(^\[(TRACE|DEBUG|INFO|MESSAGE|ALERT|WARN|WARNING|ERROR|FATAL|CRITICAL|SUCCESS)\s*:\s*([^\]]+)\] ?(.*)$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sourcePrefix(QStringLiteral(R"(^\[([^\]]+)\] ?(.*)$)"));

    QString message = line;
    if (parsePrefix) {
        auto match = timestampPrefix.match(line);
        if (!match.hasMatch()) match = bepinexPrefix.match(line);
        if (match.hasMatch()) {
            m_lastSeverity = parseSeverity(match.captured(1));
            m_lastSource = match.captured(2).trimmed();
            message = match.captured(3);
        } else {
            match = sourcePrefix.match(line);
            if (match.hasMatch()) {
                m_lastSeverity = Severity::Info;
                m_lastSource = match.captured(1).trimmed();
                message = match.captured(2);
            }
        }
    }
    if (m_lastSource.isEmpty()) m_lastSource = QStringLiteral("Game");
    appendEntry({m_lastSeverity, m_lastSource, message});
}

void LogWindow::appendEntry(Entry entry) {
    auto* scrollBar = m_logs->verticalScrollBar();
    const int previousPosition = scrollBar->value();
    const bool wasAtBottom = previousPosition == scrollBar->maximum();
    // Remove expired visible entries even when the incoming entry is filtered out.
    if (m_entries.size() == maximumEntries) {
        if (matchesFilters(m_entries.front())) removeFirstVisibleEntry();
        m_entries.pop_front();
    }
    m_entries.push_back(std::move(entry));
    if (matchesFilters(m_entries.back())) appendVisibleEntry(m_entries.back());
    scrollBar->setValue(wasAtBottom ? scrollBar->maximum() : previousPosition);
}

bool LogWindow::matchesFilters(const Entry& entry) const {
    if (static_cast<int>(entry.severity) < m_severitySlider->value()) return false;
    const QString query = m_logFilter->text();
    return query.isEmpty() || displayText(entry).contains(query, Qt::CaseInsensitive);
}

void LogWindow::appendVisibleEntry(const Entry& entry) {
    QTextCursor cursor(m_logs->document());
    cursor.movePosition(QTextCursor::End);
    if (m_visibleEntries > 0) cursor.insertBlock();
    QTextCharFormat format;
    format.setForeground(severityColor(entry.severity));
    cursor.insertText(displayText(entry), format);
    ++m_visibleEntries;
}

void LogWindow::removeFirstVisibleEntry() {
    if (m_visibleEntries == 0) return;
    QTextCursor cursor(m_logs->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.removeSelectedText();
    if (m_visibleEntries > 1) cursor.deleteChar();
    --m_visibleEntries;
}

void LogWindow::rebuildVisibleEntries() {
    auto* scrollBar = m_logs->verticalScrollBar();
    const int previousPosition = scrollBar->value();
    const bool wasAtBottom = previousPosition == scrollBar->maximum();
    m_logs->setUpdatesEnabled(false);
    m_logs->clear();
    m_visibleEntries = 0;
    for (const Entry& entry : m_entries) {
        if (matchesFilters(entry)) appendVisibleEntry(entry);
    }
    scrollBar->setValue(wasAtBottom ? scrollBar->maximum() : previousPosition);
    m_logs->setUpdatesEnabled(true);
}

void LogWindow::clearHistory() {
    m_entries.clear();
    m_pendingOutput.clear();
    m_lastSource = QStringLiteral("Game");
    m_lastSeverity = Severity::Info;
    m_skipLineFeed = false;
    m_fragmentedLine = false;
    m_visibleEntries = 0;
    m_logs->clear();
}

void LogWindow::updateThresholdLabel() {
    const QString name = severityName(static_cast<Severity>(m_severitySlider->value()));
    m_thresholdLabel->setText(tr("Minimum severity: %1").arg(name));
    m_severitySlider->setAccessibleDescription(
        tr("Show %1 and higher severity messages").arg(name));
}