"""platform_config/a3-motion.service: the unit the a3-motion-ui package
installs to /usr/lib/systemd/user. It starts what the package ships, in the
user's data folder, seeds that folder first, and starts again after a
failure (#69)."""

import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNIT = REPO / "platform_config/a3-motion.service"
SEED = "/usr/lib/a3-motion-ui/a3-motion-ui-seed"
DATA = "%h/.local/share/a3-motion"


def entries(text):
    found = []
    for line in text.splitlines():
        line = line.strip()
        if line and not line.startswith(("#", ";", "[")) and "=" in line:
            key, _, value = line.partition("=")
            found.append((key.strip(), value.strip()))
    return found


class ThePackagedUnit(unittest.TestCase):
    def setUp(self):
        self.text = UNIT.read_text()
        self.entries = entries(self.text)

    def values(self, key):
        return [value for k, value in self.entries if k == key]

    def test_it_starts_the_packaged_binary(self):
        self.assertEqual(["/usr/bin/a3-motion-ui"], self.values("ExecStart"))

    def test_it_starts_again_after_a_failure(self):
        """A new truth from Core ends Motion with 1 so systemd starts it on
        the new one (a3-system#74); without Restart= the screen stays dark (#69)."""
        self.assertEqual(["on-failure"], self.values("Restart"))
        self.assertEqual(["5"], self.values("RestartSec"))

    def test_it_runs_in_the_users_data_folder(self):
        self.assertEqual([f"-{DATA}"], self.values("WorkingDirectory"))

    def test_the_seed_runs_first_and_cannot_block_the_start(self):
        pre = self.values("ExecStartPre")
        self.assertEqual(f"-{SEED} {DATA}", pre[0])
        self.assertEqual("/usr/lib/a3-motion-ui/a3-wait-for-the-screen", pre[1])
        self.assertEqual(2, len(pre))

    def test_the_seed_and_the_working_directory_name_one_folder(self):
        seeded = self.values("ExecStartPre")[0].split()[-1]
        self.assertEqual(seeded, self.values("WorkingDirectory")[0].lstrip("-"))

    def test_it_has_its_display_and_its_cpu(self):
        self.assertEqual(["DISPLAY=:0"], self.values("Environment"))
        self.assertEqual(["0"], self.values("CPUAffinity"))

    def test_it_belongs_to_the_rig_and_starts_with_the_session(self):
        self.assertEqual(["a3-main.service"], self.values("PartOf"))
        self.assertEqual(["default.target"], self.values("WantedBy"))

    def test_nothing_names_a_checkout_a_home_or_the_old_script(self):
        for key, value in self.entries:
            with self.subTest(key=key):
                self.assertNotIn("/home/", value)
                self.assertNotIn("a3-system", value)
                self.assertNotIn("/build/", value)
                self.assertNotEqual("/usr/bin/a3-motion", value)


if __name__ == "__main__":
    unittest.main()
