"""tools/a3-offline-truth: the OSC truth with every host but local and any
moved to 192.0.2.x (TEST-NET-1, routes nowhere), so the suite cannot reach
the live Core (#67). Used by test.sh and packaging/stage."""

import importlib.machinery
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools/a3-offline-truth"
_loader = importlib.machinery.SourceFileLoader("a3_offline_truth", str(TOOL))
_spec = importlib.util.spec_from_loader("a3_offline_truth", _loader)
offline = importlib.util.module_from_spec(_spec)
_loader.exec_module(offline)

TRUTH = {"hosts": {"local": "127.0.0.1", "core": "192.168.8.10", "any": "0.0.0.0",
                   "mixer": "192.168.8.11"},
         "addresses": {"vu": {"pattern": "/vu/{n}"}}, "ports": {"motion": 9001}}


class TheOfflineTruth(unittest.TestCase):
    def test_every_named_host_moves_to_the_documentation_range_in_order(self):
        moved = offline.offline(TRUTH)
        self.assertEqual({"local": "127.0.0.1", "core": "192.0.2.10", "any": "0.0.0.0",
                          "mixer": "192.0.2.11"}, moved["hosts"])

    def test_everything_else_is_as_it_was(self):
        moved = offline.offline(TRUTH)
        self.assertEqual(TRUTH["addresses"], moved["addresses"])
        self.assertEqual(TRUTH["ports"], moved["ports"])

    def test_the_source_is_not_changed(self):
        before = json.dumps(TRUTH, sort_keys=True)
        offline.offline(TRUTH)
        self.assertEqual(before, json.dumps(TRUTH, sort_keys=True))

    def test_the_command_writes_the_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, target = Path(tmp) / "a3-osc.json", Path(tmp) / "offline.json"
            source.write_text(json.dumps(TRUTH))
            subprocess.run([sys.executable, "-I", TOOL, source, target], check=True)
            self.assertEqual("192.0.2.10", json.loads(target.read_text())["hosts"]["core"])


if __name__ == "__main__":
    unittest.main()
