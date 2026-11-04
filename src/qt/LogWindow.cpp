#include "LogWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <set>
#include <utility>

LogWindow::LogWindow(QWidget* parent)
    : QWidget(parent),
      m_logs(new QTableWidget(this)),
      m_severitySlider(new QSlider(Qt::Horizontal, this)),
      m_thresholdLabel(new QLabel(this)),
      m_logFilter(new QLineEdit(this)) {
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
    m_logs->setColumnCount(4);
    m_logs->setHorizontalHeaderLabels({tr("Timestamp"), tr("Level"), tr("Source"), tr("Message")});
    m_logs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logs->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logs->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_logs->setWordWrap(false);
    m_logs->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_logs->setShowGrid(true);
    m_logs->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_logs->setAccessibleName(tr("Game and mod log output"));
    m_logs->verticalHeader()->hide();
    m_logs->verticalHeader()->setDefaultSectionSize(24);
    auto* header = m_logs->horizontalHeader();
    header->setSectionsClickable(true);
    // Header gestures sort records; Qt's default column-selection gesture would alter diagnostics.
    disconnect(header, &QHeaderView::sectionPressed, m_logs, nullptr);
    disconnect(header, &QHeaderView::sectionEntered, m_logs, nullptr);
    header->setSortIndicatorShown(true);
    header->setSortIndicator(0, Qt::AscendingOrder);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    m_logs->setColumnWidth(0, 120);
    m_logs->setColumnWidth(1, 85);
    m_logs->setColumnWidth(2, 160);
    m_logs->setStyleSheet(
        QStringLiteral("QTableWidget { background-color: #15191f; color: #dce3eb; "
                       "selection-background-color: #375a7f; gridline-color: #424b57; "
                       "border: 1px solid #424b57; } "
                       "QHeaderView::section { border: 0; border-right: 1px solid #697582; "
                       "border-bottom: 1px solid #697582; padding: 4px; }"));
    connect(header, &QHeaderView::sectionClicked, this, &LogWindow::sortByColumn);

    auto* copyAction = new QAction(tr("Copy"), m_logs);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_logs->addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &LogWindow::copySelection);
    m_logs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_logs, &QWidget::customContextMenuRequested, this,
            [this, copyAction](const QPoint& position) {
                QMenu menu(m_logs);
                copyAction->setEnabled(!m_logs->selectedItems().isEmpty());
                menu.addAction(copyAction);
                menu.exec(m_logs->viewport()->mapToGlobal(position));
                copyAction->setEnabled(true);
            });

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
    QTime timestamp = QTime::currentTime();
    if (parsePrefix) {
        auto match = timestampPrefix.match(line);
        if (match.hasMatch()) {
            const QString timeText = line.mid(1, match.capturedStart(1) - 1).trimmed();
            const QStringList timeParts = timeText.split(QLatin1Char('.'));
            const QTime seconds = QTime::fromString(timeParts.front(), QStringLiteral("H:mm:ss"));
            const int milliseconds =
                timeParts.size() > 1
                    ? timeParts.at(1).left(3).leftJustified(3, QLatin1Char('0')).toInt()
                    : 0;
            if (seconds.isValid()) timestamp = seconds.addMSecs(milliseconds);
        }
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
    appendEntry({m_lastSeverity, m_lastSource, message, timestamp});
}

LogWindow::ViewportState LogWindow::captureViewport() const {
    const auto* scrollBar = m_logs->verticalScrollBar();
    ViewportState state;
    state.scrollPosition = scrollBar->value();
    state.atBottom = state.scrollPosition == scrollBar->maximum();
    const int topRow = m_logs->rowAt(0);
    if (topRow >= 0 && topRow < static_cast<int>(m_visibleEntries.size())) {
        state.topSequence = m_visibleEntries.at(topRow).sequence;
        state.topOffset = m_logs->rowViewportPosition(topRow);
    }
    return state;
}

void LogWindow::restoreViewport(const ViewportState& state) {
    if (state.atBottom) {
        // Resolve deferred row layout before finding the new end of the table.
        m_logs->scrollToBottom();
        return;
    }
    m_logs->doItemsLayout();
    int position = state.scrollPosition;
    if (state.topSequence) {
        const auto entry = std::find_if(
            m_visibleEntries.begin(), m_visibleEntries.end(),
            [&state](const Entry& visible) { return visible.sequence == *state.topSequence; });
        if (entry != m_visibleEntries.end()) {
            const int row = static_cast<int>(entry - m_visibleEntries.begin());
            position = m_logs->verticalHeader()->sectionPosition(row) - state.topOffset;
        }
    }
    // If the anchor itself expired or was filtered out, keep the nearest available position.
    m_logs->verticalScrollBar()->setValue(position);
}

void LogWindow::appendEntry(Entry entry) {
    const ViewportState viewport = captureViewport();
    entry.sequence = m_nextSequence++;
    // Expiration follows arrival order even when the table has another sort order.
    if (m_entries.size() == maximumEntries) {
        removeVisibleEntry(m_entries.front().sequence);
        m_entries.pop_front();
    }
    m_entries.push_back(std::move(entry));
    if (matchesFilters(m_entries.back())) appendVisibleEntry(m_entries.back());
    restoreViewport(viewport);
}

bool LogWindow::matchesFilters(const Entry& entry) const {
    if (static_cast<int>(entry.severity) < m_severitySlider->value()) return false;
    const QString query = m_logFilter->text();
    return query.isEmpty() || displayText(entry).contains(query, Qt::CaseInsensitive);
}

