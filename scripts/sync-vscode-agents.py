#!/usr/bin/env python3
"""Export VS Code agents and Claude Code subagents, and validate canonical skills (Python 3.11+)."""

import argparse
import json
from pathlib import Path
import re
import sys
import tomllib


ROOT = Path(__file__).resolve().parent.parent
AGENT_DIRECTORIES = (ROOT / ".agents", ROOT / ".codex" / "agents")
SKILL_DIRECTORY = ROOT / ".agents" / "skills"
VSCODE_AGENT_DIRECTORY = ROOT / ".github" / "agents"
CLAUDE_AGENT_DIRECTORY = ROOT / ".claude" / "agents"
GENERATED_MARKER = "<!-- Generated from "

# Both export directories sit two levels below the root, so these links resolve
# identically from .github/agents/ and .claude/agents/.
SHARED_LINK_REPLACEMENTS = (
    (
        "`skills/browser-testing-with-devtools`",
        "[browser testing skill](../../.agents/skills/browser-testing-with-devtools/SKILL.md)",
    ),
    (
        "`references/performance-checklist.md`",
        "the [performance skill](../../.agents/skills/performance-optimization/SKILL.md)",
    ),
    (
        "`skills/performance-optimization/SKILL.md`",
        "the [performance skill](../../.agents/skills/performance-optimization/SKILL.md)",
    ),
)
SOURCE_DOCS_LINK = "[docs/agents.md](../docs/agents.md)"
VSCODE_DOCS_LINK = "[VS Code agent setup](../../README.md#vs-code-custom-agents)"
CLAUDE_DOCS_LINK = "the [repository guidance](../../AGENTS.md)"

# Codex session wording that would mislead Claude Code. Each original passage must
# exist in its role source, so an edited source cannot silently skip the rewrite.
CLAUDE_FIRST_COMMENTARY = (
    "In your first commentary, identify",
    "Begin your final report by identifying",
)
CLAUDE_ROLE_REPLACEMENTS = {
    "feature-workflow": (
        (
            "roles using visible Sessions/chat tools when available, following the workflow\n"
            "skill for creation, links, and coordination. Title assignments <role> — <assignment>\n"
            "and require first commentary to identify role/name, linked skills, and read-only or\n"
            "writer ownership.",
            "roles with the Agent tool, setting `subagent_type` to the role name and the\n"
            "description to <role> — <assignment>. Give each prompt the role's scope, file\n"
            "ownership, and expected output, and require each report to begin by identifying\n"
            "role/name, linked skills, and read-only or writer ownership. The workflow skill's\n"
            "`create_session` and `spawn_agent` guidance is for other clients; Claude Code\n"
            "returns no user-openable session links, so relay each worker's report instead.",
        ),
        (
            "Invoking this\n"
            "role requests delegation; if the harness cannot delegate, disclose that limitation\n"
            "and complete the stages yourself without claiming an independent review.",
            "Invoking this\n"
            "role requests delegation. Claude Code subagents cannot start other subagents, so\n"
            "delegation works only when this role runs as the main session (for example,\n"
            "`claude --agent feature-workflow`). If the Agent tool is unavailable, disclose that\n"
            "limitation and complete the stages yourself without claiming an independent review.",
        ),
    ),
    "feature-implementer": (CLAUDE_FIRST_COMMENTARY,),
    "feature-planner": (CLAUDE_FIRST_COMMENTARY,),
    "feature-reviewer": (CLAUDE_FIRST_COMMENTARY,),
    "feature-tester": (CLAUDE_FIRST_COMMENTARY,),
}


def load_role(source: Path) -> dict:
    """Load a TOML role and require nonempty name, description, and instructions fields.

    Raises ValueError when a field is missing or empty, or when the name differs
    from the source filename.
    """
    with source.open("rb") as stream:
        role = tomllib.load(stream)
    for field in ("name", "description", "developer_instructions"):
        if not isinstance(role.get(field), str) or not role[field].strip():
            raise ValueError(f"{source.name}: {field} must be a nonempty string")
    if role["name"] != source.stem:
        raise ValueError(f"{source.name}: name must match the source filename")
    return role


def link_shared_references(instructions: str) -> str:
    """Rewrite skill and checklist paths into links that resolve from an export directory."""
    for original, replacement in SHARED_LINK_REPLACEMENTS:
        instructions = instructions.replace(original, replacement)
    return instructions


