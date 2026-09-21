# Agent Guidance

## Project status

Prismatic Launcher is an infrastructure-only C++20/Qt 6.8 desktop project foundation. `infrastructure_plan.md` is the approved decision record; the files in this repository implement its tooling, Docker validation, CI, and documentation, not product behavior.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — CMake/CTest configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — C++ dependency manifest and pinned registry baseline.
- `scripts/` — infrastructure checks and Linux CI entrypoint.
- `tests/infrastructure/` — infrastructure-only test location; product tests are not created yet.
- `Dockerfile.ci`, `.dockerignore` — Linux validation infrastructure only.
- `.github/workflows/pr-checks.yml`, `.github/workflows/release.yml` — CI and guarded release workflows.
- `README.md`, `infrastructure_plan.md` — developer documentation and approved plan.
- `.agents/skills/` — local skill instructions.
- `src/`, `packaging/`, backend/API directories — not created yet.

## Required reading and skill selection

Read `infrastructure_plan.md`, this file, and the relevant `.agents/skills/*/SKILL.md` before changing the repository. Use `infra-planner` to revise infrastructure decisions; `infra-builder` for plan-approved tooling; `incremental-implementation` and `test-driven-development` for product changes; `browser-testing-with-devtools` or `test-in-browser` only for browser surfaces; `security-and-hardening` for untrusted inputs or external integrations; `documentation-and-adrs` for durable design records; `code-review-and-quality` before merge; `ci-cd-and-automation` for workflow changes; and `git-workflow-and-versioning` for every change.

## Boundaries

- Do not add product UI, game discovery, mod-management behavior, domain schema, or release packaging while doing infrastructure-only work.
- Do not change the selected C++20, Qt 6, vcpkg, SQLite, Docker, or GitHub Release decisions without revising `infrastructure_plan.md` through `infra-planner`.
- Do not commit secrets, signing certificates, generated build output, coverage reports, vcpkg installations, or local game/mod data.
- Docker validates Linux only; it does not replace native Windows/macOS packaging, signing, or macOS notarization.

## Verification

Run these commands from the repository root when their prerequisites are installed:

```bash
./scripts/verify-infrastructure.sh
./scripts/format-check.sh
./scripts/static-analysis.sh
docker build --pull=false -f Dockerfile.ci -t prismaticlauncher-ci:local .
docker run --rm prismaticlauncher-ci:local
```

CI mirrors these checks in `.github/workflows/pr-checks.yml`. No service lifecycle is required: the Docker image is disposable, starts no daemon, exposes no ports, and should be removed with `docker image rm prismaticlauncher-ci:local` when no longer needed.

## Change checklist

1. Read the plan and applicable local instructions.
2. Keep the change within its approved boundary and update documentation when commands, paths, prerequisites, or CI behavior change.
3. Run the relevant local checks and report anything not verified.
4. Inspect `git diff --check` and the changed-file list before handoff.
