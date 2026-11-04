# Instance console

## Objective and acceptance criteria

The instance window's Console page owns the live game log. Library launches,
instance launches, repeated launch attempts, and launch errors select this page;
no separate log dialog opens. Keep process capture and lifetime behavior intact.

Show read-only Timestamp, Level, Source, and Message columns, with visible vertical
dividers. Preserve valid SMAPI timestamps and use local receipt time otherwise.
Sort chronologically by default, oldest first, with stable ties. Clicking a
non-time heading cycles ascending, descending, then chronological order. Clicking
Timestamp toggles ascending and descending. Level sorts by severity.

Preserve severity colors, reversible severity/text filters, clear, bounded 5,000
entry history, partial-line handling, selection/copy, and scrolling without
pulling a reader away from older output. No command-input feature is introduced.
Review the instance window's function summaries and document relevant ownership,
asynchronous behavior, and non-obvious implementation decisions.

## Structure and conventions

This is a C++20/Qt 6 widget change in `src/qt/LogWindow.*` and
`src/qt/InstanceWindow.*`. Retain the console's public ingestion API and embed it
as a QWidget. Tests belong in `tests/ui/LogWindowTests.cpp` and
`tests/ui/LauncherTests.cpp`. Follow `.clang-format` and the root agent guidance:

```cpp
/// @brief Selects the embedded console and starts a validated game process.
void launchGame();
```

## Ordered work and verification

1. Add Qt Test regressions and demonstrate failure for embedding and table output.
2. Embed the console, add timestamped sortable columns, and improve instance comments.
3. Adapt existing console tests and verify parsing, controls, sorting, copy, history,
   and process output with the offscreen Qt backend.
4. Independently review the feature diff and address actionable findings.

Commands from the repository root in Ubuntu WSL:

```bash
cmake --build --preset dev
QT_QPA_PLATFORM=offscreen ctest --preset dev
bash scripts/verify-infrastructure.sh
bash scripts/format-check.sh
bash scripts/static-analysis.sh
git diff --check
```

Use the existing build configuration and dependencies. Keep one writer per file,
preserve pre-existing changes, and avoid unrelated infrastructure or product work.
Do not stage, commit, push, publish, or deploy. Linux verification does not establish
native Windows/macOS behavior.

## Verification outcome

All nine CTest targets pass, including the console and launcher regressions.
Independent review found and verified fixes for viewport anchoring and selection
preservation during sorting. Changed C++ files pass formatting, and the CRLF-aware
diff check passes. Infrastructure configuration and test discovery pass.

Repository-wide formatting has existing failures outside this feature. Direct
clang-tidy analysis of the two changed production files completes with warnings
and no errors. The repository static-analysis script fails because standalone
analysis of the unchanged `tests/TestNexusNetwork.h` cannot find
`QNetworkAccessManager`; Cppcheck is not reached by that script. Ordinary diff
checks flag pre-existing CRLF line-ending changes;
`git -c core.whitespace=cr-at-eol diff --check` passes. Docker validation is
unavailable because Docker Desktop's WSL integration is disabled. Native
Windows/macOS verification remains outstanding.
