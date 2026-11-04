#include "ModInstaller.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <algorithm>
#include <array>
#include <memory>
#include <nlohmann/json.hpp>

namespace {
constexpr qint64 maxFileBytes = 512LL * 1024 * 1024;
constexpr qint64 maxTotalBytes = 2LL * 1024 * 1024 * 1024;
constexpr int maxEntries = 100000;
constexpr int maxDepth = 64;

[[noreturn]] void fail(const QString& message) { throw message; }

QString key(const QString& text) {
    return text.normalized(QString::NormalizationForm_C).toCaseFolded();
}

bool safeName(const QString& name) {
    static const QRegularExpression invalid(R"([<>:"/\\|?*\x00-\x1f])");
    static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\.))",
                                             QRegularExpression::CaseInsensitiveOption);
    return !name.isEmpty() && name != "." && name != ".." && !name.endsWith('.') &&
           !name.endsWith(' ') && !name.contains(invalid) && !reserved.match(name).hasMatch();
}

QString gameRoot(const QString& directory) {
    const auto bundle = QDir(directory).filePath("Contents/MacOS");
    return QDir(QFileInfo(bundle).isDir() ? bundle : directory).absolutePath();
}

void validateGame(const QString& directory) {
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory) || !QFileInfo(directory).isDir())
        fail("Choose an existing Stardew Valley game directory in Settings.");
    const QDir game(gameRoot(directory));
    const QStringList markers{"StardewValley.dll", "Stardew Valley.exe",    "StardewValley.exe",
                              "StardewValley",     "StardewModdingAPI.exe", "StardewModdingAPI"};
    if (std::none_of(markers.begin(), markers.end(), [&game](const QString& marker) {
            return QFileInfo(game.filePath(marker)).isFile();
        }))
        fail("This folder does not contain Stardew Valley. Choose the game directory in Settings.");
    const QFileInfo mods(game.filePath("Mods"));
    if (mods.isSymLink() || (mods.exists() && !mods.isDir()))
        fail("The Mods destination must be a regular directory, not a link or file.");
}

struct Budget {
    int entries = 0;
    qint64 bytes = 0;
    void entry(int depth) {
        if (++entries > maxEntries || depth > maxDepth)
            fail("The selection exceeds the 100,000-entry or 64-level folder limit.");
    }
    void data(qint64 count, qint64 fileBytes) {
        bytes += count;
        if (fileBytes > maxFileBytes || bytes > maxTotalBytes)
            fail("The selection exceeds the 512 MiB file or 2 GiB total size limit.");
    }
};

bool isArchive(const QFileInfo& info) {
    return info.isFile() && QStringList{"zip", "7z", "rar"}.contains(info.suffix().toLower());
}

