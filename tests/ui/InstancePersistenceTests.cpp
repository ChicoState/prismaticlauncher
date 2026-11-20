#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLockFile>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "../smapi/SmapiFixture.h"
#include "InstanceStore.h"
#include "InstanceWindow.h"
#include "LauncherConfig.h"
#include "LauncherWindow.h"

namespace {
/// @brief Creates a named library instance through the add dialog using its real controls.
void createInstance(LauncherWindow* window, const QString& name, bool modded = false) {
    QTimer::singleShot(0, window, [name, modded] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* nameField = dialog->findChild<QLineEdit*>("name");
        auto* modeField = dialog->findChild<QComboBox*>("mode");
        QVERIFY(nameField);
        QVERIFY(modeField);
        nameField->setText(name);
        modeField->setCurrentIndex(modded ? 1 : 0);
        auto* buttons = dialog->findChild<QDialogButtonBox*>("buttons");
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    auto* button = window->findChild<QPushButton*>("addInstance");
    QVERIFY(button);
    button->click();
}

/// @brief Finds a library card by its saved user-facing name regardless of display ordering.
QListWidgetItem* instanceNamed(QListWidget* instances, const QString& name) {
    for (int row = 0; row < instances->count(); ++row) {
        auto* item = instances->item(row);
        if (item->data(Qt::UserRole).toString() == name) return item;
    }
    return nullptr;
}

/// @brief Replaces a temporary fixture file and reports any incomplete write to the test.
void writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
}

/// @brief Reads all bytes from a fixture file, returning an empty array on open failure.
QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}  // namespace

class InstancePersistenceTests : public QObject {
    Q_OBJECT

   private slots:
    /// @brief Created entries and changed launch modes survive closing and reopening the launcher.
    void createdInstancesAndModesSurviveRestart() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto configFile = temporary.filePath("launcher.ini");
        {
            LauncherWindow window(configFile);
            createInstance(&window, "Persistent farm");
            auto* instances = window.findChild<QListWidget*>("instances");
            QVERIFY(instances);
            QCOMPARE(instances->count(), 2);
            auto* detail = window.findChild<InstanceWindow*>();
            QVERIFY(detail);
            detail->findChild<QComboBox*>("launchMode")->setCurrentIndex(1);
        }

