#pragma once

#include <QDate>
#include <QString>
#include <QVector>

struct FarmPackVariant {
    QDate start;
    QDate end;
    QString imagePath;

    [[nodiscard]] bool matches(const QDate& date) const;
};

struct FarmPackLoadResult;

struct FarmPack {
    QString name;
    QString defaultImagePath;
    QVector<FarmPackVariant> variants;

    [[nodiscard]] QString imageForDate(const QDate& date) const;
    [[nodiscard]] static FarmPackLoadResult load(const QString& manifestPath);
};

struct FarmPackLoadResult {
    FarmPack pack;
    QString error;

    [[nodiscard]] bool has_value() const { return error.isEmpty(); }
};
