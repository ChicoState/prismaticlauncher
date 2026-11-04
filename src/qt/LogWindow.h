#pragma once

#include <QDialog>
#include <QString>
#include <deque>

class QColor;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;

class LogWindow : public QDialog {
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
    /// @brief Retains one bounded entry and incrementally updates the console.
    void appendEntry(Entry entry);
    /// @brief Returns true for entries satisfying both filters, or false otherwise.
    bool matchesFilters(const Entry& entry) const;
    /// @brief Adds a colored plain-text line without moving the user's text cursor.
    void appendVisibleEntry(const Entry& entry);
    /// @brief Removes the oldest visible line when its retained entry expires.
    void removeFirstVisibleEntry();
    /// @brief Rebuilds the view from retained history after a filter changes.
    void rebuildVisibleEntries();
    /// @brief Clears history, unfinished output, and inherited stream metadata.
    void clearHistory();
    /// @brief Updates the visible and accessible severity threshold descriptions.
    void updateThresholdLabel();

    QPlainTextEdit* m_logs;
    QSlider* m_severitySlider;
    QLabel* m_thresholdLabel;
    QLineEdit* m_logFilter;
    std::deque<Entry> m_entries;
    QString m_pendingOutput;
    QString m_lastSource = QStringLiteral("Game");
    Severity m_lastSeverity = Severity::Info;
    int m_visibleEntries = 0;
    bool m_skipLineFeed = false;
    bool m_fragmentedLine = false;
};