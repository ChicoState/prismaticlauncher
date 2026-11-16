#include "ChickPixImages.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QResource>
#include <QSet>
#include <algorithm>

namespace {
constexpr qint64 maximumFileBytes = 16LL * 1024 * 1024;
constexpr int maximumDimension = 8192;
constexpr qint64 maximumPixels = 16LL * 1024 * 1024;
const QStringList bundledImages = {":/farmpack/default.png", ":/farmpack/spring.png",
                                   ":/farmpack/summer.png"};

/// @brief Identifies decoded pixels and dimensions independently of filenames and encoded metadata.
QByteArray imageFingerprint(const QImage& image) {
    const auto normalized = image.convertToFormat(QImage::Format_ARGB32);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(normalized.width()));
    hash.addData("x");
    hash.addData(QByteArray::number(normalized.height()));
    hash.addData(QByteArrayView(reinterpret_cast<const char*>(normalized.constBits()),
                                normalized.sizeInBytes()));
    return hash.result();
}
}  // namespace

ChickPixImages::ChickPixImages(const QString& configFile) {
    Q_INIT_RESOURCE(resources);
    folders_ = {QDir(QFileInfo(configFile).absolutePath()).filePath("chickens"),
                QDir(QCoreApplication::applicationDirPath()).filePath("chickens")};
    folders_.removeDuplicates();
}

QStringList ChickPixImages::folders() const { return folders_; }

QString ChickPixImages::allowedPath(const QString& selection) const {
    if (bundledImages.contains(selection)) return selection;
    if (!QDir::isAbsolutePath(selection)) return {};
    const QFileInfo candidate(selection);
    const auto canonicalFile = candidate.canonicalFilePath();
    if (!candidate.isFile() || canonicalFile.isEmpty()) return {};
    for (const auto& folder : folders_) {
        const auto canonicalFolder = QFileInfo(folder).canonicalFilePath();
        if (canonicalFolder.isEmpty()) continue;
        // Only direct children are offered by the catalog; resolve links before checking
        // containment.
        if (candidate.absolutePath() != QDir::cleanPath(folder)) continue;
        const auto relative = QDir(canonicalFolder).relativeFilePath(canonicalFile);
        if (relative != ".." && !relative.startsWith("../") && !QDir::isAbsolutePath(relative)) {
            return canonicalFile;
        }
    }
    return {};
}

QImage ChickPixImages::load(const QString& selection) const {
    const auto path = allowedPath(selection);
    if (path.isEmpty()) return {};
    const QFileInfo file(path);
    const auto extension = QFileInfo(selection).suffix().toLower();
    if (!QStringList{"png", "jpg", "jpeg", "webp"}.contains(extension)) return {};
    if (file.size() <= 0 || file.size() > maximumFileBytes) return {};
    QImageReader reader(path);
    const auto size = reader.size();
    const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
    if (!size.isValid() || size.width() > maximumDimension || size.height() > maximumDimension ||
        pixels > maximumPixels) {
        return {};
    }
    if (!reader.canRead()) return {};
    if (!QList<QByteArray>{"png", "jpeg", "webp"}.contains(reader.format().toLower())) return {};
    return reader.read();
}

QStringList ChickPixImages::catalog() const {
    auto candidates = bundledImages;
    for (const auto& folder : folders_) {
        const QDir directory(folder);
        const auto files = directory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const auto& file : files) {
            const auto path = file.absoluteFilePath();
            candidates.append(path);
        }
    }
    QStringList images;
    QSet<QString> filenames;
    QSet<QByteArray> fingerprints;
    for (const auto& path : candidates) {
        const auto filename = QFileInfo(path).fileName().toCaseFolded();
        if (filenames.contains(filename)) continue;
        const auto image = load(path);
        if (image.isNull()) continue;
        const auto fingerprint = imageFingerprint(image);
        if (fingerprints.contains(fingerprint)) continue;
        images.append(path);
        filenames.insert(filename);
        fingerprints.insert(fingerprint);
    }
    return images;
}

QString ChickPixImages::resolveSelection(const QString& selection,
                                         const QStringList& catalog) const {
    if (selection.isEmpty()) return defaultImage();
    if (catalog.contains(selection)) return selection;
    const auto selectedImage = load(selection);
    if (!selectedImage.isNull()) {
        const auto selectedFingerprint = imageFingerprint(selectedImage);
        const auto duplicate = std::ranges::find_if(catalog, [&](const QString& path) {
            return imageFingerprint(load(path)) == selectedFingerprint;
        });
        if (duplicate != catalog.cend()) return *duplicate;
    }
    const auto selectedName = QFileInfo(selection).fileName();
    const auto sameName = std::ranges::find_if(catalog, [&](const QString& path) {
        return QFileInfo(path).fileName().compare(selectedName, Qt::CaseInsensitive) == 0;
    });
    if (sameName != catalog.cend()) return *sameName;
    return selection;
}

QString ChickPixImages::defaultImage() { return bundledImages.first(); }
