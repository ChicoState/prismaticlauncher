#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

#include "nexus/NexusHttpTransport.h"
#include "nexus/NexusModDownloader.h"

namespace {
constexpr int ExitUsage = 2;
constexpr int ExitFailure = 1;

std::optional<qint64> positiveNumber(const QString& value) {
    bool isNumber = false;
    const qint64 number = value.toLongLong(&isNumber);
    if (!isNumber || number <= 0) {
        return std::nullopt;
    }
    return number;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName("prismatic-nexus-download");
    QCoreApplication::setApplicationVersion("0.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Download one Nexus Mods archive safely.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption gameOption("game", "Nexus game domain.", "domain");
    const QCommandLineOption modOption("mod", "Nexus mod ID.", "id");
    const QCommandLineOption fileOption("file", "Nexus file ID.", "id");
    const QCommandLineOption outputOption("output", "Destination archive path.", "path");
    const QCommandLineOption downloadKeyOption("download-key",
                                               "Nexus download key from an nxm link.", "key");
    const QCommandLineOption expiryOption("expires", "Unix expiry for --download-key.", "seconds");
    const QCommandLineOption maximumBytesOption("max-bytes", "Maximum archive size in bytes.",
                                                "bytes", "2147483648");
    parser.addOptions({gameOption, modOption, fileOption, outputOption, downloadKeyOption, expiryOption,
                       maximumBytesOption});
    parser.process(application);

    if (!parser.isSet(gameOption) || !parser.isSet(modOption) || !parser.isSet(fileOption) ||
        !parser.isSet(outputOption) || parser.isSet(downloadKeyOption) != parser.isSet(expiryOption)) {
        parser.showHelp(ExitUsage);
    }

    const std::optional<qint64> modId = positiveNumber(parser.value(modOption));
    const std::optional<qint64> fileId = positiveNumber(parser.value(fileOption));
    const std::optional<qint64> maximumBytes = positiveNumber(parser.value(maximumBytesOption));
    if (!modId.has_value() || !fileId.has_value() || !maximumBytes.has_value()) {
        QTextStream(stderr) << "--mod, --file, and --max-bytes must be positive integers.\n";
        return ExitUsage;
    }

    NexusDownloadRequest request{
        .gameDomain = parser.value(gameOption),
        .modId = *modId,
        .fileId = *fileId,
        .apiKey = qEnvironmentVariable("NEXUSMODS_API_KEY"),
        .destinationFile = parser.value(outputOption),
        .maximumBytes = *maximumBytes,
    };
    if (parser.isSet(downloadKeyOption)) {
        const std::optional<qint64> expiry = positiveNumber(parser.value(expiryOption));
        if (!expiry.has_value()) {
            QTextStream(stderr) << "--expires must be a positive Unix timestamp.\n";
            return ExitUsage;
        }
        request.downloadKey = parser.value(downloadKeyOption);
        request.downloadKeyExpiresUnixSeconds = *expiry;
    }

    QtNexusHttpTransport transport;
    NexusModDownloader downloader(transport);
    const NexusDownloadResult result = downloader.download(request);
    if (!result.succeeded()) {
        QTextStream(stderr) << result.message << '\n';
        return ExitFailure;
    }
    QTextStream(stdout) << result.message << " Bytes written: " << result.bytesWritten << '\n';
    return 0;
}
