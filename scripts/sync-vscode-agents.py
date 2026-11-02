#!/usr/bin/env python3
"""Export VS Code agents and validate canonical skills without copying them (Python 3.11+)."""

import argparse
import json
from pathlib import Path
import re
import sys
import tomllib


ROOT = Path(__file__).resolve().parent.parent
AGENT_DIRECTORIES = (ROOT / ".agents", ROOT / ".codex" / "agents")
SKILL_DIRECTORY = ROOT / ".agents" / "skills"


def render_agent(source: Path) -> str:
    """Render a role from either TOML layout without overriding the selected model."""
    with source.open("rb") as stream:
        role = tomllib.load(stream)
    for field in ("name", "description", "developer_instructions"):
        if not isinstance(role.get(field), str) or not role[field].strip():
            raise ValueError(f"{source.name}: {field} must be a nonempty string")
    if role["name"] != source.stem:
        raise ValueError(f"{source.name}: name must match the source filename")

    instructions = role["developer_instructions"].strip()
    instructions = instructions.replace(
        "[docs/agents.md](../docs/agents.md)",
        "[VS Code agent setup](../../README.md#vs-code-custom-agents)",
    )
    instructions = instructions.replace(
        "`skills/browser-testing-with-devtools`",
        "[browser testing skill](../../.agents/skills/browser-testing-with-devtools/SKILL.md)",
    )
    instructions = instructions.replace(
        "`references/performance-checklist.md`",
        "the [performance skill](../../.agents/skills/performance-optimization/SKILL.md)",
    )
    instructions = instructions.replace(
        "`skills/performance-optimization/SKILL.md`",
        "the [performance skill](../../.agents/skills/performance-optimization/SKILL.md)",
    )

    return (
        "---\n"
        f"name: {json.dumps(role['name'], ensure_ascii=False)}\n"
        f"description: {json.dumps(role['description'], ensure_ascii=False)}\n"
        "user-invocable: true\n"
        "---\n\n"
        f"<!-- Generated from {source.relative_to(ROOT).as_posix()}; "
        "update the TOML source and run\n"
        "python3 scripts/sync-vscode-agents.py to regenerate. -->\n\n"
        "Follow the [repository guidance](../../AGENTS.md) and use the "
        "[local skills](../../.agents/skills/using-agent-skills/SKILL.md) when applicable. "
        "Use the actual repository structure and technology stack; any framework or path "
        "examples in the role below must be checked against this checkout.\n\n"
        "Run repository commands in Ubuntu WSL with Bash; do not use PowerShell for "
        "repository work. Keep the Windows VS Code client available from WSL through "
        "the `code` CLI, which is the supported cross-OS editor bridge.\n\n"
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


def main() -> int:
    """Validate canonical skills, then export or check agent roles and obsolete wrappers."""
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
        exports = [
            (ROOT / ".github" / "agents" / f"{source.stem}.agent.md", render_agent(source))
            for source in sources
        ]
        for source in skills:
            validate_skill(source)
    except ValueError as error:
        parser.error(str(error))
    expected = {destination for destination, _ in exports}
    orphans = []
    for pattern, marker in (
        ("agents/*.agent.md", "<!-- Generated from "),
        ("skills/*/SKILL.md", "<!-- Generated discovery entry for "),
    ):
        for existing in sorted((ROOT / ".github").glob(pattern)):
            if existing not in expected and marker in existing.read_text(encoding="utf-8"):
                orphans.append(str(existing.relative_to(ROOT)))
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
        print("Missing or stale VS Code customizations:\n" + "\n".join(stale), file=sys.stderr)
        print("Run: python3 scripts/sync-vscode-agents.py", file=sys.stderr)
        return 1
    action = "verified" if args.check else "exported"
    print(f"{len(sources)} VS Code custom agents {action}; {len(skills)} canonical skills validated.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
