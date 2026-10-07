"""a3-motion-ui-seed: Motion's data folder kept in step with what the package
ships, as the user, before every start -- copied, never moved, never written
over (spec app-packages, decided 2026-10-07).

A file the performer changed stays; a newer shipped version goes beside it as
<name>.shipped. A file still as the last package shipped it takes the new
version. Nothing is ever deleted."""

import hashlib
import importlib.machinery
import importlib.util
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
_loader = importlib.machinery.SourceFileLoader("a3_motion_ui_seed", str(REPO / "tools/a3-motion-ui-seed"))
_spec = importlib.util.spec_from_loader("a3_motion_ui_seed", _loader)
seed = importlib.util.module_from_spec(_spec)
_loader.exec_module(seed)

FIRST = {
    "config/config.json": '{"patternDir": "pattern"}',
    "config/skins/clean.json": "clean v1",
    "pattern/clips/system/Sunset.json": "sunset v1",
    "pattern/actions/system/Speed Half.scd": "half v1",
}


def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()


class SeedCase(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.shipped = self.tmp / "usr/share/a3-motion-ui"
        self.home = self.tmp / "home"
        self.home.mkdir()
        self.root = self.home / ".local/share/a3-motion"
        self.ship(FIRST)

    def ship(self, files):
        """What a package upgrade does: /usr/share/a3-motion-ui replaced."""
        if self.shipped.exists():
            shutil.rmtree(self.shipped)
        for rel, text in files.items():
            path = self.shipped / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)

    def run_seed(self, source=None):
        log = []
        code = seed.seed(self.root, self.shipped, home=self.home, source=source, log=log.append)
        self.assertEqual(0, code)
        return log

    def tree(self):
        return {p.relative_to(self.root).as_posix(): p.read_text()
                for p in self.root.rglob("*") if p.is_file() and not p.is_symlink()}

    def write(self, rel, text):
        (self.root / rel).write_text(text)


class TheFirstStart(SeedCase):
    def test_the_shipped_files_are_copied_in(self):
        self.run_seed()
        tree = self.tree()
        for rel, text in FIRST.items():
            self.assertEqual(text, tree[rel])

    def test_the_manifest_holds_the_shipped_hashes(self):
        self.run_seed()
        manifest = json.loads((self.root / ".shipped.json").read_text())
        self.assertEqual({rel: digest(text) for rel, text in FIRST.items()}, manifest)

    def test_the_shipped_tree_is_only_read(self):
        before = {p: p.stat().st_mtime_ns for p in self.shipped.rglob("*")}
        self.run_seed()
        self.assertEqual(before, {p: p.stat().st_mtime_ns for p in self.shipped.rglob("*")})

    def test_without_a_shipped_tree_the_exit_is_still_0(self):
        shutil.rmtree(self.shipped)
        log = self.run_seed()
        self.assertTrue(any("a3-motion-ui-seed:" in line for line in log), log)


