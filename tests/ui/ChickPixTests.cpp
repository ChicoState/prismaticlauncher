#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QSize>
#include <QSlider>
#include <QTemporaryDir>
#include <QtTest>

#include "LauncherConfig.h"
#include "LauncherWindow.h"
#include "ChickPixImages.h"
#include "settings.h"

namespace {
/// @brief Creates a distinctive opaque image without using game or user artwork.
bool writeChicken(const QString& path, QColor color = QColor(23, 187, 109),
                  QSize size = QSize(32, 24)) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QImage image(size, QImage::Format_ARGB32);
    image.fill(color);
    return image.save(path, "PNG");
}

/// @brief Opens ChickPix through its settings entry, returning null when the entry is absent.
QDialog* openChickPix(settings& settingsWindow) {
    auto* configure = settingsWindow.findChild<QPushButton*>("configureChickPix");
    if (!configure) return nullptr;
    configure->click();
    return settingsWindow.findChild<QDialog*>("chickPixDialog");
}

/// @brief Compares displayed artwork to the source scaled to its observable display size.
bool displaysImage(const QLabel* label, const QString& path) {
    if (!label || label->pixmap().isNull()) return false;
    const QPixmap expected(path);
    if (expected.isNull()) return false;
    const auto displayed = label->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    const auto scaled =
        expected.scaled(displayed.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return displayed == scaled.toImage().convertToFormat(QImage::Format_ARGB32);
}

const QString defaultImagePath = ":/farmpack/default.png";

/// @brief Restores the working directory after a test temporarily changes it.
class WorkingDirectoryGuard {
   public:
    /// @brief Remembers the original working directory before a fixture changes it.
    WorkingDirectoryGuard() : original_(QDir::currentPath()) {}
    /// @brief Restores the original directory even when a test assertion returns early.
    ~WorkingDirectoryGuard() { QDir::setCurrent(original_); }

   private:
    QString original_;
};
}  // namespace

class ChickPixTests : public QObject {
    Q_OBJECT

   private slots:
    /// @brief Uses the application stylesheet while preventing tests from making live Nexus
    /// requests.
    void initTestCase() {
        qunsetenv("NEXUS_API_KEY");
        qApp->setStyle("Fusion");
        QFile stylesheet(":/ui/style.qss");
        QVERIFY(stylesheet.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    }

    /// @brief Exercises first-show artwork at normal and minimum sizes with persisted visibility.
    void startupRendersSavedArtworkWithinLibrary_data() {
        QTest::addColumn<QSize>("windowSize");
        QTest::addColumn<int>("opacity");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("normal-opaque") << QSize(1120, 760) << 100 << true;
        QTest::newRow("normal-translucent") << QSize(1120, 760) << 43 << true;
        QTest::newRow("minimum-opaque") << QSize(820, 600) << 100 << true;
        QTest::newRow("minimum-translucent") << QSize(820, 600) << 43 << true;
        QTest::newRow("normal-disabled") << QSize(1120, 760) << 43 << false;
        QTest::newRow("minimum-disabled") << QSize(820, 600) << 43 << false;
    }

    /// @brief First show paints saved artwork inside the library without a toggle or later resize.
    void startupRendersSavedArtworkWithinLibrary() {
        QFETCH(QSize, windowSize);
        QFETCH(int, opacity);
        QFETCH(bool, enabled);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = temporary.filePath("chickens/startup.png");
        const QColor green(0, 255, 0);
        QVERIFY(writeChicken(custom, green));
        LauncherConfig config(temporary.filePath("launcher.ini"));
        QVERIFY(config.saveChickPix(custom, opacity, enabled));
        QFile savedConfig(config.filePath());
        QVERIFY(savedConfig.open(QIODevice::ReadOnly));
        const auto savedBytes = savedConfig.readAll();
        savedConfig.close();
        LauncherWindow window(config.filePath());
        window.resize(windowSize);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QApplication::processEvents();
        auto* overlay = window.findChild<QLabel*>("farmPackOverlay");
        auto* toggle = window.findChild<QPushButton*>("farmPackToggle");
        QVERIFY(overlay && toggle);
        QCOMPARE(toggle->isChecked(), enabled);
        QCOMPARE(overlay->isVisible(), enabled);
        const QImage firstFrame = window.grab().toImage();
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            const auto filename = QString("chickpix-startup-%1.png").arg(QTest::currentDataTag());
            QVERIFY(firstFrame.save(QDir(screenshots).filePath(filename)));
        }
        QVERIFY(savedConfig.open(QIODevice::ReadOnly));
        QCOMPARE(savedConfig.readAll(), savedBytes);
        savedConfig.close();
        if (!enabled) return;

        const auto* parent = overlay->parentWidget();
        QVERIFY(parent);
        const auto diagnostic = QString("parent=%1x%2 overlay=(%3,%4 %5x%6)")
                                    .arg(parent->width())
                                    .arg(parent->height())
                                    .arg(overlay->x())
                                    .arg(overlay->y())
                                    .arg(overlay->width())
                                    .arg(overlay->height());
        const bool initiallyContained = parent->rect().contains(overlay->geometry());
        if (!initiallyContained) {
            qInfo() << "First show:" << diagnostic << "visible region" << overlay->visibleRegion();
            toggle->click();
            toggle->click();
            QApplication::processEvents();
            qInfo() << "After off/on: parent" << parent->size() << "overlay" << overlay->geometry()
                    << "visible region" << overlay->visibleRegion();
        }
        QVERIFY2(initiallyContained, qPrintable(diagnostic));
        QCOMPARE(overlay->geometry().right(), parent->rect().right() - 20);
        QCOMPARE(overlay->geometry().bottom(), parent->rect().bottom() - 20);
        const auto sample = overlay->mapTo(&window, overlay->rect().center());
        const auto pixel = sample * firstFrame.devicePixelRatio();
        const QColor actual = firstFrame.pixelColor(pixel);
        const auto firstVisibleRegion = overlay->visibleRegion();
        // Obtain the background only after capturing and checking the untouched first show.
        toggle->click();
        QApplication::processEvents();
        const auto background = window.grab().toImage().pixelColor(pixel);
        const double alpha = opacity / 100.0;
        const QColor expected(qRound(background.red() * (1.0 - alpha)),
                              qRound(255 * alpha + background.green() * (1.0 - alpha)),
                              qRound(background.blue() * (1.0 - alpha)));
        const bool paintedAtSavedOpacity = qAbs(actual.red() - expected.red()) <= 3 &&
                                           qAbs(actual.green() - expected.green()) <= 3 &&
                                           qAbs(actual.blue() - expected.blue()) <= 3;
        if (!paintedAtSavedOpacity) {
            toggle->click();
            QApplication::processEvents();
            qInfo() << "First frame:" << diagnostic << "visible region" << firstVisibleRegion
                    << "pixel" << actual << "expected" << expected << "background" << background;
            qInfo() << "After off/on: overlay" << overlay->geometry() << "visible region"
                    << overlay->visibleRegion() << "pixel"
                    << window.grab().toImage().pixelColor(pixel);
        }
        QVERIFY2(paintedAtSavedOpacity,
                 qPrintable(QString("First-show pixel %1 should be %2 at opacity %3. %4")
                                .arg(actual.name(), expected.name())
                                .arg(opacity)
                                .arg(diagnostic)));
    }

