#include <QDate>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "FarmPack.h"

class FarmPackTests : public QObject {
    Q_OBJECT

   private slots:
    void loadsManifestAndResolvesPaths();
    void selectsFirstMatchingSeasonalVariant();
    void supportsRangesThatCrossNewYear();
    void rejectsMissingReferencedImage();
    void rejectsPathTraversal();
    void rejectsMalformedManifest();

   private:
    static QString writeManifest(const QTemporaryDir& directory, const QByteArray& contents);
    static void touch(const QString& path);
};

QString FarmPackTests::writeManifest(const QTemporaryDir& directory,
                                     const QByteArray& contents) {
    const auto manifestPath = directory.filePath("manifest.json");
    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::WriteOnly)) return {};
    manifest.write(contents);
    return manifestPath;
}

void FarmPackTests::touch(const QString& path) {
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
}

void FarmPackTests::loadsManifestAndResolvesPaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    touch(directory.filePath("default.png"));

    const auto manifestPath = writeManifest(
        directory,
        R"({"name":"ChickPix","default":"default.png","variants":[]})");
    const auto result = FarmPack::load(manifestPath);

    QVERIFY2(result.has_value(), qPrintable(result.error));
    QCOMPARE(result.pack.name, QString("ChickPix"));
    QCOMPARE(result.pack.defaultImagePath, directory.filePath("default.png"));
    QCOMPARE(result.pack.imageForDate(QDate(2026, 1, 1)), directory.filePath("default.png"));
}

void FarmPackTests::selectsFirstMatchingSeasonalVariant() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    touch(directory.filePath("default.png"));
    touch(directory.filePath("spring.png"));
    touch(directory.filePath("spring-priority.png"));

    const auto manifestPath = writeManifest(
        directory,
        R"({"name":"ChickPix","default":"default.png","variants":[{"start":"03-01","end":"05-31","image":"spring.png"},{"start":"04-01","end":"04-30","image":"spring-priority.png"}]})");
    const auto result = FarmPack::load(manifestPath);

    QVERIFY2(result.has_value(), qPrintable(result.error));
    QCOMPARE(result.pack.imageForDate(QDate(2026, 4, 15)), directory.filePath("spring.png"));
    QCOMPARE(result.pack.imageForDate(QDate(2026, 6, 1)), directory.filePath("default.png"));
}

void FarmPackTests::supportsRangesThatCrossNewYear() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    touch(directory.filePath("default.png"));
    touch(directory.filePath("new-year.png"));

    const auto manifestPath = writeManifest(
        directory,
        R"({"name":"ChickPix","default":"default.png","variants":[{"start":"12-20","end":"01-05","image":"new-year.png"}]})");
    const auto result = FarmPack::load(manifestPath);

    QVERIFY2(result.has_value(), qPrintable(result.error));
    QCOMPARE(result.pack.imageForDate(QDate(2026, 12, 25)), directory.filePath("new-year.png"));
    QCOMPARE(result.pack.imageForDate(QDate(2027, 1, 5)), directory.filePath("new-year.png"));
    QCOMPARE(result.pack.imageForDate(QDate(2027, 1, 6)), directory.filePath("default.png"));
}

void FarmPackTests::rejectsMissingReferencedImage() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto manifestPath = writeManifest(
        directory,
        R"({"name":"ChickPix","default":"default.png","variants":[]})");

    const auto result = FarmPack::load(manifestPath);

    QVERIFY(!result.has_value());
    QVERIFY(result.error.contains("default"));
}

void FarmPackTests::rejectsPathTraversal() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    touch(directory.filePath("default.png"));
    const auto manifestPath = writeManifest(
        directory,
        R"({"name":"ChickPix","default":"../outside.png","variants":[]})");

    const auto result = FarmPack::load(manifestPath);

    QVERIFY(!result.has_value());
    QVERIFY(result.error.contains("relative"));
}

void FarmPackTests::rejectsMalformedManifest() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto manifestPath = writeManifest(directory, "not json");

    const auto result = FarmPack::load(manifestPath);

    QVERIFY(!result.has_value());
    QVERIFY(!result.error.isEmpty());
}

QTEST_APPLESS_MAIN(FarmPackTests)
#include "FarmPackTests.moc"
