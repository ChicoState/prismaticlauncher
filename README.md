# Prismatic Launcher

Prismatic Launcher is a C++20/Qt 6 desktop launcher for Stardew Valley. Its initial Qt Widgets UI follows `image.png`: an instance library with Add/Play/Edit and a separate instance window with Mods, Logs, Settings, and Modpacks. Automatic game discovery, mod installation, SQLite data access, and release packaging are not connected yet.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — C++20 build and CTest harness configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — Qt dependency manifest and immutable vcpkg baseline.
- `scripts/` — Linux CI, quality, and infrastructure-smoke scripts.
- `tests/ui/` — offscreen Qt Test coverage for config, search, navigation, and launch errors.
- `src/qt/` — launcher UI, stylesheet, entrypoint, and embedded default config.
- `src/smapi/` — SMAPI installer, launcher, and manager built as `launcher-smapi`.
- `tests/smapi/` — SMAPI construction, validation, and launch-error regression coverage.
- `Dockerfile.ci` — non-root Ubuntu Linux build/test image; it is not a production image.
- `.github/workflows/` — pull-request validation and guarded release workflow definitions.
- `documents/infrastructure_plan.md` — approved infrastructure decisions.
- `.agents/skills/` — repository-specific agent skills.
- `.agents/*.toml` — existing role definitions; `.github/agents/*.agent.md` — their VS Code custom-agent exports.
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

Linux CI runs the `coverage` preset, then enforces the 60% line-coverage floor on `src/qt/LauncherConfig.cpp`. Shell scripts use LF line endings (enforced by `.gitattributes`) so Docker and Linux runners can execute them after cross-platform checkouts.

The default build also compiles all SMAPI sources. With `BUILD_TESTING=ON` (the default), CMake requires Qt Test and registers `launcher-ui-tests` (offscreen) and `smapi-tests` with CTest. Pass `-DBUILD_TESTING=OFF` when configuring to build without the test executables.

Windows builds produce `prismatic-launch.exe` in the build's `bin` directory; macOS builds produce `bin/prismatic-launch.app`. Development executables still require the Qt runtime.

### Game path and UI scope

`prismatic-launcher/src/qt/config/defaults.ini` supplies `~/.local/share/Steam/steamapps/common/Stardew Valley`, the conventional Ubuntu Steam location. The launcher expands `~` for the current user and performs no automatic path discovery. Other Steam libraries, Snap/Flatpak Steam, and other OS installations require an explicit path in Settings.

