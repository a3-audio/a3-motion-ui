"""The a3-motion-ui package: control, maintainer scripts, staged tree.

The package writes only to /usr. postinst enables the user unit for every
user and does nothing else -- no write into any home, no restart (spec
app-packages). User data stays on purge."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEBIAN = REPO / "packaging/DEBIAN"
STAGE = REPO / "packaging/stage"
UNIT = REPO / "platform_config/a3-motion.service"
SHARE = "usr/share/a3-motion-ui"
BINARY = "src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui"
EXTRA_DEPENDS = ("x11-utils", "x11-xserver-utils", "python3")
RECOMMENDS = ("onboard", "dbus-bin", "i3")
TOOLS = ("a3-wait-for-the-screen", "a3-motion-ui-seed")


def fields(text):
    found = {}
    for line in text.splitlines():
        if line and not line[0].isspace() and ":" in line:
            key, _, value = line.partition(":")
            found[key] = value.strip()
    return found


def code_lines(path):
    return [line for line in path.read_text().splitlines()
            if line.strip() and not line.strip().startswith(("#", "echo"))]


def listed(value):
    return [item.strip() for item in value.split(",")]


class TheControlFile(unittest.TestCase):
    def setUp(self):
        self.text = (DEBIAN / "control").read_text()
        self.fields = fields(self.text)

    def test_name_architecture_and_a_version_to_stamp(self):
        self.assertEqual("a3-motion-ui", self.fields["Package"])
        self.assertEqual("amd64", self.fields["Architecture"])
        self.assertIn("Version", self.fields)

    def test_depends_are_the_binarys_libraries_and_the_units_tools(self):
        depends = listed(self.fields["Depends"])
        self.assertEqual("${shlibs:Depends}", depends[0])
        self.assertEqual(set(EXTRA_DEPENDS), set(depends[1:]))

    def test_what_only_some_fields_need_is_recommended(self):
        self.assertEqual(set(RECOMMENDS), set(listed(self.fields["Recommends"])))

    def test_it_does_not_need_the_core(self):
        """a3nuc2 runs Motion without a Core and must not get REAPER and i3's config."""
        self.assertNotIn("a3-core", self.text)

    def test_it_says_user_data_stays_and_how_it_is_seeded(self):
        self.assertIn("User data stays", self.text)
        self.assertIn(".shipped", self.text)


class TheMaintainerScripts(unittest.TestCase):
    def test_they_are_executable(self):
        for name in ("postinst", "prerm", "postrm"):
            with self.subTest(script=name):
                self.assertTrue(os.access(DEBIAN / name, os.X_OK))

    def test_postinst_enables_for_every_user(self):
        self.assertIn("systemctl --global enable a3-motion.service", (DEBIAN / "postinst").read_text())

    def test_postinst_writes_into_no_home_and_restarts_nothing(self):
        for line in code_lines(DEBIAN / "postinst"):
            for forbidden in ("restart", "/home", "$HOME", "sudo", "--user enable",
                              "--user start", "cp ", "mv ", "mkdir", "rm ", "chown"):
                with self.subTest(line=line, forbidden=forbidden):
                    self.assertNotIn(forbidden, line)

    def test_prerm_disables_on_remove_and_deconfigure_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "calls"
            fake = Path(tmp) / "systemctl"
            fake.write_text(f'#!/bin/sh\necho "$@" >> {log}\n')
            fake.chmod(0o755)
            env = {**os.environ, "PATH": f"{tmp}:{os.environ['PATH']}"}
            for event, called in (("upgrade", False), ("failed-upgrade", False),
                                  ("remove", True), ("deconfigure", True)):
                log.unlink(missing_ok=True)
                subprocess.run([DEBIAN / "prerm", event], check=True, env=env)
                with self.subTest(event=event):
                    self.assertEqual(called, log.exists())
                    if called:
                        self.assertIn("--global disable a3-motion.service", log.read_text())

    def test_postrm_calls_nothing_and_removes_nothing(self):
        """By postrm the unit file is gone; a disable there leaves dangling links."""
        self.assertNotIn("systemctl", (DEBIAN / "postrm").read_text())
        for line in code_lines(DEBIAN / "postrm"):
            self.assertNotIn("rm ", line)


class TheStagedTree(unittest.TestCase):
    def stage(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        build = Path(tmp.name) / "build"
        binary = build / BINARY
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"\x7fELF not really")
        stage = Path(tmp.name) / "stage"
        stage.mkdir()
        subprocess.run([STAGE, "files", REPO, build, stage], check=True)
        return stage

    def files(self, stage):
        return {p.relative_to(stage).as_posix() for p in stage.rglob("*") if p.is_file()}

    def expected(self):
        shipped = {f"{SHARE}/{p.relative_to(REPO).as_posix()}"
                   for part in ("resources", "pattern")
                   for p in (REPO / part).rglob("*") if p.is_file()}
        shipped |= {f"{SHARE}/config/skins/{p.name}" for p in (REPO / "config/skins").glob("*.json")}
        return shipped | {
            "usr/bin/a3-motion-ui",
            *(f"usr/lib/a3-motion-ui/{tool}" for tool in TOOLS),
            "usr/lib/systemd/user/a3-motion.service",
            f"{SHARE}/config/config.json",
            "usr/share/doc/a3-motion-ui/copyright",
        }

    def test_exactly_these_files(self):
        self.assertEqual(self.expected(), self.files(self.stage()))

    def test_the_users_state_is_never_shipped(self):
        files = self.files(self.stage())
        self.assertNotIn(f"{SHARE}/config/ui_state.json", files)

    def test_programs_are_executable_and_data_is_not(self):
        stage = self.stage()
        for name in self.files(stage):
            mode = (stage / name).stat().st_mode & 0o777
            program = name.startswith(("usr/bin/", "usr/lib/a3-motion-ui/"))
            with self.subTest(file=name):
                self.assertEqual(0o755 if program else 0o644, mode)

    def test_folders_are_world_readable(self):
        stage = self.stage()
        for folder in (p for p in stage.rglob("*") if p.is_dir()):
            with self.subTest(folder=str(folder.relative_to(stage))):
                self.assertEqual(0o755, folder.stat().st_mode & 0o777)

    def test_the_unit_is_the_repositorys(self):
        stage = self.stage()
        self.assertEqual(UNIT.read_bytes(),
                         (stage / "usr/lib/systemd/user/a3-motion.service").read_bytes())

    def test_every_program_the_unit_starts_is_in_the_tree(self):
        stage = self.stage()
        for line in UNIT.read_text().splitlines():
            key, _, value = line.partition("=")
            if not key.startswith("Exec"):
                continue
            for word in value.lstrip("-@:+!").split():
                if word.startswith(("/usr/lib/a3-motion-ui/", "/usr/bin/a3-motion-ui")):
                    with self.subTest(program=word):
                        self.assertTrue((stage / word.lstrip("/")).is_file())

    def test_the_seed_reads_where_the_package_puts_it(self):
        text = (REPO / "tools/a3-motion-ui-seed").read_text()
        self.assertIn(f'SHIPPED = Path("/{SHARE}")', text)

    def test_an_unknown_extra_argument_is_refused(self):
        done = subprocess.run([STAGE, "files", REPO, "b", "s", "extra"], capture_output=True)
        self.assertNotEqual(0, done.returncode)

    def test_an_unknown_step_is_refused(self):
        self.assertEqual(2, subprocess.run([STAGE, "nonsense"], capture_output=True).returncode)


if __name__ == "__main__":
    unittest.main()
