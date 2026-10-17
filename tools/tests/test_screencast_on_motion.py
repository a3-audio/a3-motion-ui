# SPDX-FileCopyrightText: 2026 Patric Schmitz, Raphael Eismann
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""Motion's screen as a screencast on a3nuc2, which has no a3-core.

The package ships a vendored copy of a3-core's a3-screencast and its fan-out,
opt-in; a3-motion-screencast finds the port in the OSC truth Motion reads and
starts the copy."""

import importlib.machinery
import importlib.util
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CONFIG = REPO / "platform_config"
WRAPPER = CONFIG / "a3-motion-screencast"
UNIT = CONFIG / "a3-motion-screencast.service"
STAGE = REPO / "packaging/stage"
CORE_TREE = "platform-config/debian-x86_64/a3-core/home/aaa/.local"
LIB = "usr/lib/a3-motion-ui"


def load_wrapper():
    loader = importlib.machinery.SourceFileLoader("a3_motion_screencast", str(WRAPPER))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


def core_checkout():
    given = os.environ.get("A3_CORE_CHECKOUT")
    return Path(given) if given else (REPO / "../../a3-core").resolve()


class TheVendoredCopy(unittest.TestCase):
    def test_the_vendored_files_equal_a3_cores(self):
        core = core_checkout()
        pairs = (("a3-screencast", "bin/a3-screencast"),
                 ("a3_screencast_fanout.py", "lib/a3_screencast_fanout.py"))
        if not all((core / CORE_TREE / theirs).is_file() for _, theirs in pairs):
            self.skipTest(f"no a3-core with the screencast beside the checkout ({core})")
        for ours, theirs in pairs:
            with self.subTest(file=ours):
                self.assertEqual((core / CORE_TREE / theirs).read_bytes(),
                                 (CONFIG / "screencast" / ours).read_bytes())


class TheTruth(unittest.TestCase):
    def setUp(self):
        self.module = load_wrapper()

    def test_the_port_comes_from_the_truth(self):
        with tempfile.TemporaryDirectory() as tmp:
            truth = Path(tmp) / "a3-osc.json"
            truth.write_text(json.dumps({"listeners": [
                {"name": "core.osc", "port": 9000},
                {"name": "screencast.stream", "program": "screencast",
                 "role": "stream", "port": 4321}]}))
            self.assertEqual(4321, self.module.port_from(truth))

    def test_a_truth_without_the_listener_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            truth = Path(tmp) / "a3-osc.json"
            truth.write_text(json.dumps({"listeners": []}))
            with self.assertRaises(SystemExit):
                self.module.port_from(truth)

    def test_the_truth_is_found_in_motions_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp)
            cache = home / ".cache/a3/a3-osc.json"
            given = home / "given.json"
            installed = Path("/usr/share/a3/a3-osc.json")
            truth_path = self.module.truth_path
            self.assertEqual(installed, truth_path({}, home))
            cache.parent.mkdir(parents=True)
            cache.write_text("{}")
            self.assertEqual(cache, truth_path({}, home))
            given.write_text("{}")
            self.assertEqual(given, truth_path({"A3_OSC_TRUTH": str(given)}, home))

    def test_without_a_truth_it_says_so_and_exits_1(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = {"PATH": os.environ["PATH"], "HOME": tmp}
            if Path("/usr/share/a3/a3-osc.json").exists():
                self.skipTest("this machine has the installed truth")
            done = subprocess.run(["python3", "-I", str(WRAPPER)], env=env,
                                  capture_output=True, text=True)
            self.assertEqual(1, done.returncode)
            self.assertIn("no a3-osc.json yet", done.stderr)


class TheCopyFindsItsFanOut(unittest.TestCase):
    """a3-screencast takes the fan-out path from A3_SCREENCAST_FANOUT; on
    Motion the module lives in /usr/lib/a3-motion-ui."""

    def test_the_environment_names_the_packaged_fan_out(self):
        module = load_wrapper()
        env = module.environment({"HOME": "/home/dj"}, 4321)
        self.assertEqual("/usr/lib/a3-motion-ui/a3_screencast_fanout.py",
                         env["A3_SCREENCAST_FANOUT"])
        self.assertEqual("4321", env["A3_SCREENCAST_PORT"])

    def test_home_and_the_x_environment_stay_the_real_ones(self):
        module = load_wrapper()
        given = {"HOME": "/home/dj", "DISPLAY": ":0", "XAUTHORITY": "/x/auth"}
        env = module.environment(given, 1)
        for key, value in given.items():
            self.assertEqual(value, env[key])
        self.assertNotIn("XAUTHORITY", module.environment({"HOME": "/home/dj"}, 1))


class TheUnit(unittest.TestCase):
    def setUp(self):
        self.text = UNIT.read_text()

    def lines(self):
        return [line.strip() for line in self.text.splitlines()]

    def test_the_unit_restarts(self):
        for wanted in ("Restart=always", "RestartSec=2", "StartLimitIntervalSec=0",
                       "Environment=DISPLAY=:0",
                       "ExecStartPre=/usr/lib/a3-motion-ui/a3-wait-for-the-screen"):
            with self.subTest(line=wanted):
                self.assertIn(wanted, self.lines())

    def test_it_starts_the_wrapper(self):
        self.assertIn("ExecStart=/usr/lib/a3-motion-ui/a3-motion-screencast", self.lines())

    def test_the_settings_file_is_optional(self):
        self.assertIn("EnvironmentFile=-%h/.config/a3/screencast.env", self.lines())

    def test_it_is_opt_in_by_systemctl_enable(self):
        self.assertIn("WantedBy=default.target", self.lines())

    def test_the_package_does_not_enable_it(self):
        for name in ("postinst", "prerm", "postrm"):
            with self.subTest(script=name):
                self.assertNotIn("a3-motion-screencast",
                                 (REPO / "packaging/DEBIAN" / name).read_text())


class TheStage(unittest.TestCase):
    def test_stage_ships_the_screencast(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / "build"
            binary = build / "src/a3-motion-ui/a3-motion-ui_artefacts/Release/Standalone/a3-motion-ui"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"x")
            stage = Path(tmp) / "stage"
            stage.mkdir()
            subprocess.run([STAGE, "files", REPO, build, stage], check=True)
            modes = {"a3-motion-screencast": 0o755, "a3-screencast": 0o755,
                     "a3_screencast_fanout.py": 0o644}
            for name, mode in modes.items():
                with self.subTest(file=name):
                    path = stage / LIB / name
                    self.assertTrue(path.is_file())
                    self.assertEqual(mode, path.stat().st_mode & 0o777)
            unit = stage / "usr/lib/systemd/user/a3-motion-screencast.service"
            self.assertEqual(UNIT.read_bytes(), unit.read_bytes())

    def test_ffmpeg_is_a_dependency(self):
        text = (REPO / "packaging/DEBIAN/control").read_text()
        depends = next(l for l in text.splitlines() if l.startswith("Depends:"))
        self.assertIn("ffmpeg", [d.strip() for d in depends.partition(":")[2].split(",")])


if __name__ == "__main__":
    unittest.main()
