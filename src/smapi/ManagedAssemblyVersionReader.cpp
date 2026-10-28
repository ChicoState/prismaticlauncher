#include "ManagedAssemblyVersionReader.h"

#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>

namespace {
constexpr qint64 kMaximumAssemblySize = 64 * 1024 * 1024;
constexpr int kMaximumMetadataTable = 44;

void setError(QString* errorMessage, const QString& message) {
    if (errorMessage) *errorMessage = message;
}

bool readU16(const QByteArray& data, qsizetype offset, quint16* value) {
    if (offset < 0 || offset > data.size() - 2) return false;
    const auto* bytes = reinterpret_cast<const uchar*>(data.constData() + offset);
    *value = static_cast<quint16>(bytes[0]) | (static_cast<quint16>(bytes[1]) << 8);
    return true;
}

bool readU32(const QByteArray& data, qsizetype offset, quint32* value) {
    if (offset < 0 || offset > data.size() - 4) return false;
    const auto* bytes = reinterpret_cast<const uchar*>(data.constData() + offset);
    *value = static_cast<quint32>(bytes[0]) | (static_cast<quint32>(bytes[1]) << 8) |
             (static_cast<quint32>(bytes[2]) << 16) | (static_cast<quint32>(bytes[3]) << 24);
    return true;
}

bool readU64(const QByteArray& data, qsizetype offset, quint64* value) {
    quint32 lower = 0;
    quint32 upper = 0;
    if (!readU32(data, offset, &lower) || !readU32(data, offset + 4, &upper)) return false;
    *value = static_cast<quint64>(lower) | (static_cast<quint64>(upper) << 32);
    return true;
}

qsizetype align4(qsizetype value) { return (value + 3) & ~static_cast<qsizetype>(3); }

bool hasRange(const QByteArray& data, qsizetype offset, qsizetype size) {
    return offset >= 0 && size >= 0 && offset <= data.size() && size <= data.size() - offset;
}

std::optional<qsizetype> mapRva(const QByteArray& data, quint32 rva, qsizetype sectionOffset,
                                quint16 sectionCount) {
    for (quint16 index = 0; index < sectionCount; ++index) {
        const auto current = sectionOffset + static_cast<qsizetype>(index) * 40;
        quint32 virtualSize = 0;
        quint32 virtualAddress = 0;
        quint32 rawSize = 0;
        quint32 rawAddress = 0;
        if (!readU32(data, current + 8, &virtualSize) ||
            !readU32(data, current + 12, &virtualAddress) ||
            !readU32(data, current + 16, &rawSize) || !readU32(data, current + 20, &rawAddress)) {
            return std::nullopt;
        }

        const auto mappedSize = std::max(virtualSize, rawSize);
        if (rva < virtualAddress || rva - virtualAddress >= mappedSize) continue;
        const auto fileOffset = static_cast<qsizetype>(rawAddress) + (rva - virtualAddress);
        if (!hasRange(data, fileOffset, 1)) return std::nullopt;
        return fileOffset;
    }
    return std::nullopt;
}

struct StreamInfo {
    quint32 offset = 0;
    quint32 size = 0;
};

std::optional<StreamInfo> readStreamInfo(const QByteArray& data, qsizetype metadataOffset,
                                         qsizetype metadataSize, const QByteArray& wantedName,
                                         QString* errorMessage) {
    quint32 versionLength = 0;
    if (!readU32(data, metadataOffset + 12, &versionLength)) {
        setError(errorMessage, "SMAPI metadata has no version header.");
        return std::nullopt;
    }

    const auto streamHeaderStart = align4(metadataOffset + 16 + versionLength);
    quint16 streamCount = 0;
    if (!hasRange(data, metadataOffset, metadataSize) ||
        !readU16(data, streamHeaderStart + 2, &streamCount)) {
        setError(errorMessage, "SMAPI metadata has an invalid stream header.");
        return std::nullopt;
    }

    auto cursor = streamHeaderStart + 4;
    for (quint16 index = 0; index < streamCount; ++index) {
        quint32 offset = 0;
        quint32 size = 0;
        if (!readU32(data, cursor, &offset) || !readU32(data, cursor + 4, &size)) {
            setError(errorMessage, "SMAPI metadata contains a truncated stream header.");
            return std::nullopt;
        }

        const auto nameStart = cursor + 8;
        const auto nameEnd = data.indexOf('\0', nameStart);
        if (nameEnd < 0 || nameEnd >= metadataOffset + metadataSize) {
            setError(errorMessage, "SMAPI metadata contains an invalid stream name.");
            return std::nullopt;
        }

        if (data.mid(nameStart, nameEnd - nameStart) == wantedName) {
            const auto streamEnd = static_cast<quint64>(offset) + size;
            if (streamEnd > static_cast<quint64>(metadataSize)) {
                setError(errorMessage, "SMAPI metadata stream exceeds its declared size.");
                return std::nullopt;
            }
            return StreamInfo{.offset = offset, .size = size};
        }

        cursor = align4(nameEnd + 1);
    }

    setError(errorMessage, "SMAPI assembly is missing a required metadata stream.");
    return std::nullopt;
}

struct TableContext {
    std::array<quint32, kMaximumMetadataTable + 1> rows{};
    quint8 heapSizes = 0;