void extractArchive(const QString& source, const QString& destination, Budget& budget) {
    using Reader = std::unique_ptr<archive, decltype(&archive_read_free)>;
    Reader reader(archive_read_new(), archive_read_free);
    if (!reader) fail("Cannot initialize archive reader.");
    // These formats handle their own compression. Outer filters can fall back to
    // spawning external tools, which a local mod import must never do.
    archive_read_support_filter_none(reader.get());
    archive_read_support_format_zip(reader.get());
    archive_read_support_format_7zip(reader.get());
    archive_read_support_format_rar(reader.get());
    archive_read_support_format_rar5(reader.get());
#ifdef Q_OS_WIN
    const auto opened =
        archive_read_open_filename_w(reader.get(), source.toStdWString().c_str(), 65536);
#else
    const auto opened =
        archive_read_open_filename(reader.get(), QFile::encodeName(source).constData(), 65536);
#endif
    if (opened != ARCHIVE_OK)
        fail("Cannot open archive; it may be corrupt or unsupported: " + source);
    QHash<QString, QString> spellings;
    QSet<QString> entries;
    archive_entry* entry = nullptr;
    int status = ARCHIVE_OK;
    while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const auto* raw = archive_entry_pathname_utf8(entry);
        if (!raw) fail("Archive contains an unreadable filename.");
        auto path = QString::fromUtf8(raw);
        path.replace('\\', '/');
        if (path.startsWith('/')) fail("Archive contains an absolute path: " + path);
        auto parts = path.split('/', Qt::SkipEmptyParts);
        while (!parts.isEmpty() && parts.first() == ".") parts.removeFirst();
        budget.entry(static_cast<int>(parts.size()));
        if (parts.isEmpty()) {
            if (archive_entry_filetype(entry) == AE_IFDIR) continue;
            fail("Archive contains an empty filename.");
        }
        QString relative;
        for (const auto& part : parts) {
            if (!safeName(part)) fail("Archive contains an unsafe filename: " + path);
            if (!relative.isEmpty()) relative += '/';
            relative += part;
            const auto folded = key(relative);
            if (spellings.contains(folded) && spellings.value(folded) != relative)
                fail("Archive contains filenames that conflict across platforms: " + path);
            spellings.insert(folded, relative);
        }
        if (entries.contains(key(relative)))
            fail("Archive contains a duplicate entry: " + relative);
        entries.insert(key(relative));
        const auto type = archive_entry_filetype(entry);
        if (archive_entry_symlink(entry) || archive_entry_hardlink(entry) ||
            (type != AE_IFREG && type != AE_IFDIR))
            fail("Archive links and special files cannot be installed: " + relative);
        if (archive_entry_is_encrypted(entry) > 0) fail("Encrypted archives are not supported.");
        const auto target = QDir(destination).filePath(relative);
        if (type == AE_IFDIR) {
            if (!QDir().mkpath(target)) fail("Cannot create archive staging folder.");
            continue;
        }
        if (archive_entry_size(entry) > maxFileBytes)
            fail("An archive file exceeds the 512 MiB size limit.");
        if (!QDir().mkpath(QFileInfo(target).absolutePath()))
            fail("Cannot create archive staging folder.");
        QFile output(target);
        if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            fail("Cannot stage archive file: " + relative);
        std::array<char, 65536> buffer{};
        qint64 fileBytes = 0;
        la_ssize_t count = 0;
        while ((count = archive_read_data(reader.get(), buffer.data(), buffer.size())) > 0) {
            fileBytes += count;
            budget.data(count, fileBytes);
            if (output.write(buffer.data(), count) != count)
                fail("Cannot write archive staging file.");
        }
        if (count < 0) fail("Archive is corrupt or cannot be decoded: " + source);
        if (!output.flush()) fail("Cannot finish archive staging file.");
    }
    if (status != ARCHIVE_EOF || archive_read_close(reader.get()) != ARCHIVE_OK)
        fail("Archive is corrupt or cannot be fully read: " + source);
}

void copyTree(const QString& source, const QString& destination, Budget& budget, int depth = 0) {
    budget.entry(depth);
    const QFileInfo info(source);
    if (info.isSymLink() || (!info.isFile() && !info.isDir()))
        fail("Links and special files cannot be installed: " + source);
    if (!info.isReadable()) fail("Cannot read: " + source);
    if (info.isDir()) {
        if (!QDir().mkpath(destination)) fail("Cannot create staging folder: " + destination);
        QSet<QString> names;
        const auto entries = QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                                        QDir::Hidden | QDir::System);
        for (const auto& child : entries) {
            const auto name = child.fileName();
            if (!safeName(name) || names.contains(key(name)))
                fail("Unsafe or conflicting filename: " + child.filePath());
            names.insert(key(name));
            copyTree(child.filePath(), QDir(destination).filePath(name), budget, depth + 1);
        }
        return;
    }
    if (info.size() > maxFileBytes) fail("A file exceeds the 512 MiB size limit: " + source);
    QFile input(source), output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        fail("Cannot stage file: " + source);
    qint64 fileBytes = 0;
    while (!input.atEnd()) {
        const auto block = input.read(64 * 1024);
        if (block.isEmpty() && input.error() != QFile::NoError) fail("Cannot read: " + source);
        fileBytes += block.size();
        budget.data(block.size(), fileBytes);
        if (output.write(block) != block.size()) fail("Cannot write staging file: " + destination);
    }
    if (!output.flush()) fail("Cannot finish staging file: " + destination);
}

const nlohmann::json* property(const nlohmann::json& json, const char* name) {
    const auto it = json.find(name);
    if (it != json.end()) return &*it;
    // SMAPI binds JSON properties without requiring exact capitalization; its
    // bundled mods use UniqueId while many downloaded mods use UniqueID.
    for (auto candidate = json.begin(); candidate != json.end(); ++candidate) {
        if (QString::fromStdString(candidate.key())
                .compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
            return &candidate.value();
    }
    return nullptr;
}

QString field(const nlohmann::json& json, const char* name) {
    const auto* member = property(json, name);
    if (!member || !member->is_string()) return {};
    const auto value = member->get<std::string>();
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())).trimmed();
}