def generated_notice(source: Path) -> str:
    """Return the marker comment that identifies a generated file and its TOML source."""
    return (
        f"{GENERATED_MARKER}{source.relative_to(ROOT).as_posix()}; "
        "update the TOML source and run\n"
        "python3 scripts/sync-vscode-agents.py to regenerate. -->\n\n"
    )


def render_agent(source: Path, role: dict) -> str:
    """Render a VS Code custom agent without overriding the selected model."""
    instructions = role["developer_instructions"].strip()
    instructions = instructions.replace(SOURCE_DOCS_LINK, VSCODE_DOCS_LINK)
    instructions = link_shared_references(instructions)

    return (
        "---\n"
        f"name: {json.dumps(role['name'], ensure_ascii=False)}\n"
        f"description: {json.dumps(role['description'], ensure_ascii=False)}\n"
        "user-invocable: true\n"
        "---\n\n"
        f"{generated_notice(source)}"
        "Follow the [repository guidance](../../AGENTS.md) and use the "
        "[local skills](../../.agents/skills/using-agent-skills/SKILL.md) when applicable. "
        "Use the actual repository structure and technology stack; any framework or path "
        "examples in the role below must be checked against this checkout.\n\n"
        "Run repository commands in Ubuntu WSL with Bash; do not use PowerShell for "
        "repository work. Keep the Windows VS Code client available from WSL through "
        "the `code` CLI, which is the supported cross-OS editor bridge.\n\n"
        f"{instructions}\n"
    )


def adapt_for_claude(source: Path, role: dict) -> str:
    """Replace Codex session wording in a role's instructions with Claude Code equivalents.

    Raises ValueError when an expected passage is missing from the source, so the
    replacement table must be updated whenever that role's wording changes.
    """
    instructions = role["developer_instructions"].strip()
    for original, replacement in CLAUDE_ROLE_REPLACEMENTS.get(role["name"], ()):
        if original not in instructions:
            raise ValueError(
                f"{source.name}: expected Codex wording for the Claude export was not found; "
                "update CLAUDE_ROLE_REPLACEMENTS in scripts/sync-vscode-agents.py"
            )
        instructions = instructions.replace(original, replacement)
    instructions = instructions.replace(SOURCE_DOCS_LINK, CLAUDE_DOCS_LINK)
    return link_shared_references(instructions)


def render_claude_agent(source: Path, role: dict) -> str:
    """Render a Claude Code subagent that inherits the session's tools and model.

    The Codex model and VS Code-only fields are intentionally not exported.
    Raises ValueError when adapt_for_claude cannot find expected source wording.
    """
    instructions = adapt_for_claude(source, role)

    return (
        "---\n"
        f"name: {json.dumps(role['name'], ensure_ascii=False)}\n"
        f"description: {json.dumps(role['description'], ensure_ascii=False)}\n"
        "---\n\n"
        f"{generated_notice(source)}"
        "Follow the [repository guidance](../../AGENTS.md) and use the "
        "[local skills](../../.agents/skills/using-agent-skills/SKILL.md) when applicable. "
        "Use the actual repository structure and technology stack; any framework or path "
        "examples in the role below must be checked against this checkout.\n\n"
        "Run repository commands in Ubuntu WSL with Bash; do not use PowerShell for "
        "repository work.\n\n"
        "Claude Code subagents cannot start other subagents. When this role runs as a "
        "subagent, perform any exploration the role below assigns to sub-agents yourself, "
        "and report delegation recommendations instead of attempting them.\n\n"
        f"{instructions}\n"
    )


def skill_string(header: str, field: str, source: Path) -> str:
    """Read the repository's single-line YAML string fields and reject ambiguous values."""
    matches = re.findall(rf"^{re.escape(field)}:[ \t]*(.+)$", header, re.MULTILINE)
    if len(matches) != 1:
        raise ValueError(f"{source}: {field} must appear once as a single-line string")
    value = matches[0].strip()
    if not value:
        raise ValueError(f"{source}: {field} must be a nonempty string")
    if value.startswith('"'):
        value = json.loads(value)
    elif value.startswith("'"):
        if not re.fullmatch(r"'(?:[^']|'')*'", value):
            raise ValueError(f"{source}: {field} contains malformed YAML quotes")
        value = value[1:-1].replace("''", "'")
    elif (
        re.search(r":(?:\s|$)|\s#|^[-?:](?:\s|$)", value)
        or value[0] in "[]{}&*!|>#%@`"
        or value.lower() in {"true", "false", "yes", "no", "on", "off", "null", "~"}
        or re.match(r"^[+-]?(?:\d|\.\d|\.inf$|\.nan$)", value, re.IGNORECASE)
    ):
        raise ValueError(f"{source}: quote the {field} value to make it valid YAML")
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{source}: {field} must be a nonempty string")
    return value


