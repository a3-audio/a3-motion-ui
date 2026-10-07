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
        return {p.relative_to(self.root).as_posix(): p.read_text(errors="replace")
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


class ABadManifest(SeedCase):
    """Review of 0b10d65: a manifest that cannot be read is not a first start.
    It holds the record of what he removed, so the seed adds nothing, replaces
    nothing and leaves the file as it is."""

    def removed_then(self, spoil):
        self.run_seed()
        (self.root / "config/skins/clean.json").unlink()
        manifest = self.root / ".shipped.json"
        spoil(manifest)
        before = manifest.read_bytes() if os.access(manifest, os.R_OK) else None
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn",
                   "config/config.json": "config v2"})
        log = self.run_seed()
        return manifest, before, log

    def assert_untouched(self, manifest, before, log):
        tree = self.tree()
        self.assertNotIn("config/skins/clean.json", tree)
        self.assertNotIn("pattern/clips/system/Dawn.json", tree)
        self.assertEqual('{"patternDir": "pattern"}', tree["config/config.json"])
        lines = [line for line in log if ".shipped.json" in line and "move it aside" in line]
        self.assertEqual(1, len(lines), log)

    def test_a_corrupt_manifest_adds_nothing_and_stays_as_it_is(self):
        manifest, before, log = self.removed_then(lambda m: m.write_bytes(b"\xff{not json"))
        self.assert_untouched(manifest, before, log)
        self.assertEqual(before, manifest.read_bytes())

    def test_a_manifest_that_is_not_a_map_counts_as_corrupt(self):
        manifest, before, log = self.removed_then(lambda m: m.write_text("[1, 2]"))
        self.assert_untouched(manifest, before, log)
        self.assertEqual(before, manifest.read_bytes())

    @unittest.skipIf(os.geteuid() == 0, "root reads everything")
    def test_an_unreadable_manifest_adds_nothing_and_stays_as_it_is(self):
        stamp = {}
        def lock(m):
            stamp["bytes"] = m.read_bytes()
            m.chmod(0o000)
        try:
            manifest, _before, log = self.removed_then(lock)
        finally:
            (self.root / ".shipped.json").chmod(0o644)
        self.assert_untouched(manifest, None, log)
        self.assertEqual(stamp["bytes"], manifest.read_bytes())

    def test_moved_aside_the_next_start_is_a_first_start(self):
        manifest, _before, _log = self.removed_then(lambda m: m.write_text("{bad"))
        manifest.rename(self.root / "shipped.json.bad")
        self.run_seed()
        self.assertEqual("dawn", self.tree()["pattern/clips/system/Dawn.json"])

    def test_a_linked_manifest_is_left_a_link(self):
        self.run_seed()
        elsewhere = self.tmp / "manifest.json"
        manifest = self.root / ".shipped.json"
        elsewhere.write_bytes(manifest.read_bytes())
        manifest.unlink()
        manifest.symlink_to(elsewhere)
        before = elsewhere.read_bytes()
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn"})
        log = self.run_seed()
        self.assertTrue(manifest.is_symlink())
        self.assertEqual(before, elsewhere.read_bytes())
        self.assertTrue(any(".shipped.json" in line and "link" in line for line in log), log)


class LeftoverParts(SeedCase):
    def test_leftover_parts_are_named_once_and_kept(self):
        self.run_seed()
        for folder in ("config/skins", "pattern/clips/system"):
            (self.root / folder / seed.PART_NAME).write_text("cut off")
        (self.root / "pattern/clips/system" / f"{seed.PART_NAME}.1").write_text("cut off")
        log = self.run_seed()
        lines = [line for line in log if "part" in line]
        self.assertEqual(1, len(lines), log)
        self.assertIn("3", lines[0])
        self.assertIn("config/skins", lines[0])
        self.assertTrue((self.root / "config/skins" / seed.PART_NAME).exists())
        self.assertTrue((self.root / "pattern/clips/system" / f"{seed.PART_NAME}.1").exists())

    def test_without_leftovers_nothing_is_said(self):
        self.run_seed()
        log = self.run_seed()
        self.assertEqual([], [line for line in log if "part" in line], log)


class TheHelp(unittest.TestCase):
    def test_the_docstring_says_shipped_copies_are_the_seeds(self):
        self.assertIn("belong to the seed", " ".join(seed.__doc__.split()))

    def test_help_says_it_too(self):
        import contextlib, io
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            seed.main(["--help"])
        self.assertIn("belong to the seed", " ".join(out.getvalue().split()))