QByteArray normalizeManifest(QByteArray data) {
    // SMAPI's JSON reader accepts author comments and trailing commas. Preserve
    // quoted strings exactly, including URLs and text containing comma/braces.
    bool quoted = false;
    for (qsizetype i = 0; i < data.size(); ++i) {
        if (quoted) {
            if (data.at(i) == '\\')
                ++i;
            else if (data.at(i) == '"')
                quoted = false;
        } else if (data.at(i) == '"') {
            quoted = true;
        } else if (data.at(i) == '/' && i + 1 < data.size()) {
            const bool line = data.at(i + 1) == '/';
            const bool block = data.at(i + 1) == '*';
            if (!line && !block) continue;
            auto end = line ? data.indexOf('\n', i + 2) : data.indexOf("*/", i + 2);
            if (end < 0 && block) fail("Unterminated comment in mod manifest.");
            end = end < 0 ? data.size() : end + (block ? 2 : 0);
            while (i < end) data[i++] = ' ';
            --i;
        }
    }
    quoted = false;
    for (qsizetype i = 0; i < data.size(); ++i) {
        if (quoted) {
            if (data.at(i) == '\\')
                ++i;
            else if (data.at(i) == '"')
                quoted = false;
        } else if (data.at(i) == '"') {
            quoted = true;
        } else if (data.at(i) == ',') {
            auto next = i + 1;
            while (next < data.size() && QByteArray(" \t\r\n").contains(data.at(next))) ++next;
            if (next < data.size() && (data.at(next) == '}' || data.at(next) == ']')) data[i] = ' ';
        }
    }
    return data;
}

InstalledMod readMod(const QString& directory, bool checkPayload) {
    const auto manifest = QDir(directory).filePath("manifest.json");
    QFile file(manifest);
    if (QFileInfo(manifest).isSymLink() || !file.open(QIODevice::ReadOnly) ||
        file.size() > 1024LL * 1024)
        fail("Cannot read manifest (maximum 1 MiB): " + manifest);
    const auto data = normalizeManifest(file.read(1024LL * 1024 + 1));
    const auto limitNesting = [&manifest](int depth, nlohmann::json::parse_event_t,
                                          nlohmann::json&) {
        if (depth > maxDepth) fail("Manifest exceeds the 64-level JSON nesting limit: " + manifest);
        return true;
    };
    const auto json = nlohmann::json::parse(data.cbegin(), data.cend(), limitNesting, false);
    if (!json.is_object()) fail("Invalid JSON in manifest: " + manifest);
    InstalledMod mod{field(json, "Name"), field(json, "Version"), field(json, "UniqueID"),
                     directory};
    if (mod.name.isEmpty() || mod.version.isEmpty() || mod.uniqueId.isEmpty() ||
        field(json, "Author").isEmpty() || field(json, "Description").isEmpty())
        fail("Manifest needs Name, Author, Version, Description and UniqueID: " + manifest);
    if (checkPayload) {
        const auto dll = field(json, "EntryDll");
        const auto* pack = property(json, "ContentPackFor");
        const bool isPack = pack && pack->is_object() && !field(*pack, "UniqueID").isEmpty();
        if ((!dll.isEmpty()) == isPack)
            fail("Manifest must specify EntryDll or ContentPackFor: " + manifest);
        if (!dll.isEmpty() &&
            (!safeName(dll) || !QFileInfo(QDir(directory).filePath(dll)).isFile()))
            fail("EntryDll is missing or unsafe: " + manifest);
    }
    return mod;
}

void discover(const QString& directory, QList<InstalledMod>& mods, Budget& budget,
              bool checkPayload, int depth = 0) {
    budget.entry(depth);
    const QFileInfo info(directory);
    if (info.isSymLink()) fail("A mod folder cannot be a link: " + directory);
    if (!info.isReadable()) fail("Cannot read mod folder: " + directory);
    if (QFileInfo::exists(QDir(directory).filePath("manifest.json"))) {
        mods.append(readMod(directory, checkPayload));
        return;
    }
    const auto children = QDir(directory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot |
                                                        QDir::Hidden | QDir::System);
    for (const auto& child : children) {
        // SMAPI ignores dot-prefixed folders; never scan an install in progress.
        if (child.fileName().startsWith('.')) continue;
        discover(child.filePath(), mods, budget, checkPayload, depth + 1);
    }
}

bool overlaps(const QString& first, const QString& second) {
    const auto a = key(QDir::cleanPath(first));
    const auto b = key(QDir::cleanPath(second));
    return a == b || a.startsWith(b + '/') || b.startsWith(a + '/');
}

QString sourceRoot(const QString& source) {
    QFileInfo info(source);
    if (!info.exists() || info.isSymLink()) fail("Source is missing or is a link: " + source);
    if (info.isDir()) return info.canonicalFilePath();
    QDir parent = info.absoluteDir();
    for (int depth = 0; depth <= maxDepth; ++depth) {
        if (QFileInfo(parent.filePath("manifest.json")).isFile()) return parent.canonicalPath();
        if (!parent.cdUp()) break;
    }
    fail("No manifest.json found for " + info.fileName() +
         ". Select a complete extracted SMAPI mod folder or download archive. "
         "Loose DLL/XNB files cannot be installed safely.");
}
}  // namespace