def validate_skill(source: Path) -> None:
    """Validate canonical skill metadata without creating a duplicate discovery entry."""
    text = source.read_text(encoding="utf-8")
    if not text.startswith("---\n"):
        raise ValueError(f"{source}: missing YAML frontmatter")
    header, separator, _ = text[4:].partition("\n---\n")
    if not separator:
        raise ValueError(f"{source}: missing YAML frontmatter closing delimiter")
    name = skill_string(header, "name", source)
    description = skill_string(header, "description", source)
    if name != source.parent.name:
        raise ValueError(f"{source}: name must match the skill directory")
    if len(description) > 1024:
        raise ValueError(f"{source}: description exceeds 1024 characters")


def build_exports(sources: list[Path]) -> list[tuple[Path, str]]:
    """Return each role's VS Code and Claude Code destination paired with its content.

    Raises ValueError when a role source is invalid.
    """
    exports = []
    for source in sources:
        role = load_role(source)
        exports.append(
            (VSCODE_AGENT_DIRECTORY / f"{source.stem}.agent.md", render_agent(source, role))
        )
        exports.append(
            (CLAUDE_AGENT_DIRECTORY / f"{source.stem}.md", render_claude_agent(source, role))
        )
    return exports


def find_orphans(expected: set[Path]) -> list[str]:
    """List generated files whose canonical source no longer exists, relative to the root."""
    generated_patterns = (
        (VSCODE_AGENT_DIRECTORY, "*.agent.md", GENERATED_MARKER),
        (ROOT / ".github" / "skills", "*/SKILL.md", "<!-- Generated discovery entry for "),
        (CLAUDE_AGENT_DIRECTORY, "*.md", GENERATED_MARKER),
    )
    orphans = []
    for directory, pattern, marker in generated_patterns:
        for existing in sorted(directory.glob(pattern)):
            if existing not in expected and marker in existing.read_text(encoding="utf-8"):
                orphans.append(str(existing.relative_to(ROOT)))
    return orphans


def main() -> int:
    """Validate canonical skills, then export or check agent roles and obsolete wrappers.

    Returns 0 on success and 1 when generated files are orphaned or, with --check,
    missing or stale.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check exports without writing files")
    args = parser.parse_args()
    sources = sorted(
        source for directory in AGENT_DIRECTORIES for source in directory.glob("*.toml")
    )
    if not sources:
        parser.error("No TOML agent roles found in .agents or .codex/agents")
    if len({source.stem for source in sources}) != len(sources):
        parser.error("Duplicate agent names across .agents and .codex/agents")
    skills = sorted(SKILL_DIRECTORY.glob("*/SKILL.md"))
    try:
        exports = build_exports(sources)
        for source in skills:
            validate_skill(source)
    except ValueError as error:
        parser.error(str(error))
    orphans = find_orphans({destination for destination, _ in exports})
    if orphans:
        print("Orphaned generated customizations:\n" + "\n".join(orphans), file=sys.stderr)
        print(
            "Restore their canonical sources or remove the obsolete generated entries.",
            file=sys.stderr,
        )
        return 1
    stale = []
    for destination, content in exports:
        if args.check:
            if not destination.exists() or destination.read_bytes() != content.encode("utf-8"):
                stale.append(str(destination.relative_to(ROOT)))
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(content, encoding="utf-8", newline="\n")
    if stale:
        print("Missing or stale generated agents:\n" + "\n".join(stale), file=sys.stderr)
        print("Run: python3 scripts/sync-vscode-agents.py", file=sys.stderr)
        return 1
    action = "verified" if args.check else "exported"
    print(
        f"{len(sources)} VS Code custom agents and {len(sources)} Claude Code subagents "
        f"{action}; {len(skills)} canonical skills validated."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