    /// @brief Reopening saved disabled then enabled settings paints only the enabled first frame.
    void startupReopensSavedDisabledThenEnabledWithoutRewritingConfig() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = temporary.filePath("chickens/reopened.png");
        const QColor green(0, 255, 0);
        QVERIFY(writeChicken(custom, green));
        LauncherConfig config(temporary.filePath("launcher.ini"));
        for (const bool enabled : {false, true}) {
            QVERIFY(config.saveChickPix(custom, 100, enabled));
            QFile savedConfig(config.filePath());
            QVERIFY(savedConfig.open(QIODevice::ReadOnly));
            const auto savedBytes = savedConfig.readAll();
            savedConfig.close();
            LauncherWindow reopened(config.filePath());
            reopened.show();
            QVERIFY(QTest::qWaitForWindowExposed(&reopened));
            QApplication::processEvents();
            auto* overlay = reopened.findChild<QLabel*>("farmPackOverlay");
            auto* toggle = reopened.findChild<QPushButton*>("farmPackToggle");
            QVERIFY(overlay && toggle);
            QCOMPARE(toggle->isChecked(), enabled);
            QCOMPARE(overlay->isVisible(), enabled);
            if (enabled) {
                QVERIFY(overlay->parentWidget()->rect().contains(overlay->geometry()));
                const auto frame = reopened.grab().toImage();
                const auto sample = overlay->mapTo(&reopened, overlay->rect().center());
                QCOMPARE(frame.pixelColor(sample * frame.devicePixelRatio()), green);
            }
            QVERIFY(savedConfig.open(QIODevice::ReadOnly));
            QCOMPARE(savedConfig.readAll(), savedBytes);
        }
    }

    /// @brief Places Configure ChickPix directly after Save game path and opens its dialog.
    void configureEntryFollowsSaveGamePathAndOpensDialog() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherConfig config(temporary.filePath("launcher.ini"));
        settings settingsWindow(&config);
        settingsWindow.show();
        auto* savePath = settingsWindow.findChild<QPushButton*>("savePath");
        auto* configure = settingsWindow.findChild<QPushButton*>("configureChickPix");
        QVERIFY(savePath);
        QVERIFY2(configure, "Settings must offer Configure ChickPix beneath Save game path.");
        auto* detect = settingsWindow.findChild<QPushButton*>("detectDirectory");
        QVERIFY(detect);
        QCOMPARE(configure->text(), QString("Configure ChickPix"));
        QCOMPARE(configure->styleSheet(), savePath->styleSheet());
        QCOMPARE(configure->styleSheet(), detect->styleSheet());
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QVERIFY(
                settingsWindow.grab().save(QDir(screenshots).filePath("chickpix-settings.png")));
        }
        bool directlyAfterSave = false;
        for (auto* layout : settingsWindow.findChildren<QLayout*>()) {
            const int saveIndex = layout->indexOf(savePath);
            if (saveIndex >= 0 && saveIndex + 1 < layout->count()) {
                directlyAfterSave = layout->itemAt(saveIndex + 1)->widget() == configure;
                if (directlyAfterSave) break;
            }
        }
        QVERIFY(directlyAfterSave);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        QVERIFY(buttons);
        QVERIFY(buttons->button(QDialogButtonBox::Cancel));
        buttons->button(QDialogButtonBox::Cancel)->click();
    }

    /// @brief Places Add image above Refresh images and imports a selected image into the custom
    /// catalog folder.
    void addImageEntryImportsIntoCustomFolder() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        const auto source = temporary.filePath("source/holiday.png");
        QVERIFY(writeChicken(source, QColor(221, 84, 132)));

        LauncherConfig config(configFile);
        settings settingsWindow(&config);
        settingsWindow.show();
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* addImage = dialog->findChild<QPushButton*>("chickPixAddImage");
        auto* refresh = dialog->findChild<QPushButton*>("chickPixRefresh");
        QVERIFY(addImage && refresh);
        QVERIFY(addImage->geometry().bottom() < refresh->geometry().top());

        ChickPixImages images(configFile);
        const auto imported = images.importImage(source);
        QVERIFY(!imported.isEmpty());
        QVERIFY(QFileInfo::exists(imported));
        QVERIFY(images.catalog().contains(imported));
    }

    /// @brief Rejects corrupt sources and preserves existing files when importing a duplicate name.
    void importingImageRejectsInvalidSourcesAndAvoidsOverwrite() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        const auto source = temporary.filePath("source/holiday.png");
        const auto invalid = temporary.filePath("source/broken.png");
        const auto existing = temporary.filePath("chickens/holiday.png");
        QVERIFY(writeChicken(source, QColor(221, 84, 132)));
        QVERIFY(writeChicken(existing, QColor(14, 55, 220)));
        QVERIFY(QDir().mkpath(QFileInfo(invalid).absolutePath()));
        QFile invalidFile(invalid);
        QVERIFY(invalidFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidFile.write("not an image") > 0);
        invalidFile.close();

        ChickPixImages images(configFile);
        QCOMPARE(images.importImage(invalid), QString());
        const auto imported = images.importImage(source);
        QVERIFY(!imported.isEmpty());
        QVERIFY(imported != existing);
        QCOMPARE(QImage(existing).pixelColor(0, 0), QColor(14, 55, 220));
        QCOMPARE(QFileInfo(imported).fileName(), QString("holiday (2).png"));
    }

    /// @brief Lists bundled and valid custom artwork while rejecting unsafe files.
    void imageChoicesExcludeInvalidOversizedAndEscapingFiles() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = temporary.filePath("chickens/My hen é.png");
        const auto unsupported = temporary.filePath("chickens/unsupported.txt");
        const auto corrupt = temporary.filePath("chickens/corrupt.png");
        const auto oversized = temporary.filePath("chickens/oversized.png");
        const auto tooWide = temporary.filePath("chickens/too-wide.png");
        const auto outside = temporary.filePath("outside.png");
        const auto escaped = temporary.filePath("chickens/escaped.png");
        QVERIFY(writeChicken(custom));
        QVERIFY(writeChicken(unsupported));
        QVERIFY(writeChicken(outside));
        QVERIFY(writeChicken(oversized));
        // A narrow strip exceeds decoder dimension limits while using only 64 KiB of pixels.
        QVERIFY(writeChicken(tooWide, QColor(23, 187, 109), QSize(16384, 1)));
        QFile oversizedFile(oversized);
        QVERIFY(oversizedFile.open(QIODevice::ReadWrite));
        // A sparse file exercises encoded-size limits without allocating a giant image.
        QVERIFY(oversizedFile.resize(1024LL * 1024 * 1024));
        oversizedFile.close();
        QFile corruptFile(corrupt);
        QVERIFY(corruptFile.open(QIODevice::WriteOnly));
        QCOMPARE(corruptFile.write("not an image"), qint64(12));
        corruptFile.close();