class AnUpgrade(SeedCase):
    def test_an_untouched_file_takes_the_new_version(self):
        self.run_seed()
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        log = self.run_seed()
        self.assertEqual("clean v2", self.tree()["config/skins/clean.json"])
        self.assertFalse((self.root / "config/skins/clean.json.shipped").exists())
        self.assertTrue(any("updated config/skins/clean.json" in line for line in log), log)

    def test_a_changed_file_stays_and_the_new_one_goes_beside_it(self):
        self.run_seed()
        self.write("pattern/clips/system/Sunset.json", "mine")
        self.ship({**FIRST, "pattern/clips/system/Sunset.json": "sunset v2"})
        log = self.run_seed()
        tree = self.tree()
        self.assertEqual("mine", tree["pattern/clips/system/Sunset.json"])
        self.assertEqual("sunset v2", tree["pattern/clips/system/Sunset.json.shipped"])
        self.assertTrue(any("Sunset.json.shipped" in line for line in log), log)

    def test_a_changed_file_without_a_new_shipped_version_gets_no_copy(self):
        self.run_seed()
        self.write("pattern/clips/system/Sunset.json", "mine")
        self.run_seed()
        self.assertNotIn("pattern/clips/system/Sunset.json.shipped", self.tree())

    def test_a_restart_with_the_same_package_writes_nothing(self):
        self.run_seed()
        self.write("config/config.json", "his")
        times = {p: p.stat().st_mtime_ns for p in self.root.rglob("*")}
        self.run_seed()
        self.assertEqual(times, {p: p.stat().st_mtime_ns for p in self.root.rglob("*")})

    def test_an_older_shipped_copy_is_refreshed(self):
        self.run_seed()
        self.write("config/skins/clean.json", "mine")
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.ship({**FIRST, "config/skins/clean.json": "clean v3"})
        self.run_seed()
        tree = self.tree()
        self.assertEqual("mine", tree["config/skins/clean.json"])
        self.assertEqual("clean v3", tree["config/skins/clean.json.shipped"])

    def test_a_file_new_in_the_package_is_added(self):
        self.run_seed()
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn"})
        self.run_seed()
        self.assertEqual("dawn", self.tree()["pattern/clips/system/Dawn.json"])

    def test_a_file_removed_by_hand_stays_removed(self):
        self.run_seed()
        (self.root / "config/skins/clean.json").unlink()
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertNotIn("config/skins/clean.json", self.tree())

    def test_a_file_the_package_dropped_stays(self):
        self.run_seed()
        dropped = dict(FIRST)
        del dropped["pattern/clips/system/Sunset.json"]
        self.ship(dropped)
        self.run_seed()
        self.assertEqual("sunset v1", self.tree()["pattern/clips/system/Sunset.json"])

    def test_nothing_is_ever_deleted(self):
        self.run_seed()
        self.write("config/config.json", "his")
        (self.root / "pattern/user").mkdir()
        self.write("pattern/user/Take.svg", "take")
        before = set(self.tree())
        for version in ("v2", "v3"):
            self.ship({rel: f"{text} {version}" for rel, text in FIRST.items()})
            self.run_seed()
            self.assertLessEqual(before, set(self.tree()))
        self.assertEqual("his", self.tree()["config/config.json"])
        self.assertEqual("take", self.tree()["pattern/user/Take.svg"])

    def test_a_run_cut_off_leaves_no_stray_shipped_copy(self):
        self.run_seed()
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.write("config/skins/clean.json", "clean v2")  # replaced, manifest not yet written
        self.run_seed()
        self.assertEqual("clean v2", self.tree()["config/skins/clean.json"])
        self.assertNotIn("config/skins/clean.json.shipped", self.tree())

    def test_a_link_in_the_tree_is_left_alone(self):
        self.run_seed()
        elsewhere = self.tmp / "elsewhere.json"
        elsewhere.write_text("not motion's")
        link = self.root / "config/skins/clean.json"
        link.unlink()
        link.symlink_to(elsewhere)
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertTrue(link.is_symlink())
        self.assertEqual("not motion's", elsewhere.read_text())

    def test_an_unreadable_manifest_replaces_nothing(self):
        self.run_seed()
        self.write("pattern/clips/system/Sunset.json", "mine")
        (self.root / ".shipped.json").write_text("{not json")
        self.ship({rel: f"{text} v2" for rel, text in FIRST.items()})
        self.run_seed()
        tree = self.tree()
        self.assertEqual("mine", tree["pattern/clips/system/Sunset.json"])
        self.assertEqual("clean v1", tree["config/skins/clean.json"])
        self.assertNotIn("pattern/clips/system/Sunset.json.shipped", tree)

    @unittest.skipIf(os.geteuid() == 0, "root writes into read-only folders")
    def test_a_file_that_could_not_be_added_is_tried_again(self):
        self.run_seed()
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn"})
        folder = self.root / "pattern/clips/system"
        folder.chmod(0o555)
        try:
            log = self.run_seed()
        finally:
            folder.chmod(0o755)
        self.assertTrue(any("Dawn.json" in line for line in log), log)
        self.run_seed()
        self.assertEqual("dawn", self.tree()["pattern/clips/system/Dawn.json"])


