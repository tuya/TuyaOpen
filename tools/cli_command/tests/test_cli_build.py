#!/usr/bin/env python3
# coding=utf-8
import os
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock, patch


def _touch(path):
    with open(path, "w") as f:
        f.write("x")


class TestPruneStaleBins(unittest.TestCase):
    '''
    _prune_stale_bins keeps dist/<app>_<ver>/ single-version: .build/bin
    artifacts carry the version in their name, so a version bump adds new
    files next to the old ones instead of overwriting them.
    '''

    def setUp(self):
        import tools.cli_command.cli_build as m
        self.prune = m._prune_stale_bins
        self.bin_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.bin_dir, True)
        self.logger_patch = patch(
            'tools.cli_command.cli_build.get_logger',
            return_value=MagicMock())
        self.logger_patch.start()
        self.addCleanup(self.logger_patch.stop)

    def _names(self):
        return sorted(os.listdir(self.bin_dir))

    def _make(self, *names):
        for name in names:
            _touch(os.path.join(self.bin_dir, name))

    def test_current_version_kept(self):
        self._make("switch_demo_QIO_1.0.1.bin",
                   "switch_demo_UA_1.0.1.bin",
                   "switch_demo_UG_1.0.1.bin")
        dropped = self.prune(self.bin_dir, "switch_demo", "1.0.1")
        self.assertEqual(dropped, [])
        self.assertEqual(len(self._names()), 3)

    def test_stale_version_dropped(self):
        self._make("switch_demo_QIO_1.0.0.bin",
                   "switch_demo_UA_1.0.0.bin",
                   "switch_demo_UG_1.0.0.bin",
                   "switch_demo_QIO_1.0.1.bin")
        dropped = self.prune(self.bin_dir, "switch_demo", "1.0.1")
        self.assertEqual(len(dropped), 3)
        self.assertEqual(self._names(), ["switch_demo_QIO_1.0.1.bin"])

    def test_stale_app_name_dropped(self):
        self._make("old_demo_QIO_1.0.1.bin", "switch_demo_QIO_1.0.1.bin")
        self.prune(self.bin_dir, "switch_demo", "1.0.1")
        self.assertEqual(self._names(), ["switch_demo_QIO_1.0.1.bin"])

    def test_non_semver_version_matched(self):
        self._make("switch_demo_UG_1.0.1-rc1.bin",
                   "switch_demo_UG_1.0.2-rc1.bin")
        self.prune(self.bin_dir, "switch_demo", "1.0.2-rc1")
        self.assertEqual(self._names(), ["switch_demo_UG_1.0.2-rc1.bin"])

    def test_unrelated_entries_untouched(self):
        os.makedirs(os.path.join(self.bin_dir, "debug"))
        self._make("bootloader.bin", "notes.txt",
                   "switch_demo_QIO_1.0.0.bin")
        dropped = self.prune(self.bin_dir, "switch_demo", "1.0.1")
        self.assertEqual(dropped, ["switch_demo_QIO_1.0.0.bin"])
        self.assertEqual(self._names(),
                         ["bootloader.bin", "debug", "notes.txt"])

    def test_missing_dir_is_noop(self):
        self.assertEqual(
            self.prune(os.path.join(self.bin_dir, "nope"), "a", "1"), [])


if __name__ == '__main__':
    unittest.main()
