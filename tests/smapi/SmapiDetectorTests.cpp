#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

#include "ManagedAssemblyVersionReader.h"
#include "SmapiDetector.h"

namespace {
void put16(QByteArray* data, qsizetype offset, quint16 value) {
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
}

void put32(QByteArray* data, qsizetype offset, quint32 value) {
    put16(data, offset, static_cast<quint16>(value));
    put16(data, offset + 2, static_cast<quint16>(value >> 16));
}

void put64(QByteArray* data, qsizetype offset, quint64 value) {
    put32(data, offset, static_cast<quint32>(value));
    put32(data, offset + 4, static_cast<quint32>(value >> 32));
}

void putName(QByteArray* data, qsizetype offset, const QByteArray& name) {
    std::copy(name.cbegin(), name.cend(), data->begin() + offset);
}

QByteArray managedAssemblyFixture(int major, int minor, int patch) {
    constexpr qsizetype peOffset = 0x80;
    constexpr qsizetype optionalHeaderOffset = peOffset + 24;
    constexpr qsizetype sectionOffset = optionalHeaderOffset + 0xe0;
    constexpr qsizetype cliOffset = 0x300;
    constexpr qsizetype metadataOffset = 0x400;
    constexpr qsizetype tablesOffset = metadataOffset + 0x80;
    QByteArray data(0x1400, '\0');

    put16(&data, 0x00, 0x5a4d);
    put32(&data, 0x3c, peOffset);
    put32(&data, peOffset, 0x00004550);
    put16(&data, peOffset + 6, 1);
    put16(&data, peOffset + 20, 0xe0);
    put16(&data, optionalHeaderOffset, 0x10b);
    put32(&data, optionalHeaderOffset + 92, 16);
    put32(&data, optionalHeaderOffset + 96 + 14 * 8, 0x1100);
    put32(&data, optionalHeaderOffset + 96 + 14 * 8 + 4, 0x48);

    putName(&data, sectionOffset, ".text");
    put32(&data, sectionOffset + 8, 0x1000);
    put32(&data, sectionOffset + 12, 0x1000);
    put32(&data, sectionOffset + 16, 0x1000);
    put32(&data, sectionOffset + 20, 0x200);

    put32(&data, cliOffset, 0x48);
    put32(&data, cliOffset + 8, 0x1200);
    put32(&data, cliOffset + 12, 0x200);

    put32(&data, metadataOffset, 0x424a5342);
    put16(&data, metadataOffset + 4, 1);
    put16(&data, metadataOffset + 6, 1);
    put32(&data, metadataOffset + 12, 8);
    putName(&data, metadataOffset + 16, "v4.0");
    put16(&data, metadataOffset + 24 + 0, 0);
    put16(&data, metadataOffset + 24 + 2, 2);

    const auto firstStream = metadataOffset + 28;
    put32(&data, firstStream, 0x80);
    put32(&data, firstStream + 4, 0x40);
    putName(&data, firstStream + 8, "#~");
    const auto secondStream = metadataOffset + 40;
    put32(&data, secondStream, 0xc0);
    put32(&data, secondStream + 4, 1);
    putName(&data, secondStream + 8, "#Strings");

    put32(&data, tablesOffset, 0);
    data[tablesOffset + 4] = 2;
    data[tablesOffset + 5] = 0;
    data[tablesOffset + 6] = 0;
    data[tablesOffset + 7] = 1;
    put64(&data, tablesOffset + 8, quint64{1} << 32);
    put64(&data, tablesOffset + 16, 0);
    put32(&data, tablesOffset + 24, 1);
    put16(&data, tablesOffset + 28 + 4, static_cast<quint16>(major));
    put16(&data, tablesOffset + 28 + 6, static_cast<quint16>(minor));
    put16(&data, tablesOffset + 28 + 8, static_cast<quint16>(patch));
    return data;
}

bool writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

bool writeEmptyFile(const QString& path) { return writeFile(path, {}); }

void makeExecutable(const QString& path) {
#ifndef Q_OS_WIN
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                    QFileDevice::ExeOther);
#else
    Q_UNUSED(path);
#endif
}