QString ModInstaller::modsDirectory(const QString& gameDirectory) {
    return QDir(gameRoot(gameDirectory)).filePath("Mods");
}

ModInstallResult ModInstaller::installed(const QString& gameDirectory) {
    try {
        validateGame(gameDirectory);
        ModInstallResult result;
        const auto directory = modsDirectory(gameDirectory);
        if (QFileInfo::exists(directory)) {
            Budget budget;
            discover(directory, result.mods, budget, false);
        }
        std::sort(result.mods.begin(), result.mods.end(), [](const auto& a, const auto& b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        return result;
    } catch (const QString& error) {
        return {{}, error};
    }
}

ModInstallResult ModInstaller::install(const QStringList& sources, const QString& gameDirectory) {
    QStringList published;
    try {
        validateGame(gameDirectory);
        if (sources.isEmpty()) fail("Select at least one mod folder or file.");
        const auto existing = installed(gameDirectory);
        if (!existing.succeeded()) fail(existing.error);
        const auto game = QFileInfo(gameRoot(gameDirectory)).canonicalFilePath();
        const auto destination = modsDirectory(game);
        QTemporaryDir staging(QDir(game).filePath(".prismatic-install-XXXXXX"));
        if (!staging.isValid()) fail("Cannot stage mods. Check game-folder write permissions.");
        Budget budget;
        QSet<QString> roots;
        QList<InstalledMod> candidates;
        for (const auto& source : sources) {
            const QFileInfo info(source);
            if (!info.exists() || info.isSymLink())
                fail("Source is missing or is a link: " + source);
            const bool archived = isArchive(info);
            const auto root = archived ? info.canonicalFilePath() : sourceRoot(source);
            if (overlaps(root, game)) fail("Select mod downloads outside the game directory.");
            if (roots.contains(root)) continue;
            roots.insert(root);
            const auto package = QDir(staging.path()).filePath(QString::number(roots.size()));
            const auto name = archived ? QString("unpacked") : QFileInfo(root).fileName();
            if (!safeName(name) || name.startsWith('.')) fail("Unsafe mod folder name: " + name);
            const auto target = QDir(package).filePath(name);
            if (archived) {
                if (info.size() > maxFileBytes)
                    fail("The archive exceeds the 512 MiB input size limit.");
                extractArchive(root, target, budget);
            } else {
                copyTree(root, target, budget);
            }
            const auto before = candidates.size();
            Budget scan;
            discover(target, candidates, scan, true);
            if (candidates.size() == before) fail("No SMAPI mod manifests found in: " + source);
            if (archived && candidates.size() == before + 1 &&
                candidates.last().directory == target) {
                const auto folder = candidates.last().uniqueId;
                if (!safeName(folder) || folder.startsWith('.'))
                    fail("Unsafe mod UniqueID: " + folder);
                const auto renamed = QDir(package).filePath(folder);
                if (target != renamed && !QDir().rename(target, renamed))
                    fail("Cannot name staged mod folder.");
                candidates.last().directory = renamed;
            }
        }
        QSet<QString> ids, names;
        for (const auto& mod : existing.mods) ids.insert(key(mod.uniqueId));
        for (const auto& entry :
             QDir(destination)
                 .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
            names.insert(key(entry));
        for (const auto& mod : candidates) {
            const auto name = QFileInfo(mod.directory).fileName();
            if (!safeName(name) || name.startsWith('.')) fail("Unsafe mod folder name: " + name);
            if (ids.contains(key(mod.uniqueId)))
                fail("Mod " + mod.uniqueId +
                     " is already installed or selected more than once. "
                     "Remove the existing copy before installing a replacement.");
            if (names.contains(key(name)))
                fail("A mod folder named " + name + " already exists or is selected twice.");
            ids.insert(key(mod.uniqueId));
            names.insert(key(name));
        }
        if (!QDir().mkpath(destination))
            fail("Cannot create Mods folder. Check write permissions.");
        ModInstallResult result;
        for (auto mod : candidates) {
            const auto target = QDir(destination).filePath(QFileInfo(mod.directory).fileName());
            if (!QDir().rename(mod.directory, target)) fail("Cannot install folder: " + target);
            published.append(target);
            mod.directory = target;
            result.mods.append(mod);
        }
        return result;
    } catch (const QString& error) {
        QString message = error;
        for (const auto& directory : published) {
            if (!QDir(directory).removeRecursively())
                message += "\nCould not roll back new folder; remove it manually: " + directory;
        }
        return {{}, message};
    }
}
