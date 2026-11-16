# Prismatic Launcher

Prismatic Launcher is a C++20/Qt 6 desktop launcher for Stardew Valley. Its Qt Widgets UI provides an instance library with Add/Play/Edit and a separate window with Mods, Logs, Settings, and Modpacks. Mods supports local drag-and-drop installation into one shared game directory and browsing Nexus Mods. Automatic game discovery, SQLite data access, and release packaging are not connected yet.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — C++20 build and CTest harness configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — Qt dependency manifest and immutable vcpkg baseline.
- `scripts/` — Linux CI, quality, and infrastructure-smoke scripts.
- `tests/ui/` — offscreen Qt Test coverage for config, search, navigation, SMAPI status, and launch errors.
- `tests/smapi/` — fixture-based SMAPI installation and managed-assembly version detection tests.
- `src/qt/` — launcher UI, stylesheet, entrypoint, and embedded default config.
- `src/mods/`, `tests/mods/` — staged mod installation, archive validation, and filesystem tests.
- `documents/mod-installation.md` — installation design, boundaries, and research sources.
- `Dockerfile.ci` — non-root Ubuntu Linux build/test image; it is not a production image.
- `.github/workflows/` — pull-request validation and guarded release workflow definitions.
- `documents/infrastructure_plan.md` — approved infrastructure decisions.
- `.agents/skills/` — repository-specific agent skills.
- `src/` — product source belongs here.
- `packaging/` — not created yet; future product-owned portable-archive packaging belongs here.

## Getting Started

### 1. Install host prerequisites

Install the following on every development workstation:

