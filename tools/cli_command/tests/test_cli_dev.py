#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for the backup/restore logic added to
# tools/cli_command/cli_dev.py::build_all_config_exec (B12 fix).
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_cli_dev -v

import os
import tempfile
import unittest
from unittest.mock import patch, MagicMock


class BuildAllConfigBase(unittest.TestCase):
    '''
    Drives build_all_config_exec.callback() directly against a throwaway
    tree of two candidate configs, with build_project/full_clean_project/
    init_using_config mocked out (no toolchain involved) so only the
    backup/restore logic around app_default.config is under test.
    '''

    def setUp(self):
        import tools.cli_command.cli_dev as m
        self.m = m

        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.app_configs_path = os.path.join(self._tmp.name, "configs")
        os.makedirs(self.app_configs_path)
        self.app_default_config = os.path.join(
            self._tmp.name, "app_default.config")
        self.backup_path = self.app_default_config + ".bac_backup"

        for name, content in (("a.config", "CONFIG_A=y\n"),
                             ("b.config", "CONFIG_B=y\n")):
            with open(os.path.join(self.app_configs_path, name),
                     'w', encoding='utf-8') as f:
                f.write(content)

        self.params = {
            "app_configs_path": self.app_configs_path,
            "app_default_config": self.app_default_config,
            "boards_root": os.path.join(self._tmp.name, "boards"),
        }

        self._patches = [
            patch('tools.cli_command.cli_dev.get_logger',
                 return_value=MagicMock()),
            patch('tools.cli_command.cli_dev.get_global_params',
                 return_value=self.params),
            patch('tools.cli_command.cli_dev.full_clean_project'),
            patch('tools.cli_command.cli_dev.init_using_config'),
            patch('tools.cli_command.cli_dev.build_project',
                 return_value=True),
        ]
        for p in self._patches:
            p.start()
            self.addCleanup(p.stop)

    def run_bac(self, force_clean_backup=False):
        try:
            self.m.build_all_config_exec.callback(
                dist="", log_dir=None, skip=(),
                force_clean_backup=force_clean_backup)
        except SystemExit as e:
            return e.code
        return None

    def write_app_default(self, content):
        with open(self.app_default_config, 'w', encoding='utf-8') as f:
            f.write(content)

    def read_app_default(self):
        with open(self.app_default_config, encoding='utf-8') as f:
            return f.read()


class TestBackupRestore(BuildAllConfigBase):

    def test_original_app_default_is_restored_after_run(self):
        self.write_app_default("CONFIG_ORIGINAL=y\n")
        code = self.run_bac()
        self.assertEqual(code, 0)
        self.assertEqual(self.read_app_default(), "CONFIG_ORIGINAL=y\n")
        self.assertFalse(os.path.exists(self.backup_path))

    def test_brand_new_project_ends_with_no_app_default(self):
        # app_default.config does not exist before this run: `bac` must
        # return to that same "absent" state, not leave the last test
        # config behind (the original B12 bug's behaviour).
        self.assertFalse(os.path.exists(self.app_default_config))
        code = self.run_bac()
        self.assertEqual(code, 0)
        self.assertFalse(os.path.exists(self.app_default_config))
        self.assertFalse(os.path.exists(self.backup_path))

    def test_a_build_failure_still_restores(self):
        with patch('tools.cli_command.cli_dev.build_project',
                   return_value=False):
            self.write_app_default("CONFIG_ORIGINAL=y\n")
            code = self.run_bac()
        self.assertEqual(code, 1)  # exit_flag, not the guard's sys.exit(1)
        self.assertEqual(self.read_app_default(), "CONFIG_ORIGINAL=y\n")


class TestLeftoverBackupGuard(BuildAllConfigBase):
    '''
    A leftover .bac_backup means a previous `bac` run was killed mid-way
    (SIGKILL, power loss) before its `finally` block could restore and
    clean up. Neither file may be touched by default: today's
    app_default.config may already be mid-loop test content, and blindly
    overwriting the leftover backup with it would destroy the last copy
    of the user's real original config.
    '''

    def test_leftover_backup_refuses_by_default(self):
        self.write_app_default("CONFIG_MID_LOOP=y\n")
        with open(self.backup_path, 'w', encoding='utf-8') as f:
            f.write("CONFIG_REAL_ORIGINAL=y\n")

        code = self.run_bac(force_clean_backup=False)

        self.assertEqual(code, 1)
        self.assertEqual(self.read_app_default(), "CONFIG_MID_LOOP=y\n")
        with open(self.backup_path, encoding='utf-8') as f:
            self.assertEqual(f.read(), "CONFIG_REAL_ORIGINAL=y\n")

    def test_force_clean_backup_discards_leftover_and_proceeds(self):
        self.write_app_default("CONFIG_MID_LOOP=y\n")
        with open(self.backup_path, 'w', encoding='utf-8') as f:
            f.write("CONFIG_STALE=y\n")

        code = self.run_bac(force_clean_backup=True)

        self.assertEqual(code, 0)
        # The leftover backup was discarded, so what gets restored is
        # today's (pre-loop) app_default.config -- not the stale backup.
        self.assertEqual(self.read_app_default(), "CONFIG_MID_LOOP=y\n")
        self.assertFalse(os.path.exists(self.backup_path))


if __name__ == "__main__":
    unittest.main()
