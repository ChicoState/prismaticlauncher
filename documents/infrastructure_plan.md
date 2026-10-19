# Infrastructure Plan

> Planning only. This document describes future infrastructure work. No installations, configuration changes, containers, workflows, deployments, or other implementation files were created by the infrastructure-planning process.

## 1. Project and User Experience

- **Application:** Prismatic Launcher, a Stardew Valley mod manager and installer.
- **Primary users:** Individual Stardew Valley players.
- **Primary user task:** Locate a local Stardew Valley installation, install and manage mods, and maintain mod profiles.
- **Selected platform:** Cross-platform native desktop application.
- **User-experience rationale:** Direct, reliable access to game and mod folders is essential; one native codebase should serve Windows, macOS, and Linux.
- **Required operating systems:** Windows, macOS, and Linux desktop systems supported by Stardew Valley.
- **Offline or native-device requirements:** Core profile and file management work offline; the app needs local filesystem access. Downloading public mods naturally requires connectivity.

## 2. Connectivity and Application Shape

- **Connectivity model:** Self-contained.
- **Accounts and authentication:** None. The application has no user account or hosted backend.
- **Backend required:** No; any future requests to public mod sources are direct client requests and are outside persistent application infrastructure.
- **Cross-device persistence:** Not provided; data stays on the user's computer.
- **Interaction between accounts:** None.
- **Primary application components:** Qt desktop UI; local game-discovery and filesystem service; mod installation/profile service; SQLite persistence layer; direct public-download client where implemented.

## 3. Selected Technology Stack

| Area | Selected technology | Purpose | Version policy |
|---|---|---|---|
| Primary language | C++ | Native application and core logic | Current supported C++ standard selected by the project, with compiler support on all target OSs |
| Application framework | Qt 6 | Cross-platform native UI, filesystem, networking, and SQL APIs | Supported Qt 6 LTS or maintained stable release |
| Runtime or SDK | Native C++ compiler and Qt SDK | Build and run the desktop application | Supported releases on each target OS |
| Package manager | vcpkg | Reproducible third-party C++ dependency acquisition | Commit a manifest and baseline during implementation |
| Build and test tool | CMake and CTest | Configure builds and execute tests locally and in CI | Supported stable CMake release |
| Linux validation environment | Docker | Repeatable Linux build-and-test environment only | Pinned base image and tool versions during implementation |

## 4. Storage and Persistence

- **Storage model:** Local.
- **Primary data store:** SQLite for application settings, game locations, mod records, and named profiles.
- **User files or object storage:** Local filesystem for mod archives, installed mod files, logs, and user-selected paths; no object storage service.
- **Local-development storage:** Temporary or developer-local SQLite databases and fixture directories, never a shared database service.
- **Production hosting model:** No production data hosting; each installation owns its database and files.
- **Schema and migration approach:** Versioned, transactional SQLite migrations run by the app before opening a newer schema.
- **Backup, export, or recovery approach:** Provide a future profile/settings export and document the application-data location; do not modify the game installation until validation succeeds.
- **Secrets and connection-string approach:** No database credentials. Any future public-download API token must be user-supplied or an approved build secret, never embedded in the application.
- **Reason this storage fits the access pattern:** SQLite provides robust structured local state while the filesystem remains the source for game and mod files.

## 5. Testing Tools

| Test layer | Tool or library | Planned scope | Planned execution point |
|---|---|---|---|
| Unit | Qt Test | Domain logic, profile rules, SQLite repositories, and path validation | Local and pull requests |
| Integration | Qt Test with temporary SQLite databases and fixture game/mod directories | Filesystem, migration, and installation workflows without touching a real game install | Local and pull requests |
| End-to-end or UI | Qt Test widget/UI tests | Critical workflows such as selecting a game location, creating a profile, and installing a fixture mod | Pull requests where reliable; release validation |
| Test orchestration | CTest | Discover, filter, and report all test executables | Local, Docker Linux validation, and pull requests |

## 6. Test Analysis

| Capability | Tool | Planned policy |
|---|---|---|
| Coverage | gcovr over GCC coverage data in the Linux container | Generate XML and HTML coverage reports from CTest runs |
| Coverage threshold or regression rule | gcovr and CI comparison | Start with a modest 60% line-coverage floor for tested core targets; do not accept a decrease on changed core modules without explanation |
| Mutation testing | Not initially selected | Reconsider only for high-risk mod-installation or migration logic once the suite is fast and stable |
| Flaky-test or duration analysis | CTest JUnit output and GitHub Actions timing | Retain reports and investigate repeated UI-test failures |
| Reporting | GitHub Actions artifacts and check summaries | Upload coverage reports on failures and successful protected-branch runs |

## 7. Static Analysis and Security