        LauncherWindow reopened(configFile);
        auto* instances = reopened.findChild<QListWidget*>("instances");
        QVERIFY(instances);
        QCOMPARE(instances->count(), 2);
        auto* persisted = instanceNamed(instances, "Persistent farm");
        QVERIFY(persisted);
        QVERIFY(persisted->data(Qt::UserRole + 1).toBool());
        QVERIFY(instanceNamed(instances, "Stardew Valley"));
    }

    /// @brief Spaces and Unicode remain unchanged in the instance directory and library name.
    void directoryUsesExactProvidedName() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString name = QString::fromUtf8("My Cozy Farm — Étoile");
        LauncherWindow window(temporary.filePath("launcher.ini"));
        createInstance(&window, name, true);

        const auto directory = temporary.filePath("launcher.instances/" + name);
        QVERIFY2(QFileInfo(directory).isDir(), qPrintable(directory));
        QVERIFY(QFileInfo(directory + "/instance.json").isFile());
        QVERIFY(QFileInfo(directory + "/Mods").isDir());
        auto* instances = window.findChild<QListWidget*>("instances");
        QVERIFY(instanceNamed(instances, name));
    }

    /// @brief Changing the open instance updates that entry even after selecting another card.
    void modeChangeTargetsOpenInstanceRatherThanSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherWindow window(temporary.filePath("launcher.ini"));
        createInstance(&window, "First farm");
        auto* instances = window.findChild<QListWidget*>("instances");
        QVERIFY(instances);
        auto* edited = instanceNamed(instances, "First farm");
        auto* other = instanceNamed(instances, "Stardew Valley");
        QVERIFY(edited);
        QVERIFY(other);
        instances->setCurrentItem(other);

        auto* detail = window.findChild<InstanceWindow*>();
        QVERIFY(detail);
        detail->findChild<QComboBox*>("launchMode")->setCurrentIndex(1);

        QVERIFY(edited->data(Qt::UserRole + 1).toBool());
        QVERIFY(!other->data(Qt::UserRole + 1).toBool());
    }

    /// @brief Unsafe and colliding names keep the dialog open without changing existing entries.
    void rejectedNamesPreserveInputAndLibrary() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherWindow window(temporary.filePath("launcher.ini"));
        createInstance(&window, "Cozy Farm");
        auto* instances = window.findChild<QListWidget*>("instances");
        QVERIFY(instances);
        const QStringList rejectedNames{"../Escape", "cozy farm",  "CON",       "Trailing.",
                                        " Leading",  "Slash/Name", "Back\\Name"};
        for (const auto& name : rejectedNames) {
            bool inputPreserved = false;
            bool errorShown = false;
            QTimer::singleShot(0, &window, [&inputPreserved, &errorShown, name] {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if (!dialog) return;
                auto* field = dialog->findChild<QLineEdit*>("name");
                auto* buttons = dialog->findChild<QDialogButtonBox*>("buttons");
                if (!field || !buttons) {
                    dialog->reject();
                    return;
                }
                field->setText(name);
                buttons->button(QDialogButtonBox::Ok)->click();
                inputPreserved = dialog->isVisible() && field->text() == name;
                const auto* error = dialog->findChild<QLabel*>("instanceError");
                errorShown = error && !error->text().isEmpty();
                dialog->reject();
            });
            window.findChild<QPushButton*>("addInstance")->click();
            QVERIFY2(inputPreserved, qPrintable(name));
            QVERIFY2(errorShown, qPrintable(name));
            QCOMPARE(instances->count(), 2);
        }
        QVERIFY(!QFileInfo::exists(temporary.filePath("Escape")));
    }

    /// @brief A busy library restores the previous mode in both views and leaves metadata intact.
    void failedModeSaveRestoresDisplayedState() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        LauncherWindow window(temporary.filePath("launcher.ini"));
        createInstance(&window, "Busy Farm");
        auto* instances = window.findChild<QListWidget*>("instances");
        QVERIFY(instances);
        auto* item = instanceNamed(instances, "Busy Farm");
        QVERIFY(item);
        auto* detail = window.findChild<InstanceWindow*>();
        QVERIFY(detail);
        auto* mode = detail->findChild<QComboBox*>("launchMode");
        QVERIFY(mode);
        const auto root = temporary.filePath("launcher.instances");
        const auto metadataFile = root + "/Busy Farm/instance.json";
        const auto before = readFile(metadataFile);
        QVERIFY(!before.isEmpty());
        QLockFile lock(root + "/.instance-store.lock");
        QVERIFY(lock.tryLock(0));

        mode->setCurrentIndex(1);

        QCOMPARE(mode->currentIndex(), 0);
        QVERIFY(!item->data(Qt::UserRole + 1).toBool());
        QCOMPARE(item->text(), QString("Busy Farm\nVanilla"));
        QCOMPARE(readFile(metadataFile), before);
        QVERIFY(window.statusBar()->currentMessage().contains("busy", Qt::CaseInsensitive));
        QVERIFY(detail->statusBar()->currentMessage().contains("busy", Qt::CaseInsensitive));
    }

    /// @brief A new root receives one default instance while an existing empty root stays empty.
    void initializationDoesNotReseedExistingLibraries() {
        QTemporaryDir temporary;
        InstanceStore store(temporary.filePath("instances"));
        QString error;
        QVERIFY2(store.initialize(&error), qPrintable(error));
        QCOMPARE(store.load().size(), 1);
        QVERIFY2(store.initialize(&error), qPrintable(error));
        QCOMPARE(store.load().size(), 1);
        QVERIFY(QDir(temporary.filePath("instances/Stardew Valley")).removeRecursively());
        QVERIFY2(store.initialize(&error), qPrintable(error));
        QVERIFY(store.load().isEmpty());
    }

    /// @brief Unicode-equivalent and case-insensitive names cannot replace an existing folder.
    void portableCollisionsPreserveExistingMetadata() {
        QTemporaryDir temporary;
        InstanceStore store(temporary.filePath("instances"));
        QString error;
        QVERIFY(store.initialize(&error));
        InstanceRecord created;
        const QString originalName = QString::fromUtf8("Café Farm");
        QVERIFY2(store.create(originalName, false, &created, &error), qPrintable(error));
        const auto originalBytes = readFile(created.directory + "/instance.json");
        const QString collidingName =
            QString::fromUtf8("CAFÉ FARM").normalized(QString::NormalizationForm_D);
        QVERIFY(!store.create(collidingName, true, nullptr, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(created.directory + "/instance.json"), originalBytes);
        QCOMPARE(store.load().size(), 2);

        writeFile(temporary.filePath("instances/Reserved"), "keep this file");
        QVERIFY(!store.create("reserved", true, nullptr, &error));
        QCOMPARE(readFile(temporary.filePath("instances/Reserved")), QByteArray("keep this file"));
    }

    /// @brief Corrupt and newer metadata remain intact while healthy instances still load.
    void malformedAndFutureMetadataArePreserved() {
        QTemporaryDir temporary;
        InstanceStore store(temporary.filePath("instances"));
        QString error;
        QVERIFY(store.initialize(&error));
        InstanceRecord malformed;
        InstanceRecord future;
        QVERIFY(store.create("Damaged farm", false, &malformed, &error));
        QVERIFY(store.create("Future farm", false, &future, &error));
        const QByteArray damagedBytes = "{broken json";
        const auto futureBytes =
            QJsonDocument(
                QJsonObject{{"schemaVersion", 999}, {"name", "Future farm"}, {"modded", true}})
                .toJson();
        writeFile(malformed.directory + "/instance.json", damagedBytes);
        writeFile(future.directory + "/instance.json", futureBytes);

        QStringList warnings;
        const auto healthy = store.load(&warnings);
        QCOMPARE(healthy.size(), 1);
        QCOMPARE(healthy.first().name, QString("Stardew Valley"));
        QCOMPARE(warnings.size(), 2);
        QVERIFY(!store.saveMode("Damaged farm", true, &error));
        QVERIFY(!store.saveMode("Future farm", false, &error));
        QCOMPARE(readFile(malformed.directory + "/instance.json"), damagedBytes);
        QCOMPARE(readFile(future.directory + "/instance.json"), futureBytes);
    }

    /// @brief An ordinary file at the library path produces an error without losing its bytes.
    void invalidLibraryRootDoesNotOverwriteExistingFile() {
        QTemporaryDir temporary;
        const auto root = temporary.filePath("instances");
        writeFile(root, "existing user file");
        InstanceStore store(root);
        QString error;
        QVERIFY(!store.initialize(&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(root), QByteArray("existing user file"));
    }

    /// @brief Normalized filesystem spelling preserves the entered name and remains editable.
    void normalizedFolderNamesPreserveMetadataNameAndRemainEditable() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto root = temporary.filePath("instances");
        const QString enteredName = QString::fromUtf8("Étoile");
        const QString folderName = enteredName.normalized(QString::NormalizationForm_D);
        QVERIFY(enteredName != folderName);
        const auto actualDirectory = QDir(root).filePath(folderName);
        QVERIFY(QDir().mkpath(actualDirectory + "/Mods"));
        const auto metadataFile = actualDirectory + "/instance.json";
        writeFile(metadataFile,
                  QJsonDocument(
                      QJsonObject{{"schemaVersion", 1}, {"name", enteredName}, {"modded", false}})
                      .toJson());
        InstanceStore store(root);
        QString error;
        QVERIFY2(store.initialize(&error), qPrintable(error));

        QStringList warnings;
        const auto loaded = store.load(&warnings);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().name, enteredName);
        QCOMPARE(loaded.first().directory, QFileInfo(actualDirectory).canonicalFilePath());
        QVERIFY(!loaded.first().modded);

        QVERIFY2(store.saveMode(enteredName, true, &error), qPrintable(error));
        const auto savedMetadata = QJsonDocument::fromJson(readFile(metadataFile)).object();
        QCOMPARE(savedMetadata.value("name").toString(), enteredName);
        QVERIFY(savedMetadata.value("modded").toBool());
        const auto reopened = InstanceStore(root).load();
        QCOMPARE(reopened.size(), 1);
        QCOMPARE(reopened.first().directory, QFileInfo(actualDirectory).canonicalFilePath());
        QVERIFY(reopened.first().modded);
    }