#ifdef Q_OS_UNIX
        QVERIFY(QFile::link(outside, escaped));
#endif
        LauncherConfig config(temporary.filePath("launcher.ini"));
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        QVERIFY(images);
        QSet<QString> displayedNames;
        for (int index = 0; index < images->count(); ++index) {
            const auto path = images->itemData(index).toString();
            QVERIFY(!path.isEmpty());
            QCOMPARE(images->itemText(index), QFileInfo(path).fileName());
            QCOMPARE(images->itemData(index, Qt::ToolTipRole).toString(), path);
            const auto name = images->itemText(index).toCaseFolded();
            QVERIFY(!displayedNames.contains(name));
            displayedNames.insert(name);
        }
        for (const auto& bundled :
             {QString(":/farmpack/default.png"), QString(":/farmpack/spring.png"),
              QString(":/farmpack/summer.png")}) {
            QVERIFY2(images->findData(bundled) >= 0, qPrintable(bundled));
        }
        QVERIFY(images->findData(custom) >= 0);
        for (const auto& excluded : {unsupported, corrupt, oversized, tooWide, escaped}) {
            QCOMPARE(images->findData(excluded), -1);
        }
    }

    /// @brief Supplies identical decoded pictures and case-insensitive filename collisions.
    void savedDuplicateSelectsExistingRepresentative_data() {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("same-custom-pixels-different-encoding") << QString("custom-copy");
        QTest::newRow("copy-of-bundled-picture") << QString("bundled-copy");
        QTest::newRow("bundled-filename-with-different-pixels") << QString("bundled-name");
        QTest::newRow("missing-path-with-bundled-filename") << QString("missing-name");
    }

    /// @brief Saved duplicate aliases display the same representative in the library and editor.
    void savedDuplicateSelectsExistingRepresentative() {
        QFETCH(QString, scenario);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        QString representative = defaultImagePath;
        QString alias;
        int expectedChoices = 3;
        if (scenario == "custom-copy") {
            representative = temporary.filePath("chickens/a-first.png");
            alias = temporary.filePath("chickens/z-copy.png");
            QVERIFY(writeChicken(representative));
            QImage copy(representative);
            copy.setText("Description", "Different encoded metadata, identical decoded pixels.");
            QVERIFY(copy.save(alias, "PNG"));
            expectedChoices = 4;
        } else if (scenario == "bundled-copy") {
            alias = temporary.filePath("chickens/copied-default.png");
            QVERIFY(QDir().mkpath(QFileInfo(alias).absolutePath()));
            QImage copy(defaultImagePath);
            QVERIFY(!copy.isNull());
            copy.setText("Description", "Copy of a bundled picture.");
            QVERIFY(copy.save(alias, "PNG"));
        } else if (scenario == "bundled-name") {
            alias = temporary.filePath("chickens/DEFAULT.PNG");
            QVERIFY(writeChicken(alias));
        } else {
            alias = temporary.filePath("missing/default.png");
        }
        {
            QSettings saved(configFile, QSettings::IniFormat);
            saved.setValue("appearance/farm-pack-image", alias);
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        LauncherWindow library(configFile);
        library.show();
        auto* overlay = library.findChild<QLabel*>("farmPackOverlay");
        QVERIFY(overlay && overlay->isVisible());
        QVERIFY2(displaysImage(overlay, representative),
                 "Before Save, library artwork must match the dialog's alias representative.");
        LauncherConfig config(configFile);
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* preview = dialog->findChild<QLabel*>("chickPixPreview");
        auto* save = dialog->findChild<QPushButton*>("chickPixSave");
        QVERIFY(images && preview && save);
        QCOMPARE(images->count(), expectedChoices);
        QCOMPARE(images->findData(alias), -1);
        QCOMPARE(images->currentData().toString(), representative);
        QVERIFY(displaysImage(preview, representative));
        QSet<QString> names;
        for (int index = 0; index < images->count(); ++index) {
            const auto path = images->itemData(index).toString();
            QVERIFY(!path.isEmpty());
            QCOMPARE(images->itemText(index), QFileInfo(path).fileName());
            const auto name = images->itemText(index).toCaseFolded();
            QVERIFY(!names.contains(name));
            names.insert(name);
        }
        QCOMPARE(config.farmPackImage(), alias);
        save->click();
        QCOMPARE(config.farmPackImage(), representative);
    }

    /// @brief Supplies absent and legacy-empty saved image values without migrating them on open.
    void legacyEmptyImageDisplaysDefaultWithoutWriting_data() {
        QTest::addColumn<bool>("hasLegacyKey");
        QTest::newRow("missing-image-key") << false;
        QTest::newRow("legacy-empty-image-key") << true;
    }

    /// @brief Empty legacy settings display Default in the library and dialog without an INI write.
    void legacyEmptyImageDisplaysDefaultWithoutWriting() {
        QFETCH(bool, hasLegacyKey);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        QByteArray originalBytes;
        if (hasLegacyKey) {
            {
                QSettings saved(configFile, QSettings::IniFormat);
                saved.setValue("appearance/farm-pack-image", QString());
                saved.sync();
                QCOMPARE(saved.status(), QSettings::NoError);
            }
            QFile original(configFile);
            QVERIFY(original.open(QIODevice::ReadOnly));
            originalBytes = original.readAll();
        }
        LauncherWindow window(configFile);
        window.show();
        QVERIFY(displaysImage(window.findChild<QLabel*>("farmPackOverlay"), defaultImagePath));
        window.findChild<QPushButton*>("settings")->click();
        auto* settingsWindow = window.findChild<settings*>();
        QVERIFY(settingsWindow);
        auto* dialog = openChickPix(*settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        QVERIFY(images);
        QCOMPARE(images->currentData().toString(), defaultImagePath);
        QCOMPARE(images->findData(QString()), -1);
        QCOMPARE(LauncherConfig(configFile).farmPackImage(), QString());
        dialog->reject();
        if (hasLegacyKey) {
            QFile original(configFile);
            QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(original.readAll(), originalBytes);
        } else {
            QVERIFY(!QFileInfo::exists(configFile));
        }
    }

    /// @brief Ignores chickens in the working directory when configuration lives elsewhere.
    void customDiscoveryDoesNotDependOnWorkingDirectory() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto cwdChicken = temporary.filePath("working/chickens/cwd.png");
        QVERIFY(writeChicken(cwdChicken));
        QVERIFY(QDir().mkpath(temporary.filePath("config")));
        const WorkingDirectoryGuard restoreDirectory;
        QVERIFY(QDir::setCurrent(temporary.filePath("working")));
        LauncherConfig config(temporary.filePath("config/launcher.ini"));
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        QVERIFY(images);
        QCOMPARE(images->findData(cwdChicken), -1);
    }

    /// @brief Refresh reflects added, edited, and deleted custom images while retaining selection.
    void refreshDiscoversNewImageWithoutLosingSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto first = temporary.filePath("chickens/first.png");
        const auto added = temporary.filePath("chickens/added.png");
        QVERIFY(writeChicken(first));
        LauncherConfig config(temporary.filePath("launcher.ini"));
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* refresh = dialog->findChild<QPushButton*>("chickPixRefresh");
        auto* preview = dialog->findChild<QLabel*>("chickPixPreview");
        QVERIFY(images && refresh && preview);
        QVERIFY(images->findData(first) >= 0);
        images->setCurrentIndex(images->findData(first));
        QVERIFY(displaysImage(preview, first));
        QCOMPARE(images->findData(added), -1);
        QVERIFY(writeChicken(added, QColor(193, 40, 123)));
        refresh->click();
        QVERIFY(images->findData(added) >= 0);
        QCOMPARE(images->currentData().toString(), first);
        // Refresh must reread custom pixels even when the filename and dimensions stay the same.
        QVERIFY(writeChicken(first, QColor(40, 70, 210)));
        refresh->click();
        QCOMPARE(images->currentData().toString(), first);
        QVERIFY(displaysImage(preview, first));
        QVERIFY(QFile::remove(added));
        refresh->click();
        QCOMPARE(images->findData(added), -1);
    }

    /// @brief Cancel discards staged image, opacity, and visibility edits without touching the INI.
    void cancelLeavesAllSavedAppearanceValuesUnchanged() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherConfig config(temporary.filePath("launcher.ini"));
        QVERIFY(config.saveFarmPackEnabled(true));
        QVERIFY(config.saveFarmPackOpacityPercent(75));
        {
            QSettings saved(config.filePath(), QSettings::IniFormat);
            saved.setValue("appearance/farm-pack-image", ":/farmpack/default.png");
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        QFile original(config.filePath());
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto originalBytes = original.readAll();
        original.close();
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* opacity = dialog->findChild<QSlider*>("chickPixOpacity");
        auto* enabled = dialog->findChild<QCheckBox*>("chickPixEnabled");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        QVERIFY(images && opacity && enabled && buttons);
        QCOMPARE(images->currentData().toString(), QString(":/farmpack/default.png"));
        images->setCurrentIndex(images->findData(":/farmpack/summer.png"));
        opacity->setValue(19);
        enabled->setChecked(false);
        QCOMPARE(config.farmPackOpacityPercent(), 75);
        QVERIFY(config.farmPackEnabled());
        auto* cancel = buttons->button(QDialogButtonBox::Cancel);
        QVERIFY(cancel);
        cancel->click();
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), originalBytes);
        original.close();
        dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QComboBox*>("chickPixImage")->currentData().toString(),
                 QString(":/farmpack/default.png"));
        QCOMPARE(dialog->findChild<QSlider*>("chickPixOpacity")->value(), 75);
        QVERIFY(dialog->findChild<QCheckBox*>("chickPixEnabled")->isChecked());
    }

    /// @brief Save persists custom artwork and synchronizes the live overlay and quick controls.
    void saveCustomImageUpdatesOverlayAndSurvivesRestart() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = temporary.filePath("chickens/selected.png");
        const auto configFile = temporary.filePath("launcher.ini");
        QVERIFY(writeChicken(custom));
        {
            LauncherWindow window(configFile);
            window.show();
            auto* settingsButton = window.findChild<QPushButton*>("settings");
            QVERIFY(settingsButton);
            settingsButton->click();
            auto* settingsWindow = window.findChild<settings*>();
            QVERIFY(settingsWindow);
            auto* dialog = openChickPix(*settingsWindow);
            QVERIFY(dialog);
            auto* images = dialog->findChild<QComboBox*>("chickPixImage");
            auto* opacity = dialog->findChild<QSlider*>("chickPixOpacity");
            auto* enabled = dialog->findChild<QCheckBox*>("chickPixEnabled");
            auto* save = dialog->findChild<QPushButton*>("chickPixSave");
            QVERIFY(images && opacity && enabled && save);
            QVERIFY(images->findData(custom) >= 0);
            images->setCurrentIndex(images->findData(custom));
            opacity->setValue(37);
            enabled->setChecked(true);
            save->click();
            QSettings saved(configFile, QSettings::IniFormat);
            QCOMPARE(saved.value("appearance/farm-pack-image").toString(), custom);
            QCOMPARE(saved.value("appearance/farm-pack-opacity").toInt(), 37);
            QCOMPARE(saved.value("appearance/farm-pack-enabled").toBool(), true);
            auto* overlay = window.findChild<QLabel*>("farmPackOverlay");
            auto* quickEnabled = window.findChild<QPushButton*>("farmPackToggle");
            QVERIFY(overlay && quickEnabled);
            QVERIFY(!window.findChild<QSlider*>("farmPackOpacity"));
            QVERIFY(!window.findChild<QLabel*>("farmPackOpacityLabel"));
            QVERIFY(overlay->isVisible());
            QVERIFY(displaysImage(overlay, custom));
            QCOMPARE(LauncherConfig(configFile).farmPackOpacityPercent(), 37);
            QVERIFY(quickEnabled->isChecked());
            auto* effect = qobject_cast<QGraphicsOpacityEffect*>(overlay->graphicsEffect());
            QVERIFY(effect);
            QCOMPARE(effect->opacity(), 0.37);
            dialog->close();
            dialog = openChickPix(*settingsWindow);
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QComboBox*>("chickPixImage")->currentData().toString(),
                     custom);
            dialog->close();
            quickEnabled->setChecked(false);
            QVERIFY(!overlay->isVisible());
            dialog = openChickPix(*settingsWindow);
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QSlider*>("chickPixOpacity")->value(), 37);
            QVERIFY(!dialog->findChild<QCheckBox*>("chickPixEnabled")->isChecked());
            dialog->findChild<QSlider*>("chickPixOpacity")->setValue(100);
            dialog->findChild<QCheckBox*>("chickPixEnabled")->setChecked(true);
            dialog->findChild<QPushButton*>("chickPixSave")->click();
            QVERIFY(overlay->isVisible());
            QCOMPARE(LauncherConfig(configFile).farmPackOpacityPercent(), 100);
            QCOMPARE(effect->opacity(), 1.0);
            QVERIFY(quickEnabled->isChecked());
        }
        LauncherWindow restarted(configFile);
        restarted.show();
        auto* overlay = restarted.findChild<QLabel*>("farmPackOverlay");
        QVERIFY(overlay && overlay->isVisible());
        QVERIFY(displaysImage(overlay, custom));
        QCOMPARE(LauncherConfig(configFile).farmPackOpacityPercent(), 100);
        auto* effect = qobject_cast<QGraphicsOpacityEffect*>(overlay->graphicsEffect());
        QVERIFY(effect);
        QCOMPARE(effect->opacity(), 1.0);
    }

    /// @brief Exercises bundled choices and saved visibility states.
    void savedBundledChoiceControlsArtworkAndVisibility_data() {
        QTest::addColumn<QString>("image");
        QTest::addColumn<bool>("enabled");
        QTest::newRow("default") << QString(":/farmpack/default.png") << true;
        QTest::newRow("spring") << QString(":/farmpack/spring.png") << true;
        QTest::newRow("summer") << QString(":/farmpack/summer.png") << true;
        QTest::newRow("disabled") << QString(":/farmpack/default.png") << false;
    }

    /// @brief Saves bundled artwork and immediately applies its visibility choice.
    void savedBundledChoiceControlsArtworkAndVisibility() {
        QFETCH(QString, image);
        QFETCH(bool, enabled);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        // Seed a saved choice so selecting Default must replace a previously saved image.
        {
            QSettings saved(configFile, QSettings::IniFormat);
            saved.setValue("appearance/farm-pack-image", ":/farmpack/summer.png");
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        LauncherWindow window(configFile);
        window.show();
        window.findChild<QPushButton*>("settings")->click();
        auto* settingsWindow = window.findChild<settings*>();
        QVERIFY(settingsWindow);
        auto* dialog = openChickPix(*settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* enabledControl = dialog->findChild<QCheckBox*>("chickPixEnabled");
        auto* save = dialog->findChild<QPushButton*>("chickPixSave");
        QVERIFY(images && enabledControl && save);
        QVERIFY(images->findData(image) >= 0);
        images->setCurrentIndex(images->findData(image));
        enabledControl->setChecked(enabled);
        save->click();
        QSettings saved(configFile, QSettings::IniFormat);
        QCOMPARE(saved.value("appearance/farm-pack-image").toString(), image);
        QCOMPARE(saved.value("appearance/farm-pack-enabled").toBool(), enabled);
        auto* overlay = window.findChild<QLabel*>("farmPackOverlay");
        auto* quickEnabled = window.findChild<QPushButton*>("farmPackToggle");
        QVERIFY(overlay && quickEnabled);
        QCOMPARE(quickEnabled->isChecked(), enabled);
        QCOMPARE(overlay->isVisible(), enabled);
        if (enabled) QVERIFY(displaysImage(overlay, image));
    }

    /// @brief Supplies missing and corrupt saved custom images to the fallback scenario.
    void invalidSavedImageFallsBackToDefault_data() {
        QTest::addColumn<bool>("corrupt");
        QTest::newRow("removed-image") << false;
        QTest::newRow("corrupt-image") << true;
    }

    /// @brief Missing or unreadable saved custom artwork displays Default artwork and explains
    /// why.
    void invalidSavedImageFallsBackToDefault() {
        QFETCH(bool, corrupt);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto custom = temporary.filePath("chickens/vanished.png");
        QVERIFY(writeChicken(custom));
        LauncherConfig config(temporary.filePath("launcher.ini"));
        {
            QSettings saved(config.filePath(), QSettings::IniFormat);
            saved.setValue("appearance/farm-pack-image", custom);
            saved.sync();
            QCOMPARE(saved.status(), QSettings::NoError);
        }
        QVERIFY(QFile::remove(custom));
        if (corrupt) {
            QFile broken(custom);
            QVERIFY(broken.open(QIODevice::WriteOnly));
            QCOMPARE(broken.write("broken"), qint64(6));
        }
        LauncherWindow window(config.filePath());
        window.show();
        auto* overlay = window.findChild<QLabel*>("farmPackOverlay");
        QVERIFY(overlay && overlay->isVisible());
        QVERIFY(displaysImage(overlay, defaultImagePath));
        window.findChild<QPushButton*>("settings")->click();
        auto* settingsWindow = window.findChild<settings*>();
        QVERIFY(settingsWindow);
        auto* dialog = openChickPix(*settingsWindow);
        QVERIFY(dialog);
        auto* status = dialog->findChild<QLabel*>("chickPixStatus");
        auto* preview = dialog->findChild<QLabel*>("chickPixPreview");
        QVERIFY(status && preview);
        QVERIFY(!status->text().trimmed().isEmpty());
        QVERIFY(status->text().contains("default", Qt::CaseInsensitive));
        QVERIFY(displaysImage(preview, defaultImagePath));
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        QVERIFY(images);
        const auto missingIndex = images->findData(custom);
        QVERIFY(missingIndex >= 0);
        QCOMPARE(images->itemText(missingIndex), QFileInfo(custom).fileName());
        QCOMPARE(images->itemData(missingIndex, Qt::ToolTipRole).toString(), custom);
    }

    /// @brief Reports failed persistence visibly and leaves the dialog open for correction.
    void blockedConfigSaveReportsFailureWithoutClosingDialog() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QFile blocker(temporary.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        LauncherConfig config(temporary.filePath("blocked/launcher.ini"));
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* save = dialog->findChild<QPushButton*>("chickPixSave");
        auto* status = dialog->findChild<QLabel*>("chickPixStatus");
        auto* opacity = dialog->findChild<QSlider*>("chickPixOpacity");
        QVERIFY(save && status && opacity);
        opacity->setValue(18);
        save->click();
        QVERIFY(dialog->isVisible());
        QVERIFY(status->text().contains("save", Qt::CaseInsensitive));
        QVERIFY(status->text().contains("could not", Qt::CaseInsensitive) ||
                status->text().contains("failed", Qt::CaseInsensitive));
        QVERIFY(!QFileInfo::exists(config.filePath()));
        QCOMPARE(config.farmPackOpacityPercent(), 75);
    }

    /// @brief Failed INI sync restores all cached defaults and emits no settings success signal.
    void configPathDirectorySaveRestoresCachedValuesWithoutSuccessNotification() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configPath = temporary.filePath("launcher.ini");
        // The parent is writable, so this failure occurs during sync rather than directory
        // creation.
        QVERIFY(QDir().mkpath(configPath));
        LauncherConfig config(configPath);
        QCOMPARE(config.farmPackImage(), QString());
        QCOMPARE(config.farmPackOpacityPercent(), 75);
        QVERIFY(config.farmPackEnabled());
        settings settingsWindow(&config);
        QSignalSpy changed(&settingsWindow, SIGNAL(chickPixChanged()));
        QVERIFY(changed.isValid());
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* opacity = dialog->findChild<QSlider*>("chickPixOpacity");
        auto* enabled = dialog->findChild<QCheckBox*>("chickPixEnabled");
        auto* save = dialog->findChild<QPushButton*>("chickPixSave");
        auto* status = dialog->findChild<QLabel*>("chickPixStatus");
        QVERIFY(images && opacity && enabled && save && status);
        const auto summer = images->findData(":/farmpack/summer.png");
        QVERIFY(summer >= 0);
        images->setCurrentIndex(summer);
        opacity->setValue(18);
        enabled->setChecked(false);
        save->click();
        QCOMPARE(changed.count(), 0);
        QVERIFY(dialog->isVisible());
        QVERIFY(status->text().contains("save", Qt::CaseInsensitive));
        QVERIFY(status->text().contains("could not", Qt::CaseInsensitive) ||
                status->text().contains("failed", Qt::CaseInsensitive));
        QCOMPARE(config.farmPackImage(), QString());
        QCOMPARE(config.farmPackOpacityPercent(), 75);
        QVERIFY(config.farmPackEnabled());
        QVERIFY(QFileInfo(configPath).isDir());
    }

    /// @brief Preserves Default artwork and exposes usable controls at normal and minimum sizes.
    void defaultArtworkHasPreviewAndUsableDialog() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherConfig config(temporary.filePath("launcher.ini"));
        settings settingsWindow(&config);
        auto* dialog = openChickPix(settingsWindow);
        QVERIFY(dialog);
        auto* images = dialog->findChild<QComboBox*>("chickPixImage");
        auto* opacity = dialog->findChild<QSlider*>("chickPixOpacity");
        auto* enabled = dialog->findChild<QCheckBox*>("chickPixEnabled");
        auto* preview = dialog->findChild<QLabel*>("chickPixPreview");
        auto* save = dialog->findChild<QPushButton*>("chickPixSave");
        auto* refresh = dialog->findChild<QPushButton*>("chickPixRefresh");
        QVERIFY(images && opacity && enabled && preview && save && refresh);
        QCOMPARE(images->currentData().toString(), defaultImagePath);
        QCOMPARE(opacity->minimum(), 0);
        QCOMPARE(opacity->maximum(), 100);
        QCOMPARE(opacity->value(), 75);
        QVERIFY(enabled->isChecked());
        QVERIFY(displaysImage(preview, defaultImagePath));
        const auto screenshots = qEnvironmentVariable("PRISMATIC_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshots));
            QVERIFY(dialog->grab().save(QDir(screenshots).filePath("chickpix-normal.png")));
        }
        dialog->resize(dialog->minimumSizeHint().expandedTo(dialog->minimumSize()));
        QApplication::processEvents();
        for (const auto* control : {static_cast<QWidget*>(images), static_cast<QWidget*>(opacity),
                                    static_cast<QWidget*>(enabled), static_cast<QWidget*>(save),
                                    static_cast<QWidget*>(refresh)}) {
            QVERIFY(control->isVisible());
            const QRect bounds(control->mapTo(dialog, QPoint(0, 0)), control->size());
            QVERIFY2(dialog->rect().contains(bounds), qPrintable(control->objectName()));
        }
        if (!screenshots.isEmpty()) {
            QVERIFY(dialog->grab().save(QDir(screenshots).filePath("chickpix-minimum.png")));
        }
        opacity->setValue(0);
        save->click();
        QCOMPARE(config.farmPackOpacityPercent(), 0);
        QSettings saved(config.filePath(), QSettings::IniFormat);
        QCOMPARE(saved.value("appearance/farm-pack-image").toString(), defaultImagePath);
    }
};

QTEST_MAIN(ChickPixTests)
#include "ChickPixTests.moc"
