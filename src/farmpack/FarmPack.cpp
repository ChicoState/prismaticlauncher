#include "FarmPack.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringList>
#include <utility>

namespace {

FarmPackLoadResult failure(const QString& message) {
    FarmPackLoadResult result;
    result.error = message;
    return result;
}

bool readString(const QJsonObject& object, const QString& key, QString* value,
                QString* error) {
    const auto jsonValue = object.value(key);
    if (!jsonValue.isString() || jsonValue.toString().trimmed().isEmpty()) {
        *error = "Manifest field '" + key + "' must be a non-empty string.";
        return false;
    }
    *value = jsonValue.toString().trimmed();
    return true;
}

bool parseMonthDay(const QString& value, QDate* date) {
    static const QRegularExpression format("^(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])$");
    if (!format.match(value).hasMatch()) return false;

    const auto parts = value.split('-');
    *date = QDate(2000, parts.at(0).toInt(), parts.at(1).toInt());
    return date->isValid();
}

bool resolveImagePath(const QString& manifestDirectory, const QString& rawPath,
                      const QString& field, QString* resolvedPath, QString* error) {
    QString path = rawPath;
    path.replace('\\', '/');
    path = QDir::fromNativeSeparators(path.trimmed());

    const auto segments = path.split('/', Qt::KeepEmptyParts);
    const bool hasParentSegment = segments.contains("..");
    const bool hasAbsolutePrefix = path.startsWith('/') ||
                                   QRegularExpression("^[A-Za-z]:/").match(path).hasMatch();
    if (path.isEmpty() || hasParentSegment || hasAbsolutePrefix) {
        *error = "Manifest field '" + field + "' must be a relative image path.";
        return false;
    }

    const QFileInfo manifestInfo(manifestDirectory);
    const QFileInfo imageInfo(QDir(manifestDirectory).filePath(path));
    if (!imageInfo.isFile()) {
        *error = "Manifest image does not exist: " + path;
        return false;
    }

    const auto extension = imageInfo.suffix().toLower();
    if (!QStringList({"png", "jpg", "jpeg", "webp"}).contains(extension)) {
        *error = "Manifest image has an unsupported extension: " + path;
        return false;
    }

    if (manifestDirectory.startsWith(":/")) {
        *resolvedPath = QDir(manifestDirectory).filePath(path);
        return true;
    }

    const auto canonicalRoot = manifestInfo.canonicalFilePath();
    const auto canonicalImage = imageInfo.canonicalFilePath();
    const auto relativePath = QDir(canonicalRoot).relativeFilePath(canonicalImage);
    if (relativePath == ".." || relativePath.startsWith("../")) {
        *error = "Manifest image resolves outside the pack directory: " + path;
        return false;
    }

    *resolvedPath = canonicalImage;
    return true;
}

}  // namespace

bool FarmPackVariant::matches(const QDate& date) const {
    if (!date.isValid()) return false;

    const auto day = QDate(2000, date.month(), date.day());
    if (start <= end) return day >= start && day <= end;
    return day >= start || day <= end;
}

QString FarmPack::imageForDate(const QDate& date) const {
    for (const auto& variant : variants) {
        if (variant.matches(date)) return variant.imagePath;
    }
    return defaultImagePath;
}

FarmPackLoadResult FarmPack::load(const QString& manifestPath) {
    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::ReadOnly)) {
        return failure("Could not open FarmPack manifest: " + manifestPath);
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(manifest.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure("FarmPack manifest is not a valid JSON object: " + parseError.errorString());
    }

    FarmPack pack;
    const auto object = document.object();
    QString error;
    if (!readString(object, "name", &pack.name, &error)) return failure(error);

    QString defaultImage;
    if (!readString(object, "default", &defaultImage, &error)) return failure(error);
    const auto manifestDirectory = QFileInfo(manifestPath).absolutePath();
    if (!resolveImagePath(manifestDirectory, defaultImage, "default", &pack.defaultImagePath,
                          &error)) {
        return failure(error);
    }

    const auto variantsValue = object.value("variants");
    if (!variantsValue.isArray()) return failure("Manifest field 'variants' must be an array.");

    const auto variants = variantsValue.toArray();
    pack.variants.reserve(variants.size());
    for (qsizetype index = 0; index < variants.size(); ++index) {
        if (!variants.at(index).isObject()) {
            return failure("Manifest variant " + QString::number(index) + " must be an object.");
        }

        const auto variantObject = variants.at(index).toObject();
        QString startValue;
        QString endValue;
        QString imageValue;
        if (!readString(variantObject, "start", &startValue, &error) ||
            !readString(variantObject, "end", &endValue, &error) ||
            !readString(variantObject, "image", &imageValue, &error)) {
            return failure("Invalid variant " + QString::number(index) + ": " + error);
        }

        FarmPackVariant variant;
        if (!parseMonthDay(startValue, &variant.start) ||
            !parseMonthDay(endValue, &variant.end)) {
            return failure("Invalid date in variant " + QString::number(index) + ". Use MM-DD.");
        }
        if (!resolveImagePath(manifestDirectory, imageValue,
                              "variants[" + QString::number(index) + "].image",
                              &variant.imagePath, &error)) {
            return failure(error);
        }
        pack.variants.push_back(std::move(variant));
    }

    return {.pack = std::move(pack)};
}
