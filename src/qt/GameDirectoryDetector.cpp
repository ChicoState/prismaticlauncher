#include "GameDirectoryDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <utility>

#include "LauncherConfig.h"

namespace {
QString installationAt(const QString& candidate) {
    auto path = QDir::fromNativeSeparators(LauncherConfig::expandHome(candidate.trimmed()));
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return {};
    path = QDir::cleanPath(path);
#ifdef Q_OS_WIN
    const auto executable = QString("Stardew Valley.exe");
#elif defined(Q_OS_MACOS)
    // Settings and launch actions use the bundle root; SMAPI lives inside the bundle.
    if (path.endsWith("/Contents/MacOS")) path.chop(QString("/Contents/MacOS").size());
    const auto executable = QString("Contents/MacOS/StardewValley");
#else
    const auto executable = QString("StardewValley");
#endif
    return QFileInfo(QDir(path).filePath(executable)).isFile() ? path : QString();
}

QStringList additionalLibraries(const QString& steamRoot) {
    constexpr qint64 maximumSize = 1024 * 1024;
    const QRegularExpression pathEntry(R"vdf("(?:path|[0-9]+)"\s*"((?:\\.|[^"\\])*)")vdf");
    QStringList libraries;
    for (const auto& name : {"steamapps/libraryfolders.vdf", "config/libraryfolders.vdf"}) {
        const auto path = QDir(steamRoot).filePath(name);
        if (!QFileInfo(path).isFile() || QFileInfo(path).size() > maximumSize) continue;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto bytes = file.read(maximumSize + 1);
        if (bytes.size() > maximumSize) continue;
        auto entries = pathEntry.globalMatch(QString::fromUtf8(bytes));
        while (entries.hasNext() && libraries.size() < 128) {
            auto library = entries.next().captured(1);
            library.replace("\\\\", "\\");
            library.replace("\\\"", "\"");
            library = QDir::fromNativeSeparators(library);
            if (QDir::isAbsolutePath(library)) libraries << QDir::cleanPath(library);
        }
    }
    libraries.removeDuplicates();
    return libraries;
}
}  // namespace

GameDirectoryDetector::GameDirectoryDetector() {
    const auto home = QDir::homePath();
#ifdef Q_OS_WIN
    QSettings steam("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
    steamRoots_ << steam.value("SteamPath").toString();
    QSettings game(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
        "\\Uninstall\\Steam App 413150",
        QSettings::NativeFormat);
    gameDirectories_ << game.value("InstallLocation").toString();
    QSettings gog("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games\\1453375253",
                  QSettings::Registry32Format);
    gameDirectories_ << gog.value("PATH").toString();
    for (const auto& variable : {"ProgramFiles(x86)", "ProgramFiles"}) {
        const auto programs = qEnvironmentVariable(variable);
        if (programs.isEmpty()) continue;
        steamRoots_ << QDir(programs).filePath("Steam");
        gameDirectories_ << QDir(programs).filePath("GOG Galaxy/Games/Stardew Valley")
                         << QDir(programs).filePath("GOG Games/Stardew Valley");
    }
#elif defined(Q_OS_MACOS)
    steamRoots_ << home + "/Library/Application Support/Steam";
    gameDirectories_ << "/Applications/Stardew Valley.app"
                     << home + "/Applications/Stardew Valley.app";
#else
    const auto dataHome = qEnvironmentVariable("XDG_DATA_HOME", home + "/.local/share");
    steamRoots_ << dataHome + "/Steam" << home + "/.local/share/Steam" << home + "/.steam/steam"
                << home + "/.steam/root" << home + "/.var/app/com.valvesoftware.Steam/data/Steam"
                << home + "/snap/steam/common/.local/share/Steam";
    gameDirectories_ << home + "/GOG Games/Stardew Valley/game"
                     << home + "/GOGGames/StardewValley/game";
#endif
    steamRoots_.removeDuplicates();
}

GameDirectoryDetector::GameDirectoryDetector(QStringList steamRoots, QStringList gameDirectories)
    : steamRoots_(std::move(steamRoots)), gameDirectories_(std::move(gameDirectories)) {}

QString GameDirectoryDetector::detect(const QString& preferredDirectory) const {
    if (const auto directory = installationAt(preferredDirectory); !directory.isEmpty())
        return directory;

    for (const auto& root : steamRoots_) {
        const auto steamRoot = QDir::fromNativeSeparators(root);
        if (steamRoot.isEmpty() || !QDir::isAbsolutePath(steamRoot)) continue;
        QStringList libraries{steamRoot};
        libraries.append(additionalLibraries(steamRoot));
        for (const auto& library : libraries) {
            for (const auto& steamApps : {"steamapps", "SteamApps"}) {
                const auto candidate =
                    QDir(library).filePath(QString(steamApps) + "/common/Stardew Valley");
                if (const auto directory = installationAt(candidate); !directory.isEmpty())
                    return directory;
            }
        }
    }
    for (const auto& candidate : gameDirectories_) {
        if (const auto directory = installationAt(candidate); !directory.isEmpty())
            return directory;
    }
    return {};
}
