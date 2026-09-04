#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for the non-exiting helpers in
# tools/cli_command/cli_config.py
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_cli_config -v

import os
import tempfile
import unittest
from unittest.mock import patch, MagicMock


class TestNormalizeSaveName(unittest.TestCase):

    def setUp(self):
        import tools.cli_command.cli_config as m
        self.normalize = m._normalize_save_name

    def test_appends_config_suffix(self):
        self.assertEqual(self.normalize("my_board"), "my_board.config")

    def test_keeps_existing_suffix(self):
        self.assertEqual(self.normalize("my_board.config"),
                         "my_board.config")

    def test_strips_whitespace(self):
        self.assertEqual(self.normalize("  my_board  "),
                         "my_board.config")

    def test_rejects_empty(self):
        self.assertEqual(self.normalize(""), "")
        self.assertEqual(self.normalize("   "), "")
        self.assertEqual(self.normalize(None), "")

    def test_rejects_forward_slash(self):
        self.assertEqual(self.normalize("a/b"), "")

    def test_rejects_backslash(self):
        self.assertEqual(self.normalize("a\\b"), "")

    def test_rejects_parent_traversal(self):
        self.assertEqual(self.normalize("../evil"), "")
        self.assertEqual(self.normalize(".."), "")


class TestInvalidateDerivedArtifacts(unittest.TestCase):
    '''
    tools/kconfiglib/CMakeLists.txt only regenerates using.cmake and
    tuya_kconfig.h when they are absent, so `config set` has to remove
    them. It must never remove using.config, which it just wrote.
    '''

    def setUp(self):
        import tools.cli_command.cli_config as m
        self.m = m

    def _run(self, build_path):
        params = {"app_build_path": build_path}
        with patch('tools.cli_command.cli_config.get_global_params',
                   return_value=params), \
             patch('tools.cli_command.cli_config.get_logger',
                   return_value=MagicMock()):
            return self.m._invalidate_derived_artifacts()

    def _make_tree(self, build_path):
        cache = os.path.join(build_path, "cache")
        include = os.path.join(build_path, "include")
        os.makedirs(cache)
        os.makedirs(include)
        paths = {
            "cmake": os.path.join(cache, "using.cmake"),
            "header": os.path.join(include, "tuya_kconfig.h"),
            "config": os.path.join(cache, "using.config"),
        }
        for path in paths.values():
            with open(path, 'w', encoding='utf-8') as f:
                f.write("placeholder\n")
        return paths

    def test_removes_using_cmake_and_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_path = os.path.join(tmp, ".build")
            paths = self._make_tree(build_path)
            removed = self._run(build_path)
            self.assertFalse(os.path.exists(paths["cmake"]))
            self.assertFalse(os.path.exists(paths["header"]))
            self.assertEqual(sorted(removed),
                             sorted([paths["cmake"], paths["header"]]))

    def test_leaves_using_config_untouched(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_path = os.path.join(tmp, ".build")
            paths = self._make_tree(build_path)
            self._run(build_path)
            self.assertTrue(os.path.exists(paths["config"]))

    def test_missing_files_is_not_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_path = os.path.join(tmp, ".build")
            os.makedirs(build_path)
            self.assertEqual(self._run(build_path), [])

    def test_absent_build_dir_is_not_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_path = os.path.join(tmp, "no_such_build")
            self.assertEqual(self._run(build_path), [])


class TestDerivedArtifactsList(unittest.TestCase):

    def test_matches_cmake_generated_files(self):
        '''
        Guard against the list drifting from
        tools/kconfiglib/CMakeLists.txt, which is what makes the
        invalidation necessary in the first place.
        '''
        import tools.cli_command.cli_config as m
        names = [parts[-1] for parts in m.DERIVED_ARTIFACTS]
        self.assertIn("using.cmake", names)
        self.assertIn("tuya_kconfig.h", names)


class TestConfigMenuResyncsUsingConfigMtime(unittest.TestCase):
    '''
    Regression test for the B11 follow-up: config_menu_exec must not
    leave app_default.config newer than using.config, or the very next
    init_using_config(force=False) call (e.g. from a plain `tos.py build`)
    would treat a menuconfig session as if the user had hand-edited
    app_default.config and pay for a needless full re-derive.
    '''

    def test_defconfig_runs_after_savedefconfig(self):
        import tools.cli_command.cli_config as m

        calls = []
        params = {
            "using_config": "using.config",
            "catalog_kconfig": "Catalog.Kconfig",
            "app_default_config": "app_default.config",
        }

        with patch('tools.cli_command.cli_config.full_clean_project'), \
             patch('tools.cli_command.cli_config.init_using_config'), \
             patch('tools.cli_command.cli_config.get_global_params',
                   return_value=params), \
             patch('tools.cli_command.cli_config.Kconfig'), \
             patch('tools.cli_command.cli_config.menuconfig'), \
             patch('tools.cli_command.cli_config._savedefconfig',
                   side_effect=lambda *a, **k: calls.append('savedefconfig')), \
             patch('tools.cli_command.cli_config._defconfig',
                   side_effect=lambda *a, **k: calls.append('defconfig')):
            with self.assertRaises(SystemExit):
                m.config_menu_exec.callback()

        self.assertEqual(
            calls, ['savedefconfig', 'defconfig'],
            "using.config must be re-derived AFTER app_default.config is "
            "written, or its mtime is left stale relative to "
            "app_default.config")


if __name__ == "__main__":
    unittest.main()
