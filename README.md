# Prismatic Launcher

Prismatic Launcher is a C++20/Qt 6 desktop launcher for Stardew Valley. Its initial Qt Widgets UI follows `image.png`: an instance library with Add/Play/Edit and a detail view with Mods, Logs, Settings, and Modpacks. Automatic game discovery, mod installation, SQLite data access, and release packaging are not connected yet.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — C++20 build and CTest harness configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — Qt dependency manifest and immutable vcpkg baseline.
- `scripts/` — Linux CI, quality, and infrastructure-smoke scripts.
- `tests/ui/` — offscreen Qt Test coverage for config, search, navigation, and launch errors.
- `src/qt/` — launcher UI, stylesheet, entrypoint, and embedded default config.
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

With Qt available on CMake's search path, configure, build, test, and run:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
./build/dev/bin/prismatic-launch
```

For an SDK installed outside system paths, pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64` to the configure command (use your platform's actual SDK directory). To resolve Qt through the pinned vcpkg manifest instead, pass `-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"`. CMake links Qt Widgets and Qt Test using the [Qt CMake integration](https://doc.qt.io/qt-6/cmake-get-started.html).

On Ubuntu with `qt6-base-dev`, CMake 3.28+, a C++ compiler, and Make installed, Ninja is optional with this alternate build:

```bash
cmake -S . -B build/local -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/local -j
ctest --test-dir build/local --output-on-failure
./build/local/bin/prismatic-launch
```

Windows builds produce `prismatic-launch.exe` in the build's `bin` directory; macOS builds produce `bin/prismatic-launch.app`. Development executables still require the Qt runtime.

### Game path and UI scope

`prismatic-launcher/src/qt/config/defaults.ini` supplies `~/.local/share/Steam/steamapps/common/Stardew Valley`, the conventional Ubuntu Steam location. The launcher expands `~` for the current user and performs no automatic path discovery. Other Steam libraries, Snap/Flatpak Steam, and other OS installations require an explicit path in Settings.

Settings → Save game path writes a per-user INI override using [Qt's settings API](https://doc.qt.io/qt-6/qsettings.html). On Ubuntu the default is `~/.config/Prismatic/Prismatic Launcher/launcher.ini` (or under `XDG_CONFIG_HOME` when set). The UI shows the exact path. Use `--config /absolute/path/launcher.ini` for an alternate configuration file:

```ini
[game]
directory=/home/your-user/.local/share/Steam/steamapps/common/Stardew Valley
```

Play validates and starts the selected game's executable from that folder. Vanilla uses `StardewValley` on Ubuntu; Modded uses an already-installed `StardewModdingAPI`. Add Instance creates session-only UI entries sharing that game path. Modes and entries reset on restart; the game path persists. Mods and Modpacks show empty preview pages; mod-management actions are disabled. Logs shows launcher activity for the current session, with text search, rather than game output. No game files are copied, installed, or modified. SQLite remains the planned future structured persistence layer; this INI is only the bootstrap path configuration.

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
PRISMATIC_SCREENSHOTS=/tmp/prismatic-preview QT_QPA_PLATFORM=offscreen ./build/dev/launcher-ui-tests
```

## CI and releases

Pull requests run Linux Docker validation, infrastructure checks, formatting, static analysis, Gitleaks, and CodeQL. Windows and macOS runners install Qt 6.8.3, build the launcher, and run the offscreen Qt tests.
=======
Pull requests run Linux Docker validation, infrastructure checks, formatting, static-analysis configuration, Gitleaks, and CodeQL. Before configuring and building the CMake harness, the Windows and macOS runners install Qt 6.8.3; Windows then activates its MSVC 2022 x64 build environment to match the Qt kit. Product targets will extend those validations.

Tags matching `v*` start the guarded GitHub Release workflow. It intentionally fails before publishing until product-owned `packaging/` scripts exist and the protected `release` environment contains the required Apple notarization configuration. Future releases will publish portable archives: Windows ZIP, macOS ZIP, and Linux `tar.gz`, along with SHA-256 checksums.

Required GitHub configuration: create a protected `release` environment; configure `APPLE_CERTIFICATE`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, and `APPLE_TEAM_ID` for macOS signing/notarization. If Windows signing is adopted, also configure `WINDOWS_CODESIGN_CERTIFICATE` and `WINDOWS_CODESIGN_PASSWORD` and update the product packaging implementation.

## Troubleshooting

- **`cmake --preset dev` cannot find Ninja:** install Ninja and ensure it is on `PATH`, or use a future platform-specific preset.
- **Docker cannot connect to the daemon:** start Docker Desktop or the Docker Engine service, then rerun the two Docker commands above.
- **Qt is not found after product targets are added:** install Qt 6.8 LTS and configure CMake with the appropriate `Qt6_DIR`, or use the vcpkg toolchain chosen by the product implementation.
- **A release workflow stops at prerequisites:** this is expected until `packaging/` and the protected release environment are deliberately configured; it does not publish partial releases.