| Check | Tool | Planned enforcement |
|---|---|---|
| Formatting | clang-format | Verify formatting in every pull request |
| Linting | clang-tidy | Run selected correctness, bug-prone, performance, and readability checks in pull requests |
| Type checking or compiler warnings | GCC, Clang, and MSVC warnings | Enable a documented strict warning set; warnings remain visible and newly introduced serious warnings block merging |
| Anti-pattern or maintainability analysis | Cppcheck | Run C++ correctness and maintainability analysis in pull requests |
| Dependency vulnerability scanning | Dependabot alerts and updates for GitHub Actions and supported dependency manifests | Enable repository alerts and review automated update pull requests |
| Secret scanning | Gitleaks | Scan the repository and pull-request changes; block detected secrets |
| Static security analysis | GitHub CodeQL for C++ | Run security-and-quality analysis on pull requests and on a schedule |

## 8. Development Technologies Requiring Manual Installation

These are developer-workstation prerequisites that will not be supplied by the planned Docker environment.

| Technology | Why it is needed | Required on which machines | Version policy | Planned installation or verification method | Why Docker does not provide it |
|---|---|---|---|---|---|
| Git | Clone, branch, and contribute to the repository | All developer workstations | Supported stable release | Future onboarding check | Source-control credentials and workstation integration are host concerns |
| CMake, a C++ compiler, and native build tools | Configure and build the native app | All developer workstations | Supported compiler/toolchain for each OS | Future onboarding check | Native compilers and platform SDK integration are required for local Qt builds |
| Qt 6 SDK | Develop and debug the native UI | All developer workstations | Selected supported Qt 6 release | Future documented Qt installer or package-manager setup | Desktop UI development and debugging need host-native Qt tooling |
| vcpkg | Resolve C++ dependencies | All developer workstations | Repository-pinned baseline when implemented | Future bootstrap and verification instructions | Developers need it for native local builds; CI container has its own provisioned copy |
| Docker Desktop or Docker Engine | Run the repeatable Linux build-and-test image | Developers using Linux validation; CI runner provides its own daemon | Supported stable release | Future optional onboarding check | Docker itself is the host runtime for the planned container |
| Platform packaging and signing tools | Produce distributable Windows, macOS, and Linux packages | Release maintainers on the applicable native OS | Current platform-supported tools | Future release-runner setup | Containers cannot replace native platform signing, notarization, or packaging environments |

### Host tools intentionally not required

- **Not required because Docker supplies them:** Linux CI compiler, Qt dependencies, and coverage tooling inside the future Linux validation image.
- **Not required for this platform:** A hosted database, backend runtime, cloud CLI, mobile SDK, or production application container runtime.

## 9. Docker Plan

- **Planned Docker role:** Linux build-and-test container only.
- **Future files that would be created during implementation:** A Linux CI `Dockerfile`, `.dockerignore`, and any narrowly scoped helper configuration needed to invoke it.
- **Planned images and services:** One non-root Linux builder/test image with pinned C++ compiler, CMake, Qt development packages, vcpkg dependencies, CTest, and gcovr; no database or application service.
- **Development container behavior:** Build and test a source checkout in a disposable container; native UI development remains on the host.
- **Ports:** None.
- **Bind mounts and named volumes:** Bind-mount the source read/write only when a developer intentionally uses the validation container; use a cache volume for vcpkg only if it is safe to invalidate.
- **Environment-variable and secret handling:** No secrets in the image or Docker build context; pass only non-sensitive build settings at run time.
- **Local database or service containers:** None; integration tests create temporary SQLite databases.
- **Production image or non-container release path:** Native platform packages are built on native GitHub runners and attached to GitHub Releases; no production container is released.
- **Build stages and hardening:** Use a minimal pinned builder image, a non-root user, `.dockerignore`, and no embedded credentials; production-image health checks do not apply.
- **Planned future development command:** `docker build -f Dockerfile.ci -t prismaticlauncher-ci .` (future only; not executed by this planning process).
- **Planned future validation command:** `docker run --rm -v "$PWD:/workspace" prismaticlauncher-ci` (future only; not executed by this planning process).

## 10. GitHub Actions Plan

### A. Automated pull-request checks

- **Future workflow file:** `.github/workflows/pr-checks.yml`
- **Trigger:** `pull_request`; optionally `push` to the protected default branch for post-merge confirmation.
- **Runner or matrix:** Ubuntu runner for the Docker/Linux build and coverage; Windows and macOS runners for native configure/build/test validation where Qt setup time is acceptable.
- **Permissions:** Default to `contents: read`; grant only the narrow permissions required for CodeQL result upload and Dependabot pull requests.
- **Planned jobs in order:**
  1. Checkout, set up dependency caches, and verify the vcpkg baseline/lockfile policy.
  2. Run clang-format verification, clang-tidy, Cppcheck, Gitleaks, and CodeQL analysis.
  3. Build and run CTest in the Linux Docker image; collect gcovr coverage and enforce the selected threshold policy.
  4. Configure, build, and run applicable CTest suites on Windows and macOS native runners.
  5. Upload CTest logs, Qt UI-test screenshots when produced, and coverage artifacts on failures.