    [[nodiscard]] qsizetype stringIndexSize() const { return heapSizes & 1 ? 4 : 2; }
    [[nodiscard]] qsizetype guidIndexSize() const { return heapSizes & 2 ? 4 : 2; }
    [[nodiscard]] qsizetype blobIndexSize() const { return heapSizes & 4 ? 4 : 2; }

    [[nodiscard]] qsizetype tableIndexSize(int table) const {
        return rows.at(table) < 65536 ? 2 : 4;
    }

    [[nodiscard]] qsizetype codedIndexSize(int tagBits, std::initializer_list<int> tables) const {
        quint32 maximumRows = 0;
        for (const auto table : tables) maximumRows = std::max(maximumRows, rows.at(table));
        return maximumRows < (1u << (16 - tagBits)) ? 2 : 4;
    }
};

std::optional<qsizetype> tableRowSize(const TableContext& context, int table) {
    const auto s = context.stringIndexSize();
    const auto g = context.guidIndexSize();
    const auto b = context.blobIndexSize();
    const auto field = context.tableIndexSize(4);
    const auto method = context.tableIndexSize(6);
    const auto param = context.tableIndexSize(8);
    const auto event = context.tableIndexSize(20);
    const auto property = context.tableIndexSize(23);

    switch (table) {
        case 0:
            return 2 + s + 3 * g;
        case 1:
            return context.codedIndexSize(2, {0, 26, 35, 1}) + 2 * s;
        case 2:
            return 4 + 2 * s + context.codedIndexSize(2, {2, 1, 27}) + field + method;
        case 3:
            return field;
        case 4:
            return 2 + s + b;
        case 5:
            return method;
        case 6:
            return 8 + s + b + param;
        case 7:
            return param;
        case 8:
            return 4 + s;
        case 9:
            return context.tableIndexSize(2) + context.codedIndexSize(2, {2, 1, 27});
        case 10:
            return context.codedIndexSize(3, {2, 1, 26, 6, 27}) + s + b;
        case 11:
            return 2 + context.codedIndexSize(2, {4, 8, 23}) + b;
        case 12:
            return context.codedIndexSize(5, {6,  4,  1,  2,  8,  9,  10, 0,  23, 20, 17,
                                              26, 27, 32, 35, 38, 39, 40, 42, 44, 43}) +
                   context.codedIndexSize(3, {6, 10}) + b;
        case 13:
            return context.codedIndexSize(1, {4, 8}) + b;
        case 14:
            return 2 + context.codedIndexSize(2, {2, 6, 32}) + b;
        case 15:
            return 6 + context.tableIndexSize(2);
        case 16:
            return 4 + field;
        case 17:
            return b;
        case 18:
            return context.tableIndexSize(2) + event;
        case 19:
            return event;
        case 20:
            return 2 + s + context.codedIndexSize(2, {2, 1, 27});
        case 21:
            return context.tableIndexSize(2) + property;
        case 22:
            return property;
        case 23:
            return 2 + s + b;
        case 24:
            return 2 + method + context.codedIndexSize(1, {20, 23});
        case 25:
            return context.tableIndexSize(2) + context.codedIndexSize(1, {6, 10}) * 2;
        case 26:
            return s;
        case 27:
            return b;
        case 28:
            return 2 + context.codedIndexSize(1, {4, 6}) + s + context.tableIndexSize(26);
        case 29:
            return 4 + field;
        case 30:
            return 8;
        case 31:
            return 4;
        default:
            return std::nullopt;
    }
}

std::optional<SmapiVersion> readMetadataVersion(const QByteArray& data, qsizetype metadataOffset,
                                                qsizetype metadataSize, QString* errorMessage) {
    quint32 signature = 0;
    if (!readU32(data, metadataOffset, &signature) || signature != 0x424A5342) {
        setError(errorMessage, "SMAPI assembly has invalid CLR metadata.");
        return std::nullopt;
    }

    QString optimizedTablesError;
    auto tables = readStreamInfo(data, metadataOffset, metadataSize, "#~", &optimizedTablesError);
    if (!tables) tables = readStreamInfo(data, metadataOffset, metadataSize, "#-", errorMessage);
    if (!tables) return std::nullopt;

    const auto tablesOffset = metadataOffset + tables->offset;
    if (!hasRange(data, tablesOffset, tables->size)) {
        setError(errorMessage, "SMAPI assembly has invalid metadata table bounds.");
        return std::nullopt;
    }
    const auto tablesEnd = tablesOffset + tables->size;
    quint16 tableHeader = 0;
    quint8 heapSizes = 0;
    quint64 valid = 0;
    if (!readU16(data, tablesOffset + 4, &tableHeader) ||
        !readU64(data, tablesOffset + 8, &valid)) {
        setError(errorMessage, "SMAPI assembly has a truncated metadata table stream.");
        return std::nullopt;
    }
    heapSizes = static_cast<quint8>(tableHeader >> 8);

    TableContext context;
    context.heapSizes = heapSizes;
    auto rowCountOffset = tablesOffset + 24;
    for (int table = 0; table <= kMaximumMetadataTable; ++table) {
        if (valid & (quint64{1} << table)) {
            if (rowCountOffset > tablesEnd - 4 ||
                !readU32(data, rowCountOffset, &context.rows[table])) {
                setError(errorMessage, "SMAPI assembly has truncated metadata row counts.");
                return std::nullopt;
            }
            rowCountOffset += 4;
        }
    }

    if (context.rows[32] == 0) {
        setError(errorMessage, "SMAPI assembly has no Assembly metadata row.");
        return std::nullopt;
    }

    auto assemblyOffset = rowCountOffset;
    for (int table = 0; table < 32; ++table) {
        const auto rowSize = tableRowSize(context, table);
        if (!rowSize) {
            setError(errorMessage, "SMAPI assembly uses an unsupported metadata table.");
            return std::nullopt;
        }
        const auto tableSize = static_cast<quint64>(*rowSize) * context.rows[table];
        if (tableSize > std::numeric_limits<qsizetype>::max() - assemblyOffset) {
            setError(errorMessage, "SMAPI assembly metadata is too large.");
            return std::nullopt;
        }
        assemblyOffset += static_cast<qsizetype>(tableSize);
        if (assemblyOffset > tablesEnd) {
            setError(errorMessage, "SMAPI assembly metadata table exceeds its declared size.");
            return std::nullopt;
        }
    }

    if (assemblyOffset > tablesEnd - 12) {
        setError(errorMessage, "SMAPI assembly has a truncated Assembly metadata row.");
        return std::nullopt;
    }

    quint16 major = 0;
    quint16 minor = 0;
    quint16 patch = 0;
    quint32 hashAlgorithm = 0;
    if (!readU32(data, assemblyOffset, &hashAlgorithm) ||
        !readU16(data, assemblyOffset + 4, &major) || !readU16(data, assemblyOffset + 6, &minor) ||
        !readU16(data, assemblyOffset + 8, &patch)) {
        setError(errorMessage, "SMAPI assembly has a truncated Assembly metadata row.");
        return std::nullopt;
    }
    Q_UNUSED(hashAlgorithm);

    return SmapiVersion{.major = major, .minor = minor, .patch = patch};
}
}  // namespace

