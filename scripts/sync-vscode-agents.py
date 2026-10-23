#!/usr/bin/env python3
"""Export the repository's TOML roles as VS Code custom agents (Python 3.11+)."""

import argparse
import json
from pathlib import Path
import sys
import tomllib


ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRECTORY = ROOT / ".agents"
DESTINATION_DIRECTORY = ROOT / ".github" / "agents"


def render_agent(source: Path) -> str:
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
        f"<!-- Generated from .agents/{source.name}; update the TOML source and run\n"
        "python3 scripts/sync-vscode-agents.py to regenerate. -->\n\n"
        "Follow the [repository guidance](../../AGENTS.md) and use the "
        "[local skills](../../.agents/skills/using-agent-skills/SKILL.md) when applicable. "
        "Use the actual repository structure and technology stack; any framework or path "
        "examples in the role below must be checked against this checkout.\n\n"
        f"{instructions}\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check exports without writing files")
    args = parser.parse_args()
    sources = sorted(SOURCE_DIRECTORY.glob("*.toml"))
    if not sources:
        parser.error(f"No TOML agent roles found in {SOURCE_DIRECTORY}")
    exports = [(source, render_agent(source)) for source in sources]
    stale = []
    for source, content in exports:
        destination = DESTINATION_DIRECTORY / f"{source.stem}.agent.md"
        if args.check:
            if not destination.exists() or destination.read_bytes() != content.encode("utf-8"):
                stale.append(str(destination.relative_to(ROOT)))
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(content, encoding="utf-8", newline="\n")
    if stale:
        print("Missing or stale VS Code agents:\n" + "\n".join(stale), file=sys.stderr)
        print("Run: python3 scripts/sync-vscode-agents.py", file=sys.stderr)
        return 1
    print(f"{len(exports)} VS Code custom agents {'verified' if args.check else 'exported'}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