#ifdef Q_OS_UNIX
    /// @brief A library below a trusted linked parent exposes canonical instance destinations.
    void linkedParentReturnsCanonicalInstanceDirectories() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto realParent = temporary.filePath("Actual data");
        const auto linkedParent = temporary.filePath("Linked data");
        QVERIFY(QDir().mkdir(realParent));
        QVERIFY(QFile::link(realParent, linkedParent));
        InstanceStore store(linkedParent + "/instances");
        QString error;
        QVERIFY2(store.initialize(&error), qPrintable(error));
        InstanceRecord created;
        QVERIFY2(store.create("Alias Farm", false, &created, &error), qPrintable(error));
        const auto realDirectory = realParent + "/instances/Alias Farm";
        const auto canonicalDirectory = QFileInfo(realDirectory).canonicalFilePath();
        QVERIFY(!canonicalDirectory.isEmpty());
        QCOMPARE(created.directory, canonicalDirectory);

        QStringList warnings;
        QString loadedDirectory;
        for (const auto& instance : store.load(&warnings)) {
            if (instance.name == "Alias Farm") loadedDirectory = instance.directory;
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QCOMPARE(loadedDirectory, canonicalDirectory);
        QVERIFY2(store.saveMode("Alias Farm", true, &error), qPrintable(error));
        const auto metadata = QJsonDocument::fromJson(readFile(realDirectory + "/instance.json"));
        QVERIFY(metadata.object().value("modded").toBool());
    }

    /// @brief Linked instance folders and metadata are rejected without modifying their targets.
    void linkedInstancePathsDoNotEscapeLibrary() {
        QTemporaryDir temporary;
        InstanceStore store(temporary.filePath("instances"));
        QString error;
        QVERIFY(store.initialize(&error));
        InstanceRecord original;
        QVERIFY(store.create("Original farm", false, &original, &error));
        const auto originalBytes = readFile(original.directory + "/instance.json");
        QVERIFY(QFile::link(original.directory, temporary.filePath("instances/Linked farm")));
        QVERIFY(!store.saveMode("Linked farm", true, &error));
        QVERIFY(!store.create("linked farm", true, nullptr, &error));

        const auto externalMetadata = temporary.filePath("external.json");
        writeFile(externalMetadata, originalBytes);
        QVERIFY(QFile::remove(original.directory + "/instance.json"));
        QVERIFY(QFile::link(externalMetadata, original.directory + "/instance.json"));
        QVERIFY(!store.saveMode("Original farm", true, &error));
        QCOMPARE(readFile(externalMetadata), originalBytes);
        QStringList warnings;
        QCOMPARE(store.load(&warnings).size(), 1);
        QCOMPARE(warnings.size(), 2);
    }

    /// @brief Defines modded and vanilla launches for exact argument capture.
    void launchUsesOnlySelectedInstancesMods_data() {
        QTest::addColumn<bool>("modded");
        QTest::newRow("modded") << true;
        QTest::newRow("vanilla") << false;
    }

    /// @brief Modded receives its spaced Mods path as one argument while vanilla receives none.
    void launchUsesOnlySelectedInstancesMods() {
        QFETCH(bool, modded);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto game = temporary.filePath("Game installation");
#ifdef Q_OS_MACOS
        const auto executableDirectory = game + "/Contents/MacOS";
#else
        const auto executableDirectory = game;
#endif
        QVERIFY(SmapiTestFixtures::createSmapiFixture(executableDirectory, {4, 5, 2}, {4, 5, 2}));
        const QByteArray script =
            "#!/bin/sh\nprintf '%s\\n' \"$#\" > launch-args.txt\n"
            "for argument in \"$@\"; do printf '%s\\n' \"$argument\" >> launch-args.txt; done\n";
        const auto executable =
            executableDirectory + (modded ? "/StardewModdingAPI" : "/StardewValley");
        writeFile(executable, script);
        QVERIFY(QFile::setPermissions(
            executable, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        LauncherConfig config(temporary.filePath("launcher.ini"));
        QVERIFY(config.saveGameDirectory(game));
        LauncherWindow window(config.filePath());
        createInstance(&window, "My Spaced Farm", modded);
        auto* detail = window.findChild<InstanceWindow*>();
        QVERIFY(detail);
        detail->findChild<QPushButton*>("playDetailInstance")->click();
        const auto argumentFile = game + "/launch-args.txt";
        const auto expected =
            modded
                ? QByteArray("3\n--developer-mode\n--mods-path\n") +
                      temporary.filePath("launcher.instances/My Spaced Farm/Mods").toUtf8() + "\n"
                : QByteArray("0\n");
        QTRY_COMPARE_WITH_TIMEOUT(readFile(argumentFile), expected, 5000);
    }
#endif
};

QTEST_MAIN(InstancePersistenceTests)
#include "InstancePersistenceTests.moc"
