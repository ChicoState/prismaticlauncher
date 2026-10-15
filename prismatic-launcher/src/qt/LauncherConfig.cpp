#include "LauncherConfig.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <utility>

LauncherConfig::LauncherConfig(QString filePath) : filePath_(std::move(filePath)) {
}

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

QString LauncherConfig::filePath() const {
    return filePath_;
}

bool LauncherConfig::saveGameDirectory(const QString& directory) {
    const auto path = expandHome(directory.trimmed());
    if (path.isEmpty() || !QDir::isAbsolutePath(path))
        return false;
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath()))
        return false;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setValue("game/directory", QDir::cleanPath(path));
    settings.sync();
    return settings.status() == QSettings::NoError;
}
