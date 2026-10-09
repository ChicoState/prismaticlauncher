# Prismatic Launcher

Prismatic Launcher is a cross-platform native desktop manager for Stardew Valley mods. It currently
contains an initial Nexus Mods archive downloader; product UI, game discovery, archive installation,
SQLite data access, and release packaging have not been implemented.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — C++20 build and CTest harness configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — Qt dependency manifest and immutable vcpkg baseline.
- `scripts/` — Linux CI, quality, and infrastructure-smoke scripts.
- `tests/infrastructure/` — reserved for infrastructure-only probes; product tests are not present.
- `Dockerfile.ci` — non-root Ubuntu Linux build/test image; it is not a production image.
- `.github/workflows/` — pull-request validation and guarded release workflow definitions.
- `prismatic-launcher/documents/infrastructure_plan.md` — approved infrastructure decisions.
- `.agents/skills/` — repository-specific agent skills.
- `prismatic-launcher/src/` — product source, including the Nexus download client and CLI.
- `tests/nexus/` — deterministic Nexus download-client tests; these do not contact Nexus Mods.
- `docs/specs/nexus-mod-download.md` — scope and acceptance criteria for the Nexus integration.
- `docs/decisions/0001-nexus-mod-download-transport.md` — Nexus API and transport rationale.
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

Set `VCPKG_ROOT` to your local vcpkg checkout, then configure and build the application:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

No `.env` file or local service is needed: the app is self-contained and will use local SQLite. Do not add secrets, certificates, or game data to the repository.

### 3. Download a Nexus Mods archive

Create a user-owned API key in [Nexus Mods API settings](https://www.nexusmods.com/settings/api-keys).
Do not put it in a file, command history, or source control. Supply it only as the
`NEXUSMODS_API_KEY` environment variable when running the downloader:

```bash
NEXUSMODS_API_KEY='replace-with-your-key' ./build/dev/prismatic-nexus-download \
  --game stardewvalley --mod 2400 --file 12345 --output ./downloads/mod.zip
```

The API key is not accepted as a CLI argument and the program never prints it. The `--download-key`
and `--expires` options are available together for an `nxm` download key/expiry, which Nexus can
require for non-premium downloads. The command only resolves and saves an archive; it does not
inspect, extract, or install it.

### 4. Validate infrastructure in Docker

Build and run the Linux validation image:

```bash
docker build --pull=false -f Dockerfile.ci -t prismaticlauncher-ci:local .
docker run --rm prismaticlauncher-ci:local
docker run --rm --entrypoint bash prismaticlauncher-ci:local -lc ./scripts/verify-infrastructure.sh
```

The image is disposable and exposes no ports or volumes. Remove the local image when no longer needed with `docker image rm prismaticlauncher-ci:local`.

### 5. Quality checks

```bash
./scripts/format-check.sh
./scripts/static-analysis.sh
./scripts/verify-infrastructure.sh
```

Formatting and static-analysis scripts inspect the built Nexus downloader and its tests. Linux coverage generation enforces a 60% line-coverage floor for tested core targets.

## CI and releases

Pull requests run Linux Docker validation, infrastructure checks, formatting, static-analysis configuration, Gitleaks, and CodeQL. Windows and macOS runners install Qt 6.8, then configure, build, and discover the CTest suite.

Tags matching `v*` start the guarded GitHub Release workflow. It intentionally fails before publishing until product-owned `packaging/` scripts exist and the protected `release` environment contains the required Apple notarization configuration. Future releases will publish portable archives: Windows ZIP, macOS ZIP, and Linux `tar.gz`, along with SHA-256 checksums.

Required GitHub configuration: create a protected `release` environment; configure `APPLE_CERTIFICATE`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, and `APPLE_TEAM_ID` for macOS signing/notarization. If Windows signing is adopted, also configure `WINDOWS_CODESIGN_CERTIFICATE` and `WINDOWS_CODESIGN_PASSWORD` and update the product packaging implementation.

## Troubleshooting

- **`cmake --preset dev` cannot find Ninja:** install Ninja and ensure it is on `PATH`, or use a future platform-specific preset.
- **Docker cannot connect to the daemon:** start Docker Desktop or the Docker Engine service, then rerun the two Docker commands above.
- **Qt is not found after product targets are added:** install Qt 6.8 LTS and configure CMake with the appropriate `Qt6_DIR`, or use the vcpkg toolchain chosen by the product implementation.
- **A release workflow stops at prerequisites:** this is expected until `packaging/` and the protected release environment are deliberately configured; it does not publish partial releases.