1. [Git](https://git-scm.com/downloads), a supported stable release.
2. [CMake](https://cmake.org/download/) 3.28 or newer.
3. [Ninja](https://ninja-build.org/) and a C++20-capable native compiler: MSVC Build Tools on Windows, Xcode Command Line Tools on macOS, or GCC/Clang on Linux.
4. [Qt 6.8 LTS](https://www.qt.io/download-qt-installer) for native UI development.
5. [vcpkg](https://github.com/microsoft/vcpkg), bootstrapped locally. The repository pins its dependency registry baseline in `vcpkg-configuration.json`.
6. [Docker Desktop](https://www.docker.com/products/docker-desktop/) or Docker Engine if you will run the repeatable Linux validation environment.

Native release maintainers also need their platform packaging tools. macOS distribution requires an Apple Developer account, signing certificate, and notarization credentials. Windows signing requires a code-signing certificate if the project elects to sign Windows archives.

### 2. Configure the native toolchain

For automatic dependency installation, set `VCPKG_ROOT` to your bootstrapped vcpkg checkout and use the `dev-vcpkg` preset. During configuration, vcpkg installs the manifest dependencies (including libarchive, nlohmann-json, and Qt) using the pinned registry baseline. CMake supplies libarchive headers and linkage to the mod installer through `LibArchive::LibArchive`; no separate libarchive installation is needed. The first configure requires network access and may take time to build Qt.

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset dev-vcpkg
cmake --build --preset dev-vcpkg
ctest --preset dev-vcpkg
./build/dev-vcpkg/bin/prismatic-launch
```

This preset uses a separate build directory so it can coexist with the system-package build below.

With Qt, nlohmann_json, and libarchive development files available on CMake's search path, configure, build, test, and run:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
./build/dev/bin/prismatic-launch
```

To make a live, read-only request to Nexus Mods, supply an API key only through the process environment. The probe fetches the ten most recently updated mods, then prints each mod's detailed metadata one at a time. It never prints the key:

```bash
NEXUS_API_KEY='your-api-key' ./build/dev/nexus-fetch-list
```

To browse mods in the desktop UI, launch `./build/dev/bin/prismatic-launch`
with `NEXUS_API_KEY` set in its environment, then click **Browse Nexus mods** on the
main library page. Select a name in the left-hand list to see the author, version,
endorsements, summary, and full details on the right. **Open download page** opens
the selected mod's Files tab on Nexus Mods; it does not install anything. The list
loads 50 mods per page, sorted by latest update. Use **Previous** and **Next** to
browse more, with a result counter showing the total. The filter searches only
the current page. **Refresh** returns to the first page;
**Back to library** returns to your instances. Requests run asynchronously
and the page reports loading, empty results, and errors. The key is kept in memory
only; after changing it, restart the launcher. This view does not install mods.

The CTest suite is network-free and validates list/detail response parsing and list filtering. No `.env` file or local service is needed: the planned app is self-contained and will use local SQLite. Do not add secrets, certificates, or game data to the repository.
For an SDK installed outside system paths, pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64` to the configure command (use your platform's actual SDK directory). To resolve Qt through the pinned vcpkg manifest instead, pass `-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`. CMake links Qt Widgets and Qt Test using the [Qt CMake integration](https://doc.qt.io/qt-6/cmake-get-started.html).

On Ubuntu install `qt6-base-dev`, `nlohmann-json3-dev`, and `libarchive-dev` alongside CMake 3.28+ and a C++ compiler. On macOS libarchive is also available through Homebrew (`brew install libarchive`; pass its prefix as `LibArchive_ROOT` if needed). The vcpkg manifest supplies libarchive on all platforms. With Make installed, Ninja is optional with this alternate build:

```bash
cmake -S . -B build/local -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/local -j
ctest --test-dir build/local --output-on-failure
./build/local/bin/prismatic-launch
```

Windows builds produce `prismatic-launch.exe` in the build's `bin` directory; macOS builds produce `bin/prismatic-launch.app`. Development executables still require the Qt runtime.

### Game path and UI scope

`src/qt/config/defaults.ini` supplies `~/.local/share/Steam/steamapps/common/Stardew Valley`, the conventional Ubuntu Steam location. The launcher expands `~` for the current user. In Settings, **Auto Detect Directory** checks the current path first, then common Steam locations on Windows, macOS, and Linux (including Flatpak and Snap), additional libraries from Steam's `libraryfolders.vdf`, and standard GOG locations. It accepts a folder only when the platform's game executable is present. On macOS it selects the bundle root, with the executable under `Contents/MacOS`. The common installation locations follow [SMAPI's game-path discovery](https://github.com/Pathoschild/SMAPI/blob/develop/build/find-game-folder.targets).

Detection fills the path field; **Save game path** persists it. If detection finds nothing, it keeps your current entry and suggests browsing manually. Custom installations outside the supported locations can still be selected with Browse. Use the library's **SMAPI** button to check SMAPI status for the saved game directory in the separate SMAPI window.

The library’s Settings button opens the separate launcher settings window (`settings.ui`). Browse selects a directory; Save game path writes a per-user INI override using [Qt's settings API](https://doc.qt.io/qt-6/qsettings.html). On Ubuntu the default is `~/.config/Prismatic/Prismatic Launcher/launcher.ini` (or under `XDG_CONFIG_HOME` when set). The UI shows the exact path. Use `--config /absolute/path/launcher.ini` for an alternate configuration file:

Edit Instance → Settings → Save game path writes a per-user INI override using [Qt's settings API](https://doc.qt.io/qt-6/qsettings.html). On Ubuntu the default is `~/.config/Prismatic/Prismatic Launcher/launcher.ini` (or under `XDG_CONFIG_HOME` when set). The UI shows the exact path. Use `--config /absolute/path/launcher.ini` for an alternate configuration file:

```ini
[game]
directory=/home/your-user/.local/share/Steam/steamapps/common/Stardew Valley
```

Play validates and starts the selected game's executable from that folder. Vanilla uses `StardewValley` on Ubuntu; Modded uses an already-installed `StardewModdingAPI`. Edit Instance, double-clicking an instance, and Add Instance open a separate instance window; its Settings tab shares the configured game path. Add Instance creates session-only UI entries. Modes and entries reset on restart; the game path persists. Mods provides Installed and Browse Nexus tabs; Modpacks remains a preview page. Logs shows launcher activity, including mod-install results, for the current session, with text search, rather than game output. SQLite remains the planned future structured persistence layer; this INI is only the bootstrap path configuration.

### Install local mods

1. Set the Stardew Valley game directory in **Settings** and save it. Install SMAPI separately and quit the game before changing mods.
2. On Nexus Mods, use the mod's **Files → Manual download** flow, checking the author's requirements and compatibility notes. The launcher's **Open download page** button opens that Files tab.
3. Open **Edit Instance → Mods → Installed**. Drop one or more extracted folders or `.zip`, `.7z`, or `.rar` archives onto the page, or use **Choose files…** / **Choose folder…**. This works offline without a Nexus key.
4. Read the installation result and installed-mod list. Every library entry currently uses the same `Mods` folder. On macOS a configured bundle root resolves to `Contents/MacOS/Mods`.

The installer finds `manifest.json` inside download wrappers and multi-mod bundles, preserves each mod's assets, and copies complete mod folders. Dropping an individual file inside an extracted mod installs its enclosing mod folder. Loose files without a manifest, including standalone DLLs and legacy XNB replacements, receive an explanatory error. Sources are preserved; base-game files are never replaced. These rules follow the [Stardew installation guide](https://stardewvalleywiki.com/Modding:Player_Guide/Getting_Started) and [SMAPI manifest format](https://stardewvalleywiki.com/Modding:Modder_Guide/APIs/Manifest).

The full selection is validated and staged before installation. Existing mod IDs and folder collisions are rejected, so updates require removing the existing copy manually first. Unsafe archive paths, links, encrypted or corrupt archives, and oversized inputs are rejected. Limits per selection are 100,000 entries, 2 GiB of extracted/copied data, 512 MiB per file/archive input, and 64 folder levels. Install SMAPI, Content Patcher, and any other required dependencies separately; the installer does not resolve dependencies or check game-version compatibility. See [the design](documents/mod-installation.md) for details.

The SMAPI settings dialog performs a read-only check of the configured game directory. It validates the modern SMAPI file layout (including SMAPI's commented JSON metadata), reads the matching version from SMAPI's internal managed assemblies, and reports missing or mismatched components. It never downloads, installs, updates, or modifies SMAPI.

No `.env` file or local service is needed. Do not add secrets, certificates, or game data to the repository.

### 3. Validate infrastructure in Docker

Build and run the Linux validation image:

```bash
docker build --pull=false -f Dockerfile.ci -t prismaticlauncher-ci:local .
docker run --rm prismaticlauncher-ci:local
docker run --rm --entrypoint bash prismaticlauncher-ci:local -lc ./scripts/verify-infrastructure.sh
```

The image is disposable and exposes no ports or volumes. Remove the local image when no longer needed with `docker image rm prismaticlauncher-ci:local`.

### 4. Quality checks

```bash
./scripts/format-check.sh
./scripts/static-analysis.sh
./scripts/verify-infrastructure.sh
```

Formatting and analysis include `prismatic-launcher/src/qt` and `tests`. Static analysis needs a configured and built compilation database; set `PRISMATIC_BUILD_DIR=build/local` for an alternate build directory. Linux CI uses the coverage build when the dev build is unavailable; a fresh standalone analysis run configures and builds the dev preset automatically. The 60% coverage floor applies to tested core configuration logic (`LauncherConfig.cpp`); UI presentation and tests are outside this initial core threshold.

To capture offscreen screenshots alongside the UI test run:

```bash
PRISMATIC_SCREENSHOTS=/tmp/prismatic-preview ctest --preset dev --output-on-failure
```

## CI and releases

Pull requests run Linux Docker validation, infrastructure checks, formatting, static analysis, Gitleaks, and CodeQL. Windows and macOS runners install Qt 6.8.3, build the launcher, and run the offscreen Qt tests.
Pull requests run Linux Docker validation, infrastructure checks, formatting, static-analysis configuration, Gitleaks, and CodeQL. The Linux image installs `nlohmann-json3-dev` and `libarchive-dev` alongside its Qt development packages. Before configuring and building the CMake harness, the Windows and macOS runners install Qt 6.8.3 and install `nlohmann-json` and `libarchive` with the repository-pinned vcpkg revision in classic mode. Native CMake configuration uses vcpkg's toolchain with `VCPKG_MANIFEST_MODE=OFF` and the same `VCPKG_TARGET_TRIPLET` used at installation. This loads vcpkg's package wrappers, including the compression dependencies required to link static libarchive on macOS, while retaining the separately installed Qt SDK. Windows also adds vcpkg's runtime DLL directory to PATH and activates its MSVC 2022 x64 build environment to match the Qt kit.

Tags matching `v*` start the guarded GitHub Release workflow. It intentionally fails before publishing until product-owned `packaging/` scripts exist and the protected `release` environment contains the required Apple notarization configuration. Future releases will publish portable archives: Windows ZIP, macOS ZIP, and Linux `tar.gz`, along with SHA-256 checksums.

Required GitHub configuration: create a protected `release` environment; configure `APPLE_CERTIFICATE`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, and `APPLE_TEAM_ID` for macOS signing/notarization. If Windows signing is adopted, also configure `WINDOWS_CODESIGN_CERTIFICATE` and `WINDOWS_CODESIGN_PASSWORD` and update the product packaging implementation.

## Troubleshooting

- **`cmake --preset dev` cannot find Ninja:** install Ninja and ensure it is on `PATH`, or use a future platform-specific preset.
- **Docker cannot connect to the daemon:** start Docker Desktop or the Docker Engine service, then rerun the two Docker commands above.
- **Qt is not found after product targets are added:** install Qt 6.8 LTS and configure CMake with the appropriate `Qt6_DIR`, or use the vcpkg toolchain chosen by the product implementation.
- **A release workflow stops at prerequisites:** this is expected until `packaging/` and the protected release environment are deliberately configured; it does not publish partial releases.
