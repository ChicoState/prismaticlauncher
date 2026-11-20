#include "InstanceStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <algorithm>
#include <utility>

namespace {
/// @brief Sets an optional diagnostic and returns failure to the caller.
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

/// @brief Rejects names that cannot be represented safely as portable, visible folder names.
bool validName(const QString& name, QString* error) {
    if (name.isEmpty() || name != name.trimmed() || name.startsWith('.') || name.endsWith('.') ||
        name.toUtf8().size() > 200) {
        return fail(error,
                    "Use a name of 1–200 UTF-8 bytes without leading/trailing spaces or periods.");
    }
    for (const auto character : name) {
        if (character.category() == QChar::Other_Control ||
            QStringLiteral("<>:\"/\\|?*").contains(character)) {
            return fail(error,
                        "The name contains a character that cannot be used in a folder name.");
        }
    }
    static const QRegularExpression device("^(CON|PRN|AUX|NUL|COM[0-9¹²³]|LPT[0-9¹²³])$",
                                           QRegularExpression::CaseInsensitiveOption);
    if (device.match(name.section('.', 0, 0).trimmed()).hasMatch()) {
        return fail(error, "Choose a name that is not a reserved Windows device name.");
    }
    return true;
}

/// @brief Produces a portable comparison key without altering the user's stored name.
QString nameKey(const QString& name) {
    return name.normalized(QString::NormalizationForm_C).toCaseFolded();
}

/// @brief Rejects missing, linked, or unreadable library roots.
bool readableRoot(const QString& root, QString* error) {
    const QFileInfo info(root);
    if (info.isSymLink() || !info.isDir() || !info.isReadable()) {
        return fail(error, "Cannot read instance library: " + root);
    }
    return true;
}

/// @brief Validates one record and derives all filesystem paths from its containing folder.
bool readRecord(const QString& directory, InstanceRecord* record, QString* error) {
    const QFileInfo folder(directory);
    const QFileInfo metadata(directory + "/instance.json");
    const QFileInfo mods(directory + "/Mods");
    if (folder.isSymLink() || !folder.isDir() || metadata.isSymLink() || !metadata.isFile() ||
        mods.isSymLink() || !mods.isDir()) {
        return fail(error, "Expected an ordinary instance folder, instance.json, and Mods folder.");
    }
    QFile file(metadata.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
        return fail(error, "Cannot read instance metadata, or metadata exceeds 64 KiB.");
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto object = document.object();
    const auto savedName = object.value("name").toString();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        object.value("schemaVersion").toDouble(-1) != 1 || !object.value("name").isString() ||
        savedName.normalized(QString::NormalizationForm_C) !=
            folder.fileName().normalized(QString::NormalizationForm_C) ||
        !object.value("modded").isBool()) {
        return fail(error,
                    "Invalid or unsupported instance metadata; original files were preserved.");
    }
    // Some filesystems decompose Unicode directory names. Validate the original input's length.
    if (!validName(savedName, error)) return false;
    // Trusted library parents may be aliases; child instance and Mods links were rejected above.
    *record = {savedName, object.value("modded").toBool(), folder.canonicalFilePath()};
    return true;
}

/// @brief Atomically writes JSON metadata, retaining unknown fields supplied by the caller.
bool writeMetadata(const QString& directory, const QJsonObject& metadata, QString* error) {
    QSaveFile file(directory + "/instance.json");
    file.setDirectWriteFallback(false);
    const auto bytes = QJsonDocument(metadata).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        return fail(error, "Could not save instance metadata: " + file.errorString());
    }
    return true;
}
}  // namespace

InstanceStore::InstanceStore(QString root) : root_(QFileInfo(std::move(root)).absoluteFilePath()) {}

QString InstanceStore::rootForConfig(const QString& configFile) {
    const QFileInfo config(configFile);
    return QDir(config.absolutePath()).filePath(config.completeBaseName() + ".instances");
}

QString InstanceStore::rootDirectory() const { return root_; }