bool LogWindow::entryPrecedes(const Entry& left, const Entry& right) const {
    int comparison = 0;
    switch (m_sortColumn) {
        case 0:
            comparison =
                left.timestamp.msecsSinceStartOfDay() - right.timestamp.msecsSinceStartOfDay();
            break;
        case 1:
            comparison = static_cast<int>(left.severity) - static_cast<int>(right.severity);
            break;
        case 2:
            comparison = QString::compare(left.source, right.source, Qt::CaseInsensitive);
            break;
        case 3:
            comparison = QString::compare(left.message, right.message, Qt::CaseInsensitive);
            break;
    }
    // Arrival ties stay stable in both directions rather than reversing with the key.
    if (comparison == 0) return left.sequence < right.sequence;
    return m_sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
}

void LogWindow::appendVisibleEntry(const Entry& entry) {
    const auto position = std::lower_bound(
        m_visibleEntries.begin(), m_visibleEntries.end(), entry,
        [this](const Entry& left, const Entry& right) { return entryPrecedes(left, right); });
    const int row = static_cast<int>(position - m_visibleEntries.begin());
    m_visibleEntries.insert(position, entry);
    m_logs->insertRow(row);
    const QStringList cells{entry.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")),
                            severityName(entry.severity), entry.source, entry.message};
    for (int column = 0; column < cells.size(); ++column) {
        auto* item = new QTableWidgetItem(cells.at(column));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setForeground(severityColor(entry.severity));
        m_logs->setItem(row, column, item);
    }
}

void LogWindow::removeVisibleEntry(quint64 sequence) {
    const auto position =
        std::find_if(m_visibleEntries.begin(), m_visibleEntries.end(),
                     [sequence](const Entry& entry) { return entry.sequence == sequence; });
    if (position == m_visibleEntries.end()) return;
    m_logs->removeRow(static_cast<int>(position - m_visibleEntries.begin()));
    m_visibleEntries.erase(position);
}

void LogWindow::rebuildVisibleEntries() {
    const ViewportState viewport = captureViewport();
    using CellIdentity = std::pair<quint64, int>;
    std::set<CellIdentity> selectedCells;
    for (const auto* item : m_logs->selectedItems()) {
        selectedCells.emplace(m_visibleEntries.at(item->row()).sequence, item->column());
    }
    std::optional<CellIdentity> currentCell;
    if (const auto* item = m_logs->currentItem()) {
        currentCell = CellIdentity{m_visibleEntries.at(item->row()).sequence, item->column()};
    }
    m_logs->setUpdatesEnabled(false);
    m_logs->setRowCount(0);
    m_visibleEntries.clear();
    for (const Entry& entry : m_entries) {
        if (matchesFilters(entry)) appendVisibleEntry(entry);
    }
    // Cell identity survives reordering; filtering drops only identities no longer visible.
    for (int row = 0; row < static_cast<int>(m_visibleEntries.size()); ++row) {
        const quint64 sequence = m_visibleEntries.at(row).sequence;
        for (int column = 0; column < m_logs->columnCount(); ++column) {
            if (selectedCells.contains({sequence, column})) {
                m_logs->item(row, column)->setSelected(true);
            }
        }
        if (currentCell && currentCell->first == sequence) {
            m_logs->setCurrentCell(row, currentCell->second, QItemSelectionModel::NoUpdate);
        }
    }
    restoreViewport(viewport);
    m_logs->setUpdatesEnabled(true);
}

void LogWindow::sortByColumn(int column) {
    if (m_lastClickedColumn != column) {
        m_sortColumn = column;
        m_sortOrder = Qt::AscendingOrder;
    } else if (m_sortOrder == Qt::AscendingOrder) {
        m_sortOrder = Qt::DescendingOrder;
    } else if (column == 0) {
        m_sortOrder = Qt::AscendingOrder;
    } else {
        m_sortColumn = 0;
        m_sortOrder = Qt::AscendingOrder;
    }
    m_lastClickedColumn = m_sortColumn == column ? column : -1;
    m_logs->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    rebuildVisibleEntries();
}

void LogWindow::copySelection() const {
    QStringList rows;
    for (int row = 0; row < m_logs->rowCount(); ++row) {
        QStringList cells;
        for (int column = 0; column < m_logs->columnCount(); ++column) {
            const auto* item = m_logs->item(row, column);
            if (item && item->isSelected()) cells.append(item->text());
        }
        if (!cells.isEmpty()) rows.append(cells.join(QLatin1Char('\t')));
    }
    if (!rows.isEmpty()) QApplication::clipboard()->setText(rows.join(QLatin1Char('\n')));
}

void LogWindow::clearHistory() {
    m_entries.clear();
    m_pendingOutput.clear();
    m_lastSource = QStringLiteral("Game");
    m_lastSeverity = Severity::Info;
    m_skipLineFeed = false;
    m_fragmentedLine = false;
    m_visibleEntries.clear();
    m_logs->setRowCount(0);
}

void LogWindow::updateThresholdLabel() {
    const QString name = severityName(static_cast<Severity>(m_severitySlider->value()));
    m_thresholdLabel->setText(tr("Minimum severity: %1").arg(name));
    m_severitySlider->setAccessibleDescription(
        tr("Show %1 and higher severity messages").arg(name));
}