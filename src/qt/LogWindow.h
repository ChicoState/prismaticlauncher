#pragma once

#include <QString>
#include <QTime>
#include <QWidget>
#include <deque>
#include <optional>
#include <vector>

class QColor;
class QLabel;
class QLineEdit;
class QTableWidget;
class QSlider;

class LogWindow : public QWidget {
    Q_OBJECT

   public:
    enum class Severity { Trace, Debug, Info, Alert, Warn, Error, Fatal };

    /// @brief Creates a reusable console with reversible severity and text filters.
    explicit LogWindow(QWidget* parent = nullptr);
    /// @brief Consumes process fragments, retaining incomplete lines until completion.
    void appendOutput(const QString& output);
    /// @brief Flushes the previous tail and resets attribution for a new process session.
    void beginSession();
    /// @brief Displays any unfinished output without an empty trailing line.
    void flushOutput();
    /// @brief Appends plain-text launcher messages with the supplied severity and source.
    void appendMessage(const QString& message, Severity severity = Severity::Info,
                       const QString& source = QStringLiteral("Launcher"));

   private:
    struct Entry {
        Severity severity;
        QString source;
        QString message;
        QTime timestamp = QTime::currentTime();
        quint64 sequence = 0;
    };
    struct ViewportState {
        std::optional<quint64> topSequence;
        int topOffset = 0;
        int scrollPosition = 0;
        bool atBottom = true;
    };
    static constexpr int maximumEntries = 5000;
    static constexpr int maximumLineLength = 16384;

    /// @brief Returns the canonical display name for a severity threshold.
    static QString severityName(Severity severity);
    /// @brief Maps supported SMAPI and BepInEx severity names to ordered levels.
    static Severity parseSeverity(const QString& name);
    /// @brief Returns a readable severity color on the console background.
    static QColor severityColor(Severity severity);
    /// @brief Formats a physical line with explicit severity and source labels.
    static QString displayText(const Entry& entry);
    /// @brief Parses a completed stream line, inheriting metadata for continuations.
    void consumeLine(const QString& line, bool parsePrefix);
    /// @brief Captures the top visible entry and its pixel offset, or the end-following state.
    ViewportState captureViewport() const;
    /// @brief Restores the same visible entry after row changes, following the end only if
    /// captured.
    void restoreViewport(const ViewportState& state);
    /// @brief Retains one bounded entry and incrementally updates the console.
    void appendEntry(Entry entry);
    /// @brief Returns true for entries satisfying both filters, or false otherwise.
    bool matchesFilters(const Entry& entry) const;
    /// @brief Inserts a colored row in the active sort order without changing selection.
    void appendVisibleEntry(const Entry& entry);
    /// @brief Removes the row for an expired retained entry, if it is visible.
    void removeVisibleEntry(quint64 sequence);
    /// @brief Returns true when left precedes right under the active sort and arrival tie break.
    bool entryPrecedes(const Entry& left, const Entry& right) const;
    /// @brief Cycles a header sort and rebuilds retained rows in the selected order.
    void sortByColumn(int column);
    /// @brief Copies selected visible cells as tab-separated rows to the clipboard.
    void copySelection() const;
    /// @brief Rebuilds filtered and sorted history, preserving visible selection and viewport
    /// identity.
    void rebuildVisibleEntries();
    /// @brief Clears history, unfinished output, and inherited stream metadata.
    void clearHistory();
    /// @brief Updates the visible and accessible severity threshold descriptions.
    void updateThresholdLabel();

    QTableWidget* m_logs;
    QSlider* m_severitySlider;
    QLabel* m_thresholdLabel;
    QLineEdit* m_logFilter;
    std::deque<Entry> m_entries;
    QString m_pendingOutput;
    QString m_lastSource = QStringLiteral("Game");
    Severity m_lastSeverity = Severity::Info;
    std::vector<Entry> m_visibleEntries;
    quint64 m_nextSequence = 0;
    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    int m_lastClickedColumn = -1;
    bool m_skipLineFeed = false;
    bool m_fragmentedLine = false;
};