Settings → Save game path writes a per-user INI override using [Qt's settings API](https://doc.qt.io/qt-6/qsettings.html). On Ubuntu the default is `~/.config/Prismatic/Prismatic Launcher/launcher.ini` (or under `XDG_CONFIG_HOME` when set). The UI shows the exact path. Use `--config /absolute/path/launcher.ini` for an alternate configuration file:

```ini
[game]
directory=/home/your-user/.local/share/Steam/steamapps/common/Stardew Valley
```

Play validates and starts the selected game's executable from that folder. Vanilla uses `StardewValley` on Ubuntu; Modded uses an already-installed `StardewModdingAPI`. Edit Instance, double-clicking an instance, and Add Instance open a separate instance window; its Settings tab shares the configured game path. Add Instance creates session-only UI entries. Modes and entries reset on restart; the game path persists. Mods and Modpacks show empty preview pages; mod-management actions are disabled. Logs shows launcher activity for the current session, with text search, rather than game output. No game files are copied, installed, or modified. SQLite remains the planned future structured persistence layer; this INI is only the bootstrap path configuration.

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

## VS Code custom agents

The built-in VS Code Agents window can use the seven `.agent.md` files in `.github/agents/` as roles with the Copilot or Codex session target. The original `.agents/*.toml` files remain available; TOML role files are not a supported format for this picker. The `.agents/skills/` folders provide skills, which are separate from selectable agent roles.

1. Open this repository folder in VS Code. When working in WSL, open the WSL checkout as the workspace.
2. Select the Copilot or Codex harness/session target. Run **Chat: Open Customizations**, then select **Agents**; in the Agents window, use **Customizations → Agents**.
3. The Workspace group and agent picker should list `code-reviewer`, `plan-agent`, `plan-reviewer`, `refactor-agent`, `security-auditor`, `test-engineer`, and `web-performance-auditor`. Select one to start using its role instructions. If the list was open before these files were added, run **Developer: Reload Window**.

Each export explicitly enables `user-invocable` so it can appear in the picker. The exports use the model selected in VS Code and its default available tools; TOML model identifiers are not copied into the VS Code role's model setting. Original role instructions are retained, with repository guidance added and broken documentation/skill links directed to existing repository files.

After updating the source roles, regenerate and check the exports with Python 3.11 or newer:

```bash
python3 scripts/sync-vscode-agents.py
python3 scripts/sync-vscode-agents.py --check
```

The check does not write files; it fails when an expected export is missing or differs from its TOML role. Keep the generated `.github/agents/` files in version control so teammates can use them without running Python. Their LF line endings are enforced by `.gitattributes`.

See [VS Code custom agents](https://code.visualstudio.com/docs/agent-customization/custom-agents) for discovery and [the agent file reference](https://code.visualstudio.com/docs/agents/reference/custom-agent-file-reference) for picker visibility and model behavior. If an agent is still missing after reloading, check the selected workspace and harness, then right-click in Chat and select **Diagnostics** to inspect customization-loading errors.

### Refine a task prompt

Use the repository's `refine-task` skill to evaluate and rewrite a draft request before asking an agent to implement it. In the Codex extension, type `$` and select **refine-task** (or open `/skills`), then provide the draft:

```text
$refine-task
Improve this draft task prompt:
Add a way to duplicate an instance. Preserve the original instance.
```

The skill checks the outcome, project context, scope, constraints, verification, and expected deliverables. It returns a ready-to-copy prompt plus meaningful changes and open questions. It makes one pass by default, labels proposed requirements, preserves your intent, and finishes without implementing the draft task. If the prompt is already sufficient, it keeps the wording instead of manufacturing changes.

Normal automatic discovery remains enabled for requests such as "refine this task prompt"; ordinary questions and direct implementation requests do not need this workflow. It reuses `interview-me`, `idea-refine`, or `spec-driven-development` only when the requested clarification or additional artifact needs them.

Review the result, then explicitly ask Codex to implement it when ready. The skill instructions live in `.agents/skills/refine-task/SKILL.md`; `agents/openai.yaml` provides the display name, description, and starting prompt for supported skill pickers. Codex normally discovers skill changes automatically; restart the Codex session if the new skill is missing.

## CI and releases

Pull requests run Linux Docker validation, infrastructure checks, formatting, static analysis, Gitleaks, and CodeQL. Windows and macOS runners install Qt 6.8.3, build the launcher, and run the offscreen Qt tests.
=======
Pull requests run Linux Docker validation, infrastructure checks, formatting, static-analysis configuration, Gitleaks, and CodeQL. Before configuring and building the CMake harness, the Windows and macOS runners install Qt 6.8.3; Windows then activates its MSVC 2022 x64 build environment to match the Qt kit. Product targets will extend those validations.

Tags matching `v*` start the guarded GitHub Release workflow. It intentionally fails before publishing until product-owned `packaging/` scripts exist and the protected `release` environment contains the required Apple notarization configuration. Future releases will publish portable archives: Windows ZIP, macOS ZIP, and Linux `tar.gz`, along with SHA-256 checksums.

Required GitHub configuration: create a protected `release` environment; configure `APPLE_CERTIFICATE`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, and `APPLE_TEAM_ID` for macOS signing/notarization. If Windows signing is adopted, also configure `WINDOWS_CODESIGN_CERTIFICATE` and `WINDOWS_CODESIGN_PASSWORD` and update the product packaging implementation.

## Troubleshooting

- **VS Code reports Qt `#include` errors from the configuration provider:** run `cmake --preset dev`, then run **CMake: Configure** with the `dev` preset in VS Code. The SMAPI sources must appear in `build/dev/compile_commands.json` so the editor receives the Qt include paths and C++20 flags. Local `.vscode/c_cpp_properties.json` can use `"configurationProvider": "ms-vscode.cmake-tools"` and `"compileCommands": ["${workspaceFolder}/build/dev/compile_commands.json"]`; see the [VS Code IntelliSense configuration reference](https://code.visualstudio.com/docs/cpp/customize-cpp-settings). If diagnostics remain stale after configuring, reload the editor window.
- **`cmake --preset dev` cannot find Ninja:** install Ninja and ensure it is on `PATH`, or use a future platform-specific preset.
- **Docker cannot connect to the daemon:** start Docker Desktop or the Docker Engine service, then rerun the two Docker commands above.
- **Qt is not found after product targets are added:** install Qt 6.8 LTS and configure CMake with the appropriate `Qt6_DIR`, or use the vcpkg toolchain chosen by the product implementation.
- **A release workflow stops at prerequisites:** this is expected until `packaging/` and the protected release environment are deliberately configured; it does not publish partial releases.