- **Service containers:** None.
- **Caching:** Cache vcpkg download/binary caches and Qt/tooling caches only when keys include OS, compiler, Qt version, vcpkg baseline, and manifest inputs.
- **Coverage and analysis reporting:** Publish Linux gcovr summary and retain HTML/XML reports as artifacts; surface tool failures as required checks.
- **Failure artifacts:** CTest JUnit/XML logs, coverage output, build logs, and UI-test screenshots or traces when available.
- **Checks that should block merging:** Formatting, static analysis, secret scan, CodeQL, successful Linux build/tests, coverage policy, and successful native build/tests for supported OS runners.
- **Proposed branch-protection settings:** Require the blocking checks above, require a current branch before merge, require at least one approving review, and restrict direct pushes to the default branch.

### B. New-release deployment

- **Future workflow file:** `.github/workflows/release.yml`
- **Release trigger:** A pushed, validated `v*` tag; allow `workflow_dispatch` only for maintainers selecting an existing tag.
- **Release destination:** GitHub Releases with Windows, macOS, and Linux installer/package assets.
- **Runner or matrix:** Windows, macOS, and Ubuntu native runners; do not use Docker for Windows or macOS release packaging.
- **Planned jobs in order:**
  1. Verify tag/version consistency, run formatting, static analysis, build, and the applicable CTest suite before publishing.
  2. Build packages on the native OS matrix and sign/notarize assets where credentials are available.
  3. Generate checksums and attach packages, checksums, and release notes to the GitHub Release.
  4. Download each uploaded asset for a basic install/package smoke check before marking the release ready.
- **Build artifacts:** Windows ZIP archive, macOS signed/notarized app ZIP archive, Linux `tar.gz` archive, and SHA-256 checksums.
- **Signing, notarization, or store requirements:** Windows code-signing certificate where distributed installer policy requires it; Apple Developer certificate, signing identity, notarization credentials, and Apple Developer account for macOS distribution; no app-store accounts selected.
- **Database migration step:** No hosted migration. The released app performs its own local transactional SQLite migration at startup.
- **Environment approval:** A protected GitHub `release` environment with maintainer approval before publishing signed assets.
- **Post-deployment verification:** Validate release assets and checksums; manually smoke-test the signed package on each target OS before broad announcement.
- **Failed-release or rollback approach:** Mark the GitHub Release as draft or pre-release, remove defective assets, publish a corrected version tag/release, and document affected versions in release notes; do not overwrite released artifacts.

### GitHub configuration required later

| Name | Type | Purpose |
|---|---|---|
| `release` | GitHub environment | Protect publication of public release assets with required reviewer approval |
| `WINDOWS_CODESIGN_CERTIFICATE` | Secret/certificate | Encoded Windows code-signing certificate, if Windows binaries are signed |
| `WINDOWS_CODESIGN_PASSWORD` | Secret | Password for the Windows signing certificate, if used |
| `APPLE_CERTIFICATE` | Secret/certificate | macOS signing certificate material |
| `APPLE_CERTIFICATE_PASSWORD` | Secret | Password for the macOS signing certificate |
| `APPLE_SIGNING_IDENTITY` | Secret or variable | Selected macOS signing identity name |
| `APPLE_ID` | Secret | Apple account used for notarization |
| `APPLE_APP_SPECIFIC_PASSWORD` | Secret | App-specific password for notarization |
| `APPLE_TEAM_ID` | Variable | Apple Developer team identifier |
| GitHub Actions and GitHub Releases access | Account/token | Repository-maintainer authorization; use the workflow-provided token with least privileges where sufficient |

## 11. Planned Repository Artifacts - Not Created by This Skill

List only the files that a later implementation task is expected to create. This section is documentation, not authorization to create them now.

- [ ] Application manifest or project file: `CMakeLists.txt`, `vcpkg.json`, and a vcpkg configuration/baseline file.
- [ ] Lockfile: vcpkg baseline/lock-equivalent metadata supported by the selected vcpkg workflow.
- [ ] Test configuration: CMake/CTest definitions and Qt Test targets.
- [ ] Static-analysis configuration: `.clang-format`, `.clang-tidy`, Cppcheck configuration if needed, and Gitleaks policy.
- [ ] Docker or Compose files: Linux CI `Dockerfile` and `.dockerignore`; no Compose file expected.
- [ ] `.github/workflows/pr-checks.yml`: Required pull-request validation workflow.
- [ ] `.github/workflows/release.yml`: Required tagged-release packaging and publishing workflow.
- [ ] Deployment or store configuration: Native package/signing configuration; no hosting or store configuration selected.

## 12. Assumptions and Open Items

- **Assumptions:** Stardew Valley installation paths can be located or chosen by the user; mod sources may be accessed publicly without an application backend; initial support covers Windows, macOS, and Linux.
- **Decisions still requiring an external account, credential, certificate, or organizational approval:** Whether Windows signing is required; Apple Developer enrollment and notarization credentials; release-environment approvers; any future public mod-source API terms or credentials.
- **Items to confirm before implementation begins:** Exact supported OS and Qt versions, game-store install-location discovery scope, mod-source integration and license/terms, application-data location, and the initial coverage threshold after the first representative test suite exists.
