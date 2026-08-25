#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for the try/finally fix around update_submodules() in
# tools/cli_command/cli_check.py (B3 fix).
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_cli_check -v

import unittest
from unittest.mock import patch, call, MagicMock


class TestUpdateSubmodulesMirrorCleanup(unittest.TestCase):
    '''
    set_repo_mirro rewrites the user's global ~/.gitconfig (github ->
    gitee, 13 repos) for the duration of the submodule download. Before
    this fix, an exception (or Ctrl-C) from download_submoudules() would
    skip the unset call and leave that rewrite permanently in place,
    affecting every other git operation on the machine.
    '''

    def _patched(self, country_code, download_side_effect):
        import tools.cli_command.cli_check as m
        params = {"open_root": "/fake/root"}
        return m, [
            patch('tools.cli_command.cli_check.get_global_params',
                 return_value=params),
            patch('tools.cli_command.cli_check.get_country_code',
                 return_value=country_code),
            patch('tools.cli_command.cli_check.set_repo_mirro'),
            patch('tools.cli_command.cli_check.download_submoudules',
                 side_effect=download_side_effect),
        ]

    def test_mirror_is_unset_even_if_download_raises(self):
        m, patches = self._patched("China", RuntimeError("boom"))
        with patches[0], patches[1], patches[2] as mirro, patches[3]:
            with self.assertRaises(RuntimeError):
                m.update_submodules()
            self.assertEqual(
                mirro.call_args_list,
                [call(unset=False), call(unset=True)])

    def test_mirror_set_and_unset_on_success(self):
        m, patches = self._patched("China", lambda open_root: True)
        with patches[0], patches[1], patches[2] as mirro, patches[3]:
            ret = m.update_submodules()
            self.assertTrue(ret)
            self.assertEqual(
                mirro.call_args_list,
                [call(unset=False), call(unset=True)])

    def test_mirror_not_touched_outside_china(self):
        m, patches = self._patched("Other", lambda open_root: True)
        with patches[0], patches[1], patches[2] as mirro, patches[3]:
            ret = m.update_submodules()
            self.assertTrue(ret)
            mirro.assert_not_called()


if __name__ == "__main__":
    unittest.main()