class DataSafety(SeedCase):
    """Lessons from stemdeck-seed's review: copy beside, link into place,
    never overwrite, and keep going past any one bad file."""

    def test_a_file_named_like_the_part_is_never_written_over(self):
        self.run_seed()
        folder = self.root / "pattern/clips/system"
        (folder / seed.PART_NAME).write_text("his")
        (folder / f".Dawn.json.part").write_text("his too")
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn"})
        self.run_seed()
        tree = self.tree()
        self.assertEqual("dawn", tree["pattern/clips/system/Dawn.json"])
        self.assertEqual("his", tree[f"pattern/clips/system/{seed.PART_NAME}"])
        self.assertEqual("his too", tree["pattern/clips/system/.Dawn.json.part"])

    def test_no_part_file_is_left_behind(self):
        self.run_seed()
        self.write("config/skins/clean.json", "mine")
        self.ship({rel: f"{text} v2" for rel, text in FIRST.items()})
        self.run_seed()
        self.assertEqual([], [p for p in self.tree() if "part" in p])

    def test_a_dangling_link_in_the_shipped_tree_is_skipped(self):
        (self.shipped / "config/skins/gone.json").symlink_to(self.tmp / "nowhere")
        self.run_seed()
        tree = self.tree()
        self.assertEqual("clean v1", tree["config/skins/clean.json"])
        self.assertFalse(os.path.lexists(self.root / "config/skins/gone.json"))

    @unittest.skipIf(os.geteuid() == 0, "root reads everything")
    def test_an_unreadable_shipped_file_is_logged_and_the_rest_go_in(self):
        unreadable = self.shipped / "config/skins/clean.json"
        unreadable.chmod(0o000)
        try:
            log = self.run_seed()
        finally:
            unreadable.chmod(0o644)
        self.assertTrue(any("clean.json" in line for line in log), log)
        self.assertEqual("sunset v1", self.tree()["pattern/clips/system/Sunset.json"])
        self.run_seed()
        self.assertEqual("clean v1", self.tree()["config/skins/clean.json"])

    @unittest.skipIf(os.geteuid() == 0, "root reads everything")
    def test_an_unreadable_shipped_folder_forgets_no_removal(self):
        self.run_seed()
        (self.root / "config/skins/clean.json").unlink()
        folder = self.shipped / "config/skins"
        folder.chmod(0o000)
        try:
            self.run_seed()
        finally:
            folder.chmod(0o755)
        self.run_seed()
        self.assertNotIn("config/skins/clean.json", self.tree())

    def test_a_linked_folder_in_the_tree_is_not_written_through(self):
        self.run_seed()
        elsewhere = self.tmp / "elsewhere"
        elsewhere.mkdir()
        skins = self.root / "config/skins"
        shutil.rmtree(skins)
        skins.symlink_to(elsewhere)
        self.ship({**FIRST, "config/skins/new.json": "new", "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertEqual([], list(elsewhere.iterdir()))

    def test_a_link_where_the_shipped_copy_goes_is_left_alone(self):
        self.run_seed()
        self.write("config/skins/clean.json", "mine")
        elsewhere = self.tmp / "elsewhere.json"
        elsewhere.write_text("not motion's")
        (self.root / "config/skins/clean.json.shipped").symlink_to(elsewhere)
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertTrue((self.root / "config/skins/clean.json.shipped").is_symlink())
        self.assertEqual("not motion's", elsewhere.read_text())
        self.assertEqual("mine", self.tree()["config/skins/clean.json"])

    def test_an_undecodable_manifest_does_not_stop_the_start(self):
        self.run_seed()
        (self.root / ".shipped.json").write_bytes(b"\xff\xfe{")
        self.write("config/skins/clean.json", "mine")
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertEqual("mine", self.tree()["config/skins/clean.json"])

    def test_an_error_that_is_not_an_oserror_still_exits_0(self):
        original = seed.reconcile
        seed.reconcile = lambda *args: 1 / 0
        try:
            log = self.run_seed()
        finally:
            seed.reconcile = original
        self.assertTrue(any("ZeroDivisionError" in line for line in log), log)

    def test_a_log_that_fails_does_not_stop_the_start(self):
        def broken(line):
            raise UnicodeEncodeError("ascii", line, 0, 1, "strict")
        self.assertEqual(0, seed.seed(self.root, self.shipped, home=self.home, log=broken))
        self.assertEqual("clean v1", self.tree()["config/skins/clean.json"])

    def test_undecodable_file_names_do_not_break_a_strict_log(self):
        name = os.fsdecode(b"Caf\xe9.json")
        (self.shipped / "pattern/clips/system" / name).write_text("cafe")
        log = []
        def strict(line):
            line.encode("utf-8")
            log.append(line)
        self.assertEqual(0, seed.seed(self.root, self.shipped, home=self.home, log=strict))
        self.assertTrue(any("Caf" in line for line in log), log)
        self.assertEqual("cafe", (self.root / "pattern/clips/system" / name).read_text())
        self.run_seed()  # the manifest with that name reads back

    def test_copies_and_the_manifest_are_synced_to_disk(self):
        synced = []
        original = os.fsync
        def spy(fd):
            synced.append(os.readlink(f"/proc/self/fd/{fd}"))
            return original(fd)
        seed.os.fsync = spy
        try:
            self.run_seed()
        finally:
            seed.os.fsync = original
        self.assertEqual(len(FIRST) + 1, len(synced), synced)

    def test_main_exits_0_when_the_root_cannot_be_made(self):
        blocker = self.tmp / "file"
        blocker.write_text("x")
        self.assertEqual(0, seed.main([str(blocker / "root"), "--shipped", str(self.shipped)]))

    def test_report_names_what_differs_and_what_waits(self):
        self.run_seed()
        self.write("config/skins/clean.json", "mine")
        self.ship({**FIRST, "config/skins/clean.json": "clean v2"})
        self.run_seed()
        self.assertEqual(["differs: config/skins/clean.json",
                          "waiting: config/skins/clean.json.shipped"],
                         seed.report(self.root, self.shipped))

    def test_a_first_start_over_his_files_says_so_once(self):
        self.root.mkdir(parents=True)
        (self.root / "config/skins").mkdir(parents=True)
        self.write("config/skins/clean.json", "his")
        self.write("config/config.json", "his")
        log = self.run_seed()
        self.assertEqual(1, sum("differ" in line for line in log), log)
        self.assertEqual("his", self.tree()["config/skins/clean.json"])
        self.assertNotIn("config/skins/clean.json.shipped", self.tree())


if __name__ == "__main__":
    unittest.main()
