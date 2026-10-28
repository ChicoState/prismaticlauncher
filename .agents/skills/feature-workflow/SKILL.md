---
name: feature-workflow
description: Coordinate an application feature through requirements refinement, planning, implementation, testing, and independent review. Use for end-to-end feature requests or explicit feature-workflow invocations; not prompt-only rewriting, plan-only requests, or ordinary questions.
---

# Feature Workflow

Coordinate the requested feature through completion. The main agent owns scope,
user communication, integration, and the final verification report. Use the
[fill-in template](../../../documents/templates/feature-request.md) when the
user wants a reusable starting request.

## Establish the request

- Read root `AGENTS.md`, inspect the relevant code and Git status, and distinguish
  the requested feature from pre-existing working-tree or staged changes.
- Refine the feature's outcome, scope, constraints, and observable acceptance
  criteria without inventing product decisions. Ask only questions that materially
  affect the result and cannot be answered from the repository. Continue independent
  investigation while awaiting answers; wait before work that depends on an answer.
- Unfilled template fields are missing input, not feature requirements. Do not
  implement a template or an example feature as if the user had requested it.
- Select skills by the actual work. Use `spec-driven-development` for a significant
  feature lacking a specification, `planning-and-task-breakdown` for dependent
  tasks, `incremental-implementation` for multiple-file implementation, and
  `test-driven-development` for behavior changes. Apply `git-workflow-and-versioning`
  throughout and `code-review-and-quality` before handoff. Root readability and
  function-summary rules apply to production code, tests, and helpers.
- Add `api-and-interface-design`, `security-and-hardening`, `ci-cd-and-automation`,
  or `performance-optimization` only when the task calls for them. This is a native
  Qt application: use Qt Test for widget verification; browser skills apply only
  if the feature actually introduces or changes a browser surface.

## Coordinate the stages

Invoking this workflow requests delegation. Use the role instructions from
`.codex/agents/` for focused assignments:

| Agent | Responsibility | Writes |
| --- | --- | --- |
| `feature-planner` | Inspect context, refine acceptance criteria, return an ordered implementation and validation plan. | None. |
| `feature-implementer` | Implement one assigned slice and resolve review findings. | Assigned application/tooling files. |
| `feature-tester` | Write meaningful tests for acceptance criteria and run focused checks. | Assigned tests and fixtures. |
| `feature-reviewer` | Independently inspect the final diff, contracts, coverage, and verification evidence. | None. |

1. Have the planner return its findings, unresolved questions, relevant paths,
   selected skills, and a short ordered plan. Resolve material unknowns, then
   continue to implementation without inserting a routine approval checkpoint.
   Save a specification when the task's size or the user requires one.
2. For a behavior change, assign a focused test to the tester and confirm the
   expected failure before the implementer changes that behavior. Check each
   completed slice before expanding the change. Pure documentation or formatting
   changes need appropriate validation, not artificial failing tests.
3. Give each assignment the refined scope, acceptance criteria, relevant context,
   permitted files, and expected output. Keep one writer per file. Run only
   independent work in parallel, with at most three active workers or the lower
   client limit. Reuse finished workers for related assignments; stages that depend
   on earlier results run in order. Tell workers they share the checkout, must
   preserve others' changes, and need an explicit handoff before editing another
   writer's files. The coordinator owns delegation; workers must not recursively
   delegate through either session tools or embedded subagents.
4. When the implementation is ready, send a fresh reviewer the requirements,
   changed paths/diff, and verification evidence. Have the implementer address
   actionable findings, then rerun the affected checks and request focused review
   of the fixes. If a blocker requires user input or unavailable tooling, explain
   it accurately rather than claiming completion.

Use an additional specialist assignment only when a concrete risk needs it, for
example a filesystem security review or measured performance investigation. The
existing `.agents/*.toml` personas remain available for separate specialized tasks.

### Visible delegation and worker identity

- Prefer `create_session` when exposed by the client. Omit `relationship` (the
  default is `currentSession`) for work contributing to this feature; do not use
  `independent`. Title each chat `<role> — <assignment>` and provide the role's
  instructions, selected skills, scope, ownership, and expected output in its
  initial prompt. A title names the assignment; it does not select a custom agent.
- Share the returned user-openable link. Retain the session/chat identifier for
  `send_message` and `get_session_context`; use `list_sessions` for status and
  returned `openLink` values. Reuse related chats, queue separate assignments,
  and use `delivery: "steer"` only to update active work. Sending is asynchronous:
  read the completed result before dispatching dependent work.
- Every worker's first commentary must identify its role/name, selected skills
  with links to their `SKILL.md` files, owned files, and read-only or writer scope.
  Users can inspect these details in the linked chat; do not claim that the UI
  automatically exposes an active skill inventory.
- `collaboration.spawn_agent` creates embedded workers rather than user-openable
  Sessions/chat tabs. If visible-session tools are unavailable, disclose that
  limitation before using available embedded agents with the same role instructions
  and ownership rules. Do not promise tab visibility across clients. If neither
  mechanism is available, perform the stages in the main session and do not claim
  an independent review. Do not invent configuration keys or change user-wide
  settings to force visibility.

## Verify and hand off

- Verify with the actual CMake/CTest targets and relevant formatting/static checks.
  Qt UI tests use the configured offscreen backend. Report native Windows/macOS
  checks as unverified unless they were actually run; Linux checks alone do not
  establish cross-platform compatibility.
- Inspect the final diff and `git diff --check`. Ensure new sources and resources
  referenced by build files are present in the proposed change. Keep pre-existing
  issues distinct from regressions introduced by this feature.
- Report the resulting behavior, changed files, tests/checks actually run,
  independent review outcome, and material remaining limitations.
- Do not stage, commit, push, publish, or deploy unless the user separately requests
  that action. A skill's suggestion to commit between slices does not authorize it.
