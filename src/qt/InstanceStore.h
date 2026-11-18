#pragma once

#include <QList>
#include <QStringList>

struct InstanceRecord {
    QString name;
    bool modded = false;
    QString directory;
};

class InstanceStore {
   public:
    /// @brief Uses an explicit local library root without opening or modifying it.
    explicit InstanceStore(QString root);
    /// @brief Returns the isolated library path beside an INI configuration.
    static QString rootForConfig(const QString& configFile);
    /// @brief Returns the absolute library root used by this store.
    QString rootDirectory() const;
    /// @brief Creates a new library with one default instance; existing libraries are preserved.
    /// @return True on success, or false with an error if storage cannot be initialized.
    bool initialize(QString* error);
    /// @brief Loads valid instance folders and appends diagnostics for records that cannot be read.
    QList<InstanceRecord> load(QStringList* warnings = nullptr) const;
    /// @brief Publishes a new user-named folder containing atomic metadata and an empty Mods
    /// folder.
    /// @return True after publication, or false with an error and no existing files replaced.
    bool create(const QString& name, bool modded, InstanceRecord* record, QString* error);
    /// @brief Atomically changes an existing healthy instance's mode without renaming its folder.
    /// @return True after commit, or false with an error and the old metadata retained.
    bool saveMode(const QString& name, bool modded, QString* error);

   private:
    QString root_;
};
