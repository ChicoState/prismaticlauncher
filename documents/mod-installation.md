# Local mod installation

## Objective and scope

Install manually downloaded Stardew Valley SMAPI mods and content packs into the
single configured game's `Mods` folder. All library entries share this location.
The existing Nexus browser continues opening the author's Files page. There is no
automatic download, dependency installation, update/replacement, or instance storage.

## User experience and acceptance criteria

- The Mods page offers Installed and Nexus views. Installed opens without a Nexus key.
- Drop one or more folders, ZIP/7z/RAR archives, or files; keyboard users can use
  file/folder pickers. A file within an extracted mod resolves to its nearest
  enclosing mod folder. A loose DLL, JSON, or legacy XNB without a manifest is
  rejected with actionable guidance instead of guessing its destination.
- Discover `manifest.json` recursively in download wrappers and multi-mod bundles.
  Install complete mod folders, preserving assets and relative paths; ignore wrapper
  documentation. A root-level archive manifest gets a folder named by its UniqueID.
- Validate manifest identity and entry DLL/content-pack metadata. Required dependencies
  and SMAPI remain the player's responsibility and are explained in the UI.
  Match property names without regard to capitalization, including `UniqueId` in
  SMAPI's bundled mods and IDs inside `ContentPackFor`.
- Show installed names, versions and IDs from disk; refresh after installation,
  game-path changes, and on request. Show destination, busy state and persistent results.
- Reject an existing UniqueID or folder collision without overwriting files. Validate
  the entire drop before publishing anything. On a publish failure, roll back folders
  created by this operation and report any rollback failure.
- Do file work outside the UI thread. Keep source downloads intact.

## Implementation and trust boundaries

`src/mods/ModInstaller.*` provides synchronous filesystem operations independent of
widgets; `src/qt/ModInstallPage.*` runs them on a worker thread. Qt Test fixtures in
`tests/mods/` exercise real temporary game/mod trees and generated archives; widget
tests exercise drag/drop, status, refresh, and shared path behavior.

Untrusted manifests, archive entries and folders cross into a writable game directory.
Use libarchive through CMake's `LibArchive::LibArchive`, declared in vcpkg and the
Linux Docker prerequisites, to inspect and stream archives without invoking a shell.
Only ordinary files/directories are accepted. Reject traversal, absolute paths,
portable-name collisions, links and special files. Apply a 100,000-entry, 2 GiB
total data, 512 MiB per-file, 1 MiB manifest and 64-level depth limit per operation.
Stage under a hidden temporary directory beside `Mods` for same-filesystem renames;
never extract directly into `Mods` or overwrite game content. Reject a symlinked
`Mods` destination. Source directories must not overlap the game directory.
This protects against malicious downloads, not a concurrent local process altering
the filesystem during installation. Quit the game before installing mods.

Use existing C++20/Qt conventions, for example
`ModInstaller::install(const QStringList& sources, const QString& gameDirectory)`;
keep archive and filesystem policy in the service, not drop event handlers.

## Work and validation

1. Add fixture tests and installer contract, verify failures, then implement staging,
   validation, duplicate detection and installation. Run `mod-installer-tests`.
2. Add the asynchronous local UI and connect it to the shared configuration. Exercise
   drops and error states with `mod-install-ui-tests`, then run the existing UI suite.
3. Update prerequisites and user documentation; inspect a rendered Qt screenshot.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./scripts/verify-infrastructure.sh
./scripts/format-check.sh
./scripts/static-analysis.sh
git diff --check
```

Always validate inputs and run the relevant checks; ask before destructive updates
or expanding into game-file replacement; never execute a mod or modify its source.

## Research and rationale

- [Nexus Content Patcher](https://www.nexusmods.com/stardewvalley/mods/1915?tab=description)
  links manual downloads and instructs installation through SMAPI and the Mods folder.
- [Stardew player guide](https://stardewvalleywiki.com/Modding:Player_Guide/Getting_Started)
  describes unpacking into individual subfolders, nested download wrappers, and
  keeping downloads outside Mods. macOS uses the game `Contents/MacOS` directory.
- [Manifest reference](https://stardewvalleywiki.com/Modding:Modder_Guide/APIs/Manifest)
  specifies UniqueID, metadata, EntryDll or ContentPackFor; every SMAPI mod/content
  pack needs a manifest. This is why an arbitrary standalone DLL cannot be installed
  reliably and why content files must travel with their enclosing mod.
- [libarchive examples](https://github.com/libarchive/libarchive/wiki/Examples)
  document streaming archive reads. The application owns extraction paths and limits.
- [Qt drop events](https://doc.qt.io/qt-6/qdropevent.html) document local URL handling
  and explicit copy-action acceptance.