bool InstanceStore::initialize(QString* error) {
    if (error) error->clear();
    const QFileInfo rootInfo(root_);
    if (rootInfo.exists() || rootInfo.isSymLink()) return readableRoot(root_, error);
    const auto parent = rootInfo.absolutePath();
    if (!QDir().mkpath(parent)) return fail(error, "Could not create the library's parent folder.");
    QLockFile lock(root_ + ".lock");
    if (!lock.tryLock(0)) return fail(error, "The instance library is busy. Try again.");
    if (QFileInfo::exists(root_)) return readableRoot(root_, error);
    QTemporaryDir staging(parent + "/.instances-XXXXXX");
    if (!staging.isValid()) return fail(error, "Could not stage a new instance library.");
    InstanceStore temporary(staging.path());
    InstanceRecord initial;
    if (!temporary.create("Stardew Valley", false, &initial, error)) return false;
    if (!QDir().rename(staging.path(), root_))
        return fail(error, "Could not publish the instance library.");
    staging.setAutoRemove(false);
    return true;
}

QList<InstanceRecord> InstanceStore::load(QStringList* warnings) const {
    QList<InstanceRecord> records;
    QString error;
    if (!readableRoot(root_, &error)) {
        if (warnings) warnings->append(error);
        return records;
    }
    const auto entries = QDir(root_).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    QHash<QString, int> counts;
    for (const auto& entry : entries) ++counts[nameKey(entry.fileName())];
    for (const auto& entry : entries) {
        if (entry.fileName().startsWith('.')) continue;
        InstanceRecord record;
        if (counts.value(nameKey(entry.fileName())) > 1) {
            error = "Folder name collides with another instance entry.";
        } else if (readRecord(entry.absoluteFilePath(), &record, &error)) {
            records.append(record);
            continue;
        }
        if (warnings) warnings->append(entry.fileName() + ": " + error);
    }
    return records;
}

bool InstanceStore::create(const QString& name, bool modded, InstanceRecord* record,
                           QString* error) {
    if (error) error->clear();
    if (!validName(name, error) || !readableRoot(root_, error)) return false;
    QLockFile lock(root_ + "/.instance-store.lock");
    if (!lock.tryLock(0)) return fail(error, "The instance library is busy. Try again.");
    const auto entries = QDir(root_).entryList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                               QDir::Hidden | QDir::System);
    for (const auto& existing : entries) {
        if (nameKey(existing) == nameKey(name))
            return fail(error, "An instance or file with that name already exists.");
    }
    QTemporaryDir staging(root_ + "/.creating-XXXXXX");
    if (!staging.isValid() || !QDir(staging.path()).mkdir("Mods")) {
        return fail(error, "Could not create the instance folder.");
    }
    if (!writeMetadata(staging.path(), {{"schemaVersion", 1}, {"name", name}, {"modded", modded}},
                       error))
        return false;
    const auto destination = QDir(root_).filePath(name);
    if (!QDir().rename(staging.path(), destination))
        return fail(error, "Could not publish the instance folder.");
    staging.setAutoRemove(false);
    if (record) *record = {name, modded, QFileInfo(destination).canonicalFilePath()};
    return true;
}

bool InstanceStore::saveMode(const QString& name, bool modded, QString* error) {
    if (error) error->clear();
    if (!validName(name, error) || !readableRoot(root_, error)) return false;
    QLockFile lock(root_ + "/.instance-store.lock");
    if (!lock.tryLock(0)) return fail(error, "The instance library is busy. Try again.");
    const auto records = load();
    const auto selected = std::find_if(records.cbegin(), records.cend(),
                                       [&name](const auto& record) { return record.name == name; });
    if (selected == records.cend())
        return fail(error, "The instance is missing, invalid, or has a conflicting folder name.");
    const auto& record = *selected;
    QFile file(record.directory + "/instance.json");
    if (!file.open(QIODevice::ReadOnly)) return fail(error, "Could not read instance metadata.");
    auto metadata = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    metadata.insert("modded", modded);
    return writeMetadata(record.directory, metadata, error);
}
