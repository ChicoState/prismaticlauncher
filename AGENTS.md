# Agent Guidance

## Project status

Prismatic Launcher is a C++20/Qt 6 desktop project with application sources, tests, and build/CI infrastructure. `documents/infrastructure_plan.md` records the approved infrastructure decisions. Product changes follow the user's requested feature scope; infrastructure-only tasks stay within the infrastructure boundaries below.

## Repository map

- `CMakeLists.txt`, `CMakePresets.json` — CMake/CTest configuration.
- `vcpkg.json`, `vcpkg-configuration.json` — C++ dependency manifest and pinned registry baseline.
- `scripts/` — infrastructure checks and Linux CI entrypoint.
- `tests/infrastructure/`, `tests/ui/`, `tests/smapi/` — infrastructure checks and Qt application/service tests.
- `Dockerfile.ci`, `.dockerignore` — Linux validation infrastructure only.
- `.github/workflows/pr-checks.yml`, `.github/workflows/release.yml` — CI and guarded release workflows.
- `README.md`, `documents/infrastructure_plan.md` — developer documentation and approved infrastructure plan.
- `.agents/skills/` — local skill instructions.
- `.agents/*.toml` — existing agent role definitions.
- `.github/agents/*.agent.md` — generated VS Code agent roles; regenerate with `python3 scripts/sync-vscode-agents.py` (Python 3.11+), verify with `--check`. Role sources are `.agents/*.toml` and `.codex/agents/*.toml`. The command also validates canonical skills in `.agents/skills/`; do not create duplicate skill entries under `.github/skills/`.
- `.codex/agents/*.toml`, `.codex/config.toml` — native Codex feature workers and project delegation settings.
- `documents/templates/feature-request.md` — reusable feature request template.
- `src/qt/`, `src/smapi/` — native Qt application and SMAPI services.

## Required reading and skill selection

Read `documents/infrastructure_plan.md`, this file, and the relevant `.agents/skills/*/SKILL.md` before changing the repository. Use `infra-planner` to revise infrastructure decisions; `infra-builder` for plan-approved tooling; `incremental-implementation` and `test-driven-development` for product changes; `browser-testing-with-devtools` or `test-in-browser` only for browser surfaces; `security-and-hardening` for untrusted inputs or external integrations; `documentation-and-adrs` for durable design records; `code-review-and-quality` before merge; `ci-cd-and-automation` for workflow changes; and `git-workflow-and-versioning` for every change.

Use `refine-task` when asked to evaluate or rewrite a draft task prompt. It prepares the prompt without implementing its contents; it is not a mandatory step for ordinary coding requests.

Use `feature-workflow` for end-to-end application feature requests covering refinement, planning, implementation, tests, and independent review. Its coordinator delegates focused assignments to `feature-planner`, `feature-implementer`, `feature-tester`, and `feature-reviewer` when available. Keep plan-only and prompt-only requests within their stated scope. Follow [the workflow skill](.agents/skills/feature-workflow/SKILL.md) and use [the request template](documents/templates/feature-request.md). Workers do not recursively delegate. Do not stage, commit, push, publish, or deploy unless the user separately requests it.

## Boundaries

- Do not add product UI, game discovery, mod-management behavior, domain schema, or release packaging while doing infrastructure-only work.
- Do not change the selected C++20, Qt 6, vcpkg, SQLite, Docker, or GitHub Release decisions without revising `documents/infrastructure_plan.md` through `infra-planner`.
- Do not commit secrets, signing certificates, generated build output, coverage reports, vcpkg installations, or local game/mod data.
- Docker validates Linux only; it does not replace native Windows/macOS packaging, signing, or macOS notarization.

## Code readability and documentation

Apply these conventions to new or substantially changed code, including tests and helpers. All agents that implement, refactor, or verify code must use this standard. Keep unrelated code outside the task's scope.

### Readable implementations

- Prefer clear, explicit code over compact expressions. Use descriptive names that express purpose, units, and boolean meaning; follow the surrounding naming conventions.
- Give each function one clear responsibility. Use guard clauses to keep the main flow easy to follow, and extract named helpers when they clarify a meaningful operation. Avoid nested ternaries, long compound conditions, and abstractions that add indirection without clarity.
- Use named intermediate values for complex expressions and constants for meaningful repeated values. Make ownership, lifetime, side effects, and error handling clear, especially around Qt objects and filesystem operations.
- Follow `.clang-format` for C++ formatting. Use the existing language conventions for scripts and tooling.

### Function summaries

- Add a concise, accurate summary to every new or substantially changed named function, method, constructor, and test function or helper. Generated code and declarations explicitly marked `= default` or `= delete` are exempt.
- For C++, use `/// @brief` followed by a sentence describing the function's purpose and observable behavior. Put it immediately above the declaration in the header; for functions without a separate declaration, put it above the definition. Keep one authoritative summary rather than copying it into both header and source.
- Add `/// @param`, `/// @return`, and `/// @throws` only when they explain a meaningful contract: valid inputs, units, ownership transfer, output values, failure behavior, or exceptions. Explain both outcomes of boolean results. Do not add return documentation to constructors or void functions, and do not merely repeat types or parameter names.
- Document relevant side effects, preconditions, thread requirements, or asynchronous completion in the same block when callers need them. Describe actual behavior verified in the implementation; do not invent guarantees.
- For Python, use a short docstring; for shell and CMake functions, use a short comment above the definition. Test summaries should state the scenario and expected outcome.

Example of a constructor summary in its header:

```cpp
/// @brief Takes ownership of the services used to install and launch SMAPI.
SmapiManager(std::unique_ptr<SmapiInstaller> installer,
             std::unique_ptr<SmapiLauncher> launcher);
```

### Comments within functions

- Use `//` comments in C++ to explain intent, invariants, platform constraints, workarounds, or ordering requirements that are not clear from the code. Briefly introduce a complex processing stage when it helps the reader follow the function.
- Keep comments factual, concise, and in complete sentences. Explain why a decision is necessary; avoid narrating obvious statements or adding a comment to every line. Prefer a clearer name or a small helper when a comment would only explain a dense expression.
- Update or remove stale comments as behavior changes. Preserve useful rationale, and do not leave commented-out implementations.

### Before handoff

Read the changed code as a maintainer: verify descriptive names, straightforward control flow, function summaries in the correct place, accurate contracts, and useful comments. Run the relevant formatter and behavior checks, and report what was verified.

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
