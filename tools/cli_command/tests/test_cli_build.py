#!/usr/bin/env python3
# coding=utf-8
import os
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from click.testing import CliRunner


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


class TestPrepareOnly(unittest.TestCase):
    def test_prepare_only_runs_setup_without_compiling(self):
        from tools.cli_command import cli_build
        config = {
            'CONFIG_PLATFORM_CHOICE': 'ESP32',
            'CONFIG_CHIP_CHOICE': 'esp32p4',
            'CONFIG_PROJECT_NAME': 'switch_demo',
            'CONFIG_FRAMEWORK_CHOICE': 'base',
        }
        with patch('tools.cli_command.cli_prepare.ensure_build_env', return_value=True), \
             patch.multiple(cli_build, check_proj_dir=MagicMock(), export_country_code=MagicMock(),
                            env_check=MagicMock(return_value=True), init_using_config=MagicMock(),
                            get_global_params=MagicMock(return_value={'using_config': 'using.config'}),
                            parse_config_file=MagicMock(return_value=config),
                            download_platform=MagicMock(return_value=True),
                            prepare_platform=MagicMock(return_value=True),
                            build_setup=MagicMock(return_value=True), cmake_configure=MagicMock(),
                            ninja_build=MagicMock(), check_bin_file=MagicMock()):
            result = CliRunner().invoke(cli_build.cli, ['--prepare-only'])
            self.assertEqual(result.exit_code, 0, result.exception)
            cli_build.download_platform.assert_called_once_with('ESP32')
            cli_build.prepare_platform.assert_called_once_with('ESP32', 'esp32p4')
            cli_build.build_setup.assert_called_once_with('ESP32', 'switch_demo', 'base', 'esp32p4')
            cli_build.cmake_configure.assert_not_called()
            cli_build.ninja_build.assert_not_called()
            cli_build.check_bin_file.assert_not_called()
            cli_build.build_setup.return_value = False
            result = CliRunner().invoke(cli_build.cli, ['--prepare-only'])
            self.assertEqual(result.exit_code, 1, result.exception)
            cli_build.cmake_configure.assert_not_called()


if __name__ == '__main__':
    unittest.main()