std::optional<SmapiVersion> ManagedAssemblyVersionReader::read(const QString& filePath,
                                                               QString* errorMessage) const {
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isFile()) {
        setError(errorMessage, "SMAPI assembly file is missing.");
        return std::nullopt;
    }
    if (fileInfo.size() <= 0 || fileInfo.size() > kMaximumAssemblySize) {
        setError(errorMessage, "SMAPI assembly file has an invalid size.");
        return std::nullopt;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, "SMAPI assembly file could not be read.");
        return std::nullopt;
    }
    const auto data = file.readAll();
    if (data.size() != fileInfo.size()) {
        setError(errorMessage, "SMAPI assembly file could not be read completely.");
        return std::nullopt;
    }

    quint16 dosSignature = 0;
    quint32 peOffset = 0;
    if (!readU16(data, 0, &dosSignature) || dosSignature != 0x5A4D ||
        !readU32(data, 0x3C, &peOffset) || !hasRange(data, peOffset, 4)) {
        setError(errorMessage, "SMAPI assembly is not a valid PE file.");
        return std::nullopt;
    }

    quint32 peSignature = 0;
    quint16 sectionCount = 0;
    quint16 optionalHeaderSize = 0;
    if (!readU32(data, peOffset, &peSignature) || peSignature != 0x00004550 ||
        !readU16(data, peOffset + 6, &sectionCount) ||
        !readU16(data, peOffset + 20, &optionalHeaderSize)) {
        setError(errorMessage, "SMAPI assembly has an invalid PE header.");
        return std::nullopt;
    }

    const auto optionalHeaderOffset = peOffset + 24;
    quint16 optionalHeaderMagic = 0;
    if (!readU16(data, optionalHeaderOffset, &optionalHeaderMagic)) {
        setError(errorMessage, "SMAPI assembly has no optional PE header.");
        return std::nullopt;
    }
    const bool pe32Plus = optionalHeaderMagic == 0x20B;
    if (optionalHeaderMagic != 0x10B && !pe32Plus) {
        setError(errorMessage, "SMAPI assembly uses an unsupported PE format.");
        return std::nullopt;
    }

    const auto dataDirectoryOffset = optionalHeaderOffset + (pe32Plus ? 112 : 96);
    quint32 directoryCount = 0;
    quint32 clrRva = 0;
    if (!readU32(data, optionalHeaderOffset + (pe32Plus ? 108 : 92), &directoryCount) ||
        directoryCount <= 14 || !readU32(data, dataDirectoryOffset + 14 * 8, &clrRva)) {
        setError(errorMessage, "SMAPI assembly has no CLR metadata directory.");
        return std::nullopt;
    }

    const auto sectionOffset = optionalHeaderOffset + optionalHeaderSize;
    const auto cliOffset = mapRva(data, clrRva, sectionOffset, sectionCount);
    if (!cliOffset) {
        setError(errorMessage, "SMAPI assembly has an invalid CLR metadata location.");
        return std::nullopt;
    }

    quint32 metadataRva = 0;
    quint32 metadataSize = 0;
    if (!readU32(data, *cliOffset + 8, &metadataRva) ||
        !readU32(data, *cliOffset + 12, &metadataSize)) {
        setError(errorMessage, "SMAPI assembly has an invalid CLR header.");
        return std::nullopt;
    }

    const auto metadataOffset = mapRva(data, metadataRva, sectionOffset, sectionCount);
    if (!metadataOffset || !hasRange(data, *metadataOffset, metadataSize)) {
        setError(errorMessage, "SMAPI assembly has invalid CLR metadata bounds.");
        return std::nullopt;
    }
    return readMetadataVersion(data, *metadataOffset, metadataSize, errorMessage);
}