bool createSmapiFixture(const QString& gameDirectory, const SmapiVersion& coreVersion,
                        const SmapiVersion& interfacesVersion,
                        const QByteArray& metadata = "{\"Format\":\"4.0.0\"}") {
    const QDir game(gameDirectory);
    if (!game.mkpath("smapi-internal")) return false;
#ifdef Q_OS_WIN
    const auto executable = game.filePath("StardewModdingAPI.exe");
#else
    const auto executable = game.filePath("StardewModdingAPI");
#endif
    return writeEmptyFile(executable) && (makeExecutable(executable), true) &&
           writeEmptyFile(game.filePath("StardewModdingAPI.dll")) &&
           writeEmptyFile(game.filePath("StardewModdingAPI.deps.json")) &&
           writeEmptyFile(game.filePath("StardewModdingAPI.runtimeconfig.json")) &&
           writeFile(game.filePath("smapi-internal/metadata.json"), metadata) &&
           writeFile(
               game.filePath("smapi-internal/SMAPI.Toolkit.dll"),
               managedAssemblyFixture(coreVersion.major, coreVersion.minor, coreVersion.patch)) &&
           writeFile(game.filePath("smapi-internal/SMAPI.Toolkit.CoreInterfaces.dll"),
                     managedAssemblyFixture(interfacesVersion.major, interfacesVersion.minor,
                                            interfacesVersion.patch));
}
}  // namespace

class SmapiDetectorTests : public QObject {
    Q_OBJECT

   private slots:
    void readsManagedAssemblyVersion() {
        QTemporaryDir temp;
        const auto path = temp.filePath("SMAPI.Toolkit.dll");
        QVERIFY(writeFile(path, managedAssemblyFixture(4, 5, 2)));

        QString error;
        const auto version = ManagedAssemblyVersionReader().read(path, &error);
        QVERIFY2(version.has_value(), qPrintable(error));
        QCOMPARE(version->toString(), QString("4.5.2"));
    }

    void rejectsNonAssemblyFiles() {
        QTemporaryDir temp;
        const auto path = temp.filePath("not-an-assembly.dll");
        QVERIFY(writeFile(path, "not a PE file"));

        QString error;
        QVERIFY(!ManagedAssemblyVersionReader().read(path, &error));
        QVERIFY(!error.isEmpty());
    }

    void detectsInstalledSmapiAndVersion() {
        QTemporaryDir temp;
        const SmapiVersion expected{4, 5, 2};
        QVERIFY(createSmapiFixture(temp.path(), expected, expected));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Installed);
        QVERIFY(installation.isInstalled());
        QVERIFY(installation.version.has_value());
        QCOMPARE(installation.version->toString(), QString("4.5.2"));
    }

    void reportsNotInstalledForPlainDirectory() {
        QTemporaryDir temp;

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::NotInstalled);
        QVERIFY(!installation.isInstalled());
    }

    void ignoresLegacyRootMetadataMarker() {
        QTemporaryDir temp;
        QVERIFY(writeFile(QDir(temp.path()).filePath("StardewModdingAPI.metadata.json"), "{}"));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::NotInstalled);
    }

    void reportsInvalidGamePath() {
        QTemporaryDir temp;
        const auto missingPath = QDir(temp.path()).filePath("missing-game");

        const auto installation = SmapiDetector().inspect(missingPath);
        QCOMPARE(installation.state, SmapiState::InvalidGamePath);
    }

    void reportsIncompleteInstallation() {
        QTemporaryDir temp;
        QVERIFY(writeEmptyFile(QDir(temp.path()).filePath("StardewModdingAPI.dll")));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Incomplete);
        QVERIFY(installation.diagnostic.contains("smapi-internal/"));
    }

    void reportsMismatchedComponentVersions() {
        QTemporaryDir temp;
        QVERIFY(createSmapiFixture(temp.path(), {4, 5, 2}, {4, 5, 1}));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::ComponentMismatch);
        QVERIFY(installation.diagnostic.contains("4.5.2"));
        QVERIFY(installation.diagnostic.contains("4.5.1"));
    }

    void rejectsInvalidMetadata() {
        QTemporaryDir temp;
        QVERIFY(createSmapiFixture(temp.path(), {4, 5, 2}, {4, 5, 2}, "not json"));

        const auto installation = SmapiDetector().inspect(temp.path());
        QCOMPARE(installation.state, SmapiState::Incomplete);
        QVERIFY(installation.diagnostic.contains("valid JSON"));
    }
};

QTEST_MAIN(SmapiDetectorTests)
#include "SmapiDetectorTests.moc"
