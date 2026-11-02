#include "LauncherConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <utility>

LauncherConfig::LauncherConfig(QString filePath) : filePath_(std::move(filePath)) {}

QString LauncherConfig::expandHome(const QString& path) {
    return path.startsWith("~/") ? QDir::homePath() + path.mid(1) : path;
}

QString LauncherConfig::gameDirectory() const {
    QSettings defaults(":/ui/config/defaults.ini", QSettings::IniFormat);
    QSettings settings(filePath_, QSettings::IniFormat);
    const auto fallback = expandHome(defaults.value("game/directory").toString());
    const auto path = expandHome(settings.value("game/directory", fallback).toString().trimmed());
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : fallback);
}

QString LauncherConfig::filePath() const { return filePath_; }

bool LauncherConfig::saveGameDirectory(const QString& directory) {
    const auto path = expandHome(directory.trimmed());
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("game/directory", QDir::cleanPath(path));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool LauncherConfig::farmPackEnabled() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return settings.value("appearance/farm-pack-enabled", true).toBool();
}

bool LauncherConfig::saveFarmPackEnabled(bool enabled) {
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("appearance/farm-pack-enabled", enabled);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

int LauncherConfig::farmPackOpacityPercent() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return qBound(0, settings.value("appearance/farm-pack-opacity", 75).toInt(), 100);
}

bool LauncherConfig::saveFarmPackOpacityPercent(int percent) {
    if (percent < 0 || percent > 100) return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("appearance/farm-pack-opacity", percent);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

int LauncherConfig::farmPackSizePercent() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return qBound(25, settings.value("appearance/farm-pack-size", 100).toInt(), 200);
}

bool LauncherConfig::saveFarmPackSizePercent(int percent) {
    if (percent < 25 || percent > 200) return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("appearance/farm-pack-size", percent);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString LauncherConfig::farmPackImagePath() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return expandHome(settings.value("appearance/farm-pack-image").toString().trimmed());
}

bool LauncherConfig::saveFarmPackImagePath(const QString& imagePath) {
    const auto path = expandHome(imagePath.trimmed());
    if (!path.isEmpty() && !path.startsWith(":/") && !QFileInfo(path).isFile()) return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("appearance/farm-pack-image", path);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString LauncherConfig::farmPackImageDirectory() const {
    return QDir(QFileInfo(filePath_).absolutePath()).filePath("chickens");
}

QString LauncherConfig::importFarmPackImage(const QString& imagePath) const {
    const auto sourcePath = expandHome(imagePath.trimmed());
    const QFileInfo source(sourcePath);
    const QStringList extensions{"png", "jpg", "jpeg", "webp"};
    if (!source.isFile() || !extensions.contains(source.suffix().toLower())) return {};

    const auto directory = farmPackImageDirectory();
    if (!QDir().mkpath(directory)) return {};
    const auto canonicalSource = source.canonicalFilePath();
    const auto canonicalDirectory = QFileInfo(directory).canonicalFilePath();
    if (!canonicalSource.isEmpty() && canonicalSource.startsWith(canonicalDirectory + "/")) {
        return QDir::cleanPath(source.absoluteFilePath());
    }

    const auto baseName = source.completeBaseName();
    const auto suffix = source.suffix();
    auto destination = QDir(directory).filePath(source.fileName());
    int copyIndex = 2;
    while (QFileInfo::exists(destination)) {
        destination = QDir(directory).filePath(
            baseName + "-" + QString::number(copyIndex++) + "." + suffix);
    }
    if (!QFile::copy(source.absoluteFilePath(), destination)) return {};
    return destination;
}
