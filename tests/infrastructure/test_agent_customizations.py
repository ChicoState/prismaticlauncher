"""Check discovery exports without depending on Qt, VS Code, or model calls."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "sync-vscode-agents.py"
REPOSITORY_ROOT = SCRIPT.parents[1]


class CustomizationExportTests(unittest.TestCase):
    """Exercise isolated repositories through the customization sync command."""

    def setUp(self):
        """Create one legacy role, one native worker, and one canonical skill."""
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "scripts").mkdir()
        shutil.copyfile(SCRIPT, self.root / "scripts" / SCRIPT.name)
        self.write_role(".agents/coordinator.toml", "coordinator")
        self.write_role(".codex/agents/worker.toml", "worker")
        self.write(
            ".agents/skills/example/SKILL.md",
            '---\nname: example\ndescription: "Use for a specific task."\n---\n\n'
            "# Example\nRead [the fixture](./references/fixture.txt).\n",
        )
        self.write(".agents/skills/example/references/fixture.txt", "Fixture context.\n")

    def write(self, relative, content):
        """Write a fixture below the isolated repository root."""
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def write_role(self, relative, name):
        """Write a minimal role whose instructions must survive export."""
        self.write(
            relative,
            f"name = {json.dumps(name)}\ndescription = \"A focused role.\"\n"
            'developer_instructions = "Follow the assigned task."\n',
        )

    def run_sync(self, *arguments):
        """Run the copied command and capture diagnostics for assertions."""
        return subprocess.run(
            [sys.executable, str(self.root / "scripts" / SCRIPT.name), *arguments],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_exports_native_workers_without_duplicating_skills(self):
        """Both role layouts are exported while skills keep one canonical location."""
        result = self.run_sync()
        self.assertEqual(result.returncode, 0, result.stderr)
        for role in ("coordinator", "worker"):
            exported = self.root / f".github/agents/{role}.agent.md"
            self.assertTrue(exported.is_file(), str(exported))
            self.assertIn("Follow the assigned task.", exported.read_text())
        skill = self.root / ".github/skills/example/SKILL.md"
        self.assertFalse(skill.exists())
        canonical = self.root / ".agents/skills/example/SKILL.md"
        self.assertTrue(canonical.is_file())
        self.assertTrue((canonical.parent / "references/fixture.txt").is_file())
        self.assertEqual(self.run_sync("--check").returncode, 0)

    def test_exports_require_wsl_for_repository_commands(self):
        """Every generated role keeps repository commands out of Windows PowerShell."""
        result = self.run_sync()
        self.assertEqual(result.returncode, 0, result.stderr)
        exported = self.root / ".github/agents/coordinator.agent.md"
        instructions = exported.read_text(encoding="utf-8")
        self.assertIn("Run repository commands in Ubuntu WSL with Bash", instructions)
        self.assertIn("do not use PowerShell for repository work", instructions)
        self.assertIn("Windows VS Code client available from WSL", instructions)
        self.assertIn("through the `code` CLI", instructions)

    def test_check_detects_a_missing_native_agent_export(self):
        """Deleting a worker export causes verification to fail."""
        self.assertEqual(self.run_sync().returncode, 0)
        target = self.root / ".github/agents/worker.agent.md"
        if target.exists():
            target.unlink()
        self.assertNotEqual(self.run_sync("--check").returncode, 0)

    def test_skill_updates_require_no_generated_copy(self):
        """A valid canonical description update needs no second skill entry."""
        self.assertEqual(self.run_sync().returncode, 0)
        source = self.root / ".agents/skills/example/SKILL.md"
        source.write_text(source.read_text().replace("specific task", "different task"))
        self.assertEqual(self.run_sync("--check").returncode, 0)

    def test_invalid_native_role_prevents_partial_exports(self):
        """A filename/name mismatch is rejected before any output is written."""
        self.write_role(".codex/agents/worker.toml", "wrong-name")
        self.assertNotEqual(self.run_sync().returncode, 0)
        self.assertFalse((self.root / ".github/agents/coordinator.agent.md").exists())

    def test_duplicate_agent_names_are_rejected(self):
        """Conflicting sources cannot silently overwrite the same picker role."""
        self.write_role(".codex/agents/coordinator.toml", "coordinator")
        self.assertNotEqual(self.run_sync().returncode, 0)

    def test_malformed_skill_description_is_rejected(self):
        """An unquoted colon cannot silently break skill frontmatter discovery."""
        self.write(
            ".agents/skills/example/SKILL.md",
            "---\nname: example\ndescription: Build tasks: tools and tests.\n---\nBody.\n",
        )
        self.assertNotEqual(self.run_sync().returncode, 0)

    def test_skill_directory_name_must_match_metadata(self):
        """A mismatched skill identifier is rejected before export."""
        source = self.root / ".agents/skills/example/SKILL.md"
        source.write_text(source.read_text().replace("name: example", "name: other"))
        self.assertNotEqual(self.run_sync().returncode, 0)

    def test_check_reports_legacy_duplicate_skill_entries(self):
        """Legacy generated wrappers are reported without deleting user files."""
        self.assertEqual(self.run_sync().returncode, 0)
        self.write(
            ".github/skills/example/SKILL.md",
            "<!-- Generated discovery entry for .agents/skills/example/SKILL.md. -->\n",
        )
        self.assertNotEqual(self.run_sync("--check").returncode, 0)
        self.assertTrue((self.root / ".github/skills/example/SKILL.md").exists())

    def test_check_detects_deleted_canonical_agents(self):
        """Removing a worker cannot leave an obsolete agent export undetected."""
        self.assertEqual(self.run_sync().returncode, 0)
        (self.root / ".codex/agents/worker.toml").unlink()
        self.assertNotEqual(self.run_sync("--check").returncode, 0)

    def test_ambiguous_or_malformed_description_values_are_rejected(self):
        """YAML booleans, numbers, comments, and broken quotes are not descriptions."""
        for value in ("true", "null", "123", "# comment", "'broken'quote'", "Build:"):
            with self.subTest(value=value):
                self.write(
                    ".agents/skills/example/SKILL.md",
                    f"---\nname: example\ndescription: {value}\n---\nBody.\n",
                )
                self.assertNotEqual(self.run_sync().returncode, 0)


class WorkspaceAgentEnvironmentTests(unittest.TestCase):
    """Verify the shared VS Code workspace selects and trusts the WSL bridge."""

    def test_workspace_defaults_to_wsl_and_approves_vscode_bridge(self):
        """Agents use Ubuntu WSL while VS Code bridge calls need no confirmation."""
        settings_path = REPOSITORY_ROOT / ".vscode/settings.json"
        settings = json.loads(settings_path.read_text(encoding="utf-8"))

        self.assertEqual(
            settings["terminal.integrated.defaultProfile.windows"],
            "Ubuntu (WSL)",
        )
        self.assertEqual(
            settings["chat.tools.terminal.autoApprove"],
            {"code": True, "code.exe": True},
        )


if __name__ == "__main__":
    unittest.main()
