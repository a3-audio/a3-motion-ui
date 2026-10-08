"""tools/library-v2/sets.py: regenerating the night's sets gives back the files that
ship, byte for byte, and leaves every set it does not make alone (the mood sets
Acid, Ambient, Tension and Tribal are written by hand)."""

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("library_sets", REPO / "tools/library-v2/sets.py")
sets = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sets)

SETS = Path("sessions/system")


class RegeneratingTheSets(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        self.root = self.tmp / "pattern"
        shutil.copytree(REPO / "pattern", self.root)

    def test_gives_back_every_set_it_makes_byte_for_byte(self):
        sets.main(str(self.root))
        for name, *_ in sets.SETS:
            with self.subTest(set=name):
                shipped = (REPO / "pattern" / SETS / f"{name}.json").read_bytes()
                made = (self.root / SETS / f"{name}.json").read_bytes()
                self.assertEqual(shipped, made)

    def test_leaves_the_sets_it_does_not_make(self):
        made = {name for name, *_ in sets.SETS}
        others = [p.name for p in (REPO / "pattern" / SETS).glob("*.json") if p.stem not in made]
        self.assertTrue(others, "expected hand-made sets beside the generated ones")
        sets.main(str(self.root))
        for name in others:
            with self.subTest(set=name):
                self.assertTrue((self.root / SETS / name).exists())


if __name__ == "__main__":
    unittest.main()
