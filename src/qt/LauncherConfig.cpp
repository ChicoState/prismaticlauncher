#include "LauncherConfig.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
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
    return saveChickPix(farmPackImage(), farmPackOpacityPercent(), enabled);
}

int LauncherConfig::farmPackOpacityPercent() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return qBound(0, settings.value("appearance/farm-pack-opacity", 75).toInt(), 100);
}

bool LauncherConfig::saveFarmPackOpacityPercent(int percent) {
    return saveChickPix(farmPackImage(), percent, farmPackEnabled());
}

QString LauncherConfig::farmPackImage() const {
    QSettings settings(filePath_, QSettings::IniFormat);
    return settings.value("appearance/farm-pack-image").toString();
}

bool LauncherConfig::saveChickPix(const QString& image, int opacityPercent, bool enabled) {
    if (opacityPercent < 0 || opacityPercent > 100) return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath())) return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    const QStringList keys = {"appearance/farm-pack-image", "appearance/farm-pack-opacity",
                              "appearance/farm-pack-enabled"};
    QList<QVariant> previousValues;
    for (const auto& key : keys) previousValues.append(settings.value(key));
    settings.setValue("appearance/farm-pack-image", image);
    settings.setValue("appearance/farm-pack-opacity", opacityPercent);
    settings.setValue("appearance/farm-pack-enabled", enabled);
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    // Failed syncs can leave pending values visible through other QSettings instances.
    for (qsizetype index = 0; index < keys.size(); ++index) {
        if (previousValues.at(index).isValid()) {
            settings.setValue(keys.at(index), previousValues.at(index));
        } else {
            settings.remove(keys.at(index));
        }
    }
    settings.sync();
    return false;
}