class TheMigration(SeedCase):
    def setUp(self):
        super().setUp()
        self.old = self.home / "a3-system/a3-motion/ui"
        files = {
            "config/config.json": '{"patternDir": "pattern", "ui": {"skin": "clean"}}',
            "config/ui_state.json": "{}",
            "config/skins/clean.json": "clean v1",
            "pattern/actions/system/Speed Half.scd": "his edit",
            "pattern/clips/system/Sunset.json": "sunset v1",
            "pattern/clips/.migrated": "",
            "pattern/user/Take.svg": "take",
            "pattern/current.json": "{}",
            "build/a3-motion-ui": "binary",
            "resources/head.svg": "head",
        }
        for rel, text in files.items():
            path = self.old / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        os.utime(self.old / "pattern/user/Take.svg", (1_700_000_000, 1_700_000_000))
        self.checkout = {p.relative_to(self.old).as_posix(): (p.read_bytes(), p.stat().st_mtime_ns)
                         for p in self.old.rglob("*") if p.is_file()}

    def test_his_working_copy_is_copied_byte_for_byte(self):
        self.run_seed()
        for rel, (data, mtime) in self.checkout.items():
            if rel.startswith(("config/", "pattern/")):
                copy = self.root / rel
                self.assertEqual(data, copy.read_bytes(), rel)
                self.assertEqual(mtime, copy.stat().st_mtime_ns, rel)

    def test_the_originals_stay_as_they_were(self):
        self.run_seed()
        now = {p.relative_to(self.old).as_posix(): (p.read_bytes(), p.stat().st_mtime_ns)
               for p in self.old.rglob("*") if p.is_file()}
        self.assertEqual(self.checkout, now)

    def test_build_and_resources_are_not_copied(self):
        self.run_seed()
        self.assertFalse((self.root / "build").exists())
        self.assertFalse((self.root / "resources").exists())

    def test_his_edits_stay_his_with_no_shipped_copy_yet(self):
        log = self.run_seed()
        tree = self.tree()
        self.assertEqual("his edit", tree["pattern/actions/system/Speed Half.scd"])
        self.assertNotIn("pattern/actions/system/Speed Half.scd.shipped", tree)
        self.assertTrue(any("differ from the shipped ones" in line for line in log), log)

    def test_the_next_upgrade_puts_the_new_version_beside_his_edit(self):
        self.run_seed()
        self.ship({**FIRST, "pattern/actions/system/Speed Half.scd": "half v2"})
        self.run_seed()
        tree = self.tree()
        self.assertEqual("his edit", tree["pattern/actions/system/Speed Half.scd"])
        self.assertEqual("half v2", tree["pattern/actions/system/Speed Half.scd.shipped"])

    def test_his_config_stays_and_shipped_files_his_checkout_lacks_are_added(self):
        self.ship({**FIRST, "pattern/clips/system/Dawn.json": "dawn"})
        self.run_seed()
        tree = self.tree()
        self.assertEqual('{"patternDir": "pattern", "ui": {"skin": "clean"}}',
                         tree["config/config.json"])
        self.assertEqual("dawn", tree["pattern/clips/system/Dawn.json"])

    def test_an_existing_folder_is_never_copied_into(self):
        self.root.mkdir(parents=True)
        (self.root / "mine.txt").write_text("mine")
        self.run_seed()
        self.assertEqual("mine", (self.root / "mine.txt").read_text())
        self.assertFalse((self.root / "config/ui_state.json").exists())

    def test_a_copy_cut_off_is_done_again(self):
        part = self.root.with_name(".a3-motion.seed-part")
        (part / "config").mkdir(parents=True)
        (part / "config/config.json").write_text("half")
        self.run_seed()
        self.assertFalse(part.exists())
        self.assertEqual(self.checkout["config/config.json"][0],
                         (self.root / "config/config.json").read_bytes())

    def test_a_link_in_the_checkout_is_not_followed(self):
        (self.old / "pattern/user/outside").symlink_to(self.tmp)
        log = self.run_seed()
        self.assertFalse((self.root / "pattern/user/outside").exists())
        self.assertTrue(any("outside" in line for line in log), log)

    def test_the_loose_clone_of_a3nuc2_is_found(self):
        loose = self.home / "a3-system/a3-motion-ui"
        loose.parent.mkdir(parents=True, exist_ok=True)
        self.old.rename(loose)
        self.run_seed()
        self.assertEqual("take", self.tree()["pattern/user/Take.svg"])

    def test_without_an_old_checkout_it_says_so(self):
        shutil.rmtree(self.home / "a3-system")
        log = self.run_seed()
        self.assertTrue(any("no old checkout" in line for line in log), log)
        self.assertEqual(FIRST["config/skins/clean.json"], self.tree()["config/skins/clean.json"])


class ByHand(SeedCase):
    def make_tree(self, where):
        (where / "config").mkdir(parents=True)
        (where / "config/config.json").write_text('{"patternDir": "pattern"}')
        (where / "pattern/user").mkdir(parents=True)
        (where / "pattern/user/Take.svg").write_text("take")

    def test_from_copies_that_tree(self):
        other = self.tmp / "elsewhere/ui"
        self.make_tree(other)
        code = seed.main([str(self.root), "--shipped", str(self.shipped), "--from", str(other)])
        self.assertEqual(0, code)
        self.assertEqual("take", self.tree()["pattern/user/Take.svg"])

    def test_from_on_an_existing_folder_is_refused(self):
        other = self.tmp / "elsewhere/ui"
        self.make_tree(other)
        self.root.mkdir(parents=True)
        code = seed.main([str(self.root), "--shipped", str(self.shipped), "--from", str(other)])
        self.assertEqual(1, code)
        self.assertEqual([], list(self.root.iterdir()))

    def test_from_without_a_config_is_refused(self):
        other = self.tmp / "empty"
        other.mkdir()
        code = seed.main([str(self.root), "--shipped", str(self.shipped), "--from", str(other)])
        self.assertEqual(1, code)
        self.assertFalse(self.root.exists())

    def test_report_lists_what_differs_and_what_waits(self):
        self.run_seed()
        self.write("pattern/clips/system/Sunset.json", "mine")
        self.ship({**FIRST, "pattern/clips/system/Sunset.json": "sunset v2"})
        self.run_seed()
        lines = seed.report(self.root, self.shipped)
        self.assertIn("differs: pattern/clips/system/Sunset.json", lines)
        self.assertIn("waiting: pattern/clips/system/Sunset.json.shipped", lines)


if __name__ == "__main__":
    unittest.main()
