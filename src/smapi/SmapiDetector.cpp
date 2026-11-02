#include "SmapiDetector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>

namespace {
constexpr qint64 kMaximumMetadataSize = 16 * 1024 * 1024;

bool isRegularFile(const QDir& directory, const QString& name) {
    return QFileInfo(directory.filePath(name)).isFile();
}

bool isExecutableFile(const QDir& directory, const QString& name) {
    const QFileInfo file(directory.filePath(name));
#ifdef Q_OS_WIN
    return file.isFile();
#else
    return file.isFile() && file.isExecutable();
#endif
}

QStringList missingFiles(const QDir& gameDirectory) {
    QStringList missing;
#ifdef Q_OS_WIN
    if (!isExecutableFile(gameDirectory, "StardewModdingAPI.exe"))
        missing << "StardewModdingAPI.exe";
#else
    if (!isExecutableFile(gameDirectory, "StardewModdingAPI")) missing << "StardewModdingAPI";
#endif
    if (!isRegularFile(gameDirectory, "StardewModdingAPI.dll")) missing << "StardewModdingAPI.dll";
    if (!isRegularFile(gameDirectory, "StardewModdingAPI.deps.json"))
        missing << "StardewModdingAPI.deps.json";
    if (!isRegularFile(gameDirectory, "StardewModdingAPI.runtimeconfig.json"))
        missing << "StardewModdingAPI.runtimeconfig.json";

    const QDir internal(gameDirectory.filePath("smapi-internal"));
    if (!internal.exists()) {
        missing << "smapi-internal/";
        return missing;
    }
    if (!isRegularFile(internal, "metadata.json")) missing << "smapi-internal/metadata.json";
    if (!isRegularFile(internal, "SMAPI.Toolkit.dll"))
        missing << "smapi-internal/SMAPI.Toolkit.dll";
    if (!isRegularFile(internal, "SMAPI.Toolkit.CoreInterfaces.dll"))
        missing << "smapi-internal/SMAPI.Toolkit.CoreInterfaces.dll";
    return missing;
}

bool hasValidMetadata(const QDir& internal, QString* diagnostic) {
    const QFileInfo metadataFile(internal.filePath("metadata.json"));
    if (!metadataFile.isFile() || metadataFile.size() > kMaximumMetadataSize) {
        *diagnostic = "smapi-internal/metadata.json is missing or too large.";
        return false;
    }

    QFile file(metadataFile.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        *diagnostic = "smapi-internal/metadata.json could not be read.";
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *diagnostic = "smapi-internal/metadata.json is not valid JSON.";
        return false;
    }
    return true;
}
}  // namespace

SmapiInstallation SmapiDetector::inspect(const QString& gameDirectory) const {
    const QFileInfo gameInfo(gameDirectory);
    SmapiInstallation result;
    result.gameDirectory = gameInfo.absoluteFilePath();
    if (!gameInfo.isDir()) {
        result.state = SmapiState::InvalidGamePath;
        result.diagnostic = "The configured Stardew Valley directory does not exist.";
        return result;
    }

    const QDir gameDir(gameInfo.absoluteFilePath());
#ifdef Q_OS_WIN
    result.executablePath = gameDir.filePath("StardewModdingAPI.exe");
#else
    result.executablePath = gameDir.filePath("StardewModdingAPI");
#endif

    const auto missing = missingFiles(gameDir);
    if (missing.isEmpty()) {
        QString metadataDiagnostic;
        if (!hasValidMetadata(QDir(gameDir.filePath("smapi-internal")), &metadataDiagnostic)) {
            result.state = SmapiState::Incomplete;
            result.diagnostic = metadataDiagnostic;
            return result;
        }
    } else {
        const bool hasSmapiFiles = QFileInfo::exists(result.executablePath) ||
                                   QFileInfo::exists(gameDir.filePath("smapi-internal")) ||
                                   QFileInfo::exists(gameDir.filePath("StardewModdingAPI.dll"));
        if (!hasSmapiFiles) {
            result.state = SmapiState::NotInstalled;
            result.diagnostic = "SMAPI was not found in the configured game directory.";
            return result;
        }
        result.state = SmapiState::Incomplete;
        result.diagnostic = "SMAPI is incomplete. Missing: " + missing.join(", ");
        return result;
    }

    const QDir internal(gameDir.filePath("smapi-internal"));
    QString firstError;
    const auto coreVersion =
        versionReader_.read(internal.filePath("SMAPI.Toolkit.dll"), &firstError);
    QString secondError;
    const auto interfacesVersion =
        versionReader_.read(internal.filePath("SMAPI.Toolkit.CoreInterfaces.dll"), &secondError);
    if (!coreVersion || !interfacesVersion) {
        result.state = SmapiState::Unreadable;
        result.diagnostic = !firstError.isEmpty() ? firstError : secondError;
        return result;
    }
    if (*coreVersion != *interfacesVersion) {
        result.state = SmapiState::ComponentMismatch;
        result.version = coreVersion;
        result.diagnostic = "SMAPI internal assemblies report different versions (" +
                            coreVersion->toString() + " and " + interfacesVersion->toString() +
                            ").";
        return result;
    }

    result.state = SmapiState::Installed;
    result.version = coreVersion;
    result.diagnostic = "SMAPI is installed.";
    return result;
}
