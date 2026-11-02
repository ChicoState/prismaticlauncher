#pragma once

#include <QDir>
#include <QFile>
#include <algorithm>

#include "SmapiVersion.h"

namespace SmapiTestFixtures {
/// @brief Writes a little-endian 16-bit value into a managed assembly fixture.
inline void put16(QByteArray* data, qsizetype offset, quint16 value) {
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
}

/// @brief Writes a little-endian 32-bit value into a managed assembly fixture.
inline void put32(QByteArray* data, qsizetype offset, quint32 value) {
    put16(data, offset, static_cast<quint16>(value));
    put16(data, offset + 2, static_cast<quint16>(value >> 16));
}

/// @brief Writes a little-endian 64-bit value into a managed assembly fixture.
inline void put64(QByteArray* data, qsizetype offset, quint64 value) {
    put32(data, offset, static_cast<quint32>(value));
    put32(data, offset + 4, static_cast<quint32>(value >> 32));
}

/// @brief Copies a metadata name into the fixture at the requested offset.
inline void putName(QByteArray* data, qsizetype offset, const QByteArray& name) {
    std::copy(name.cbegin(), name.cend(), data->begin() + offset);
}

/// @brief Builds a minimal managed assembly reporting the requested version.
inline QByteArray managedAssemblyFixture(int major, int minor, int patch) {
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

/// @brief Writes fixture bytes and returns false if opening or writing fails.
inline bool writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

/// @brief Creates an empty fixture file and reports whether writing succeeded.
inline bool writeEmptyFile(const QString& path) { return writeFile(path, {}); }

/// @brief Marks a fixture executable on Unix platforms.
inline void makeExecutable(const QString& path) {
#ifndef Q_OS_WIN
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                    QFileDevice::ExeOther);
#else
    Q_UNUSED(path);
#endif
}

/// @brief Creates detectable SMAPI files and returns false if any write fails.
inline bool createSmapiFixture(const QString& gameDirectory, const SmapiVersion& coreVersion,
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
}  // namespace SmapiTestFixtures
