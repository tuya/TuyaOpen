#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for tools/cli_command/util.py::do_subprocess
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_util_subprocess -v

import os
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock


# Writes a marker file relative to the process working directory, then
# exits with the code given as argv[1]. Lives outside the directory it
# is asked to run in, so the marker's location proves where the child
# actually ran.
CHILD_SCRIPT = '''
import sys
with open("marker.txt", "w") as f:
    f.write("ok")
sys.exit(int(sys.argv[1]))
'''


class TestDoSubprocess(unittest.TestCase):

    def setUp(self):
        import tools.cli_command.util as m
        self.m = m
        self._logger_patch = patch('tools.cli_command.util.get_logger',
                                   return_value=MagicMock())
        self._logger_patch.start()
        self.addCleanup(self._logger_patch.stop)

        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.script_dir = os.path.join(self._tmp.name, "scripts")
        self.work_dir = os.path.join(self._tmp.name, "work")
        os.makedirs(self.script_dir)
        os.makedirs(self.work_dir)
        self.script = os.path.join(self.script_dir, "child.py")
        with open(self.script, 'w', encoding='utf-8') as f:
            f.write(CHILD_SCRIPT)

    def child_cmd(self, exit_code=0):
        return f'"{sys.executable}" "{self.script}" {exit_code}'

    def test_cwd_is_applied(self):
        '''
        The regression this guards: the old implementation built
        "cd <dir> && <cmd>" and ran it through os.system, whose cmd.exe
        `cd` does not switch drives without /d. On Windows the temp dir
        is usually on a different drive from the checkout, so this also
        exercises the cross-drive case that used to fail silently.
        '''
        ret = self.m.do_subprocess(self.child_cmd(), cwd=self.work_dir)
        self.assertEqual(ret, 0)
        self.assertTrue(
            os.path.exists(os.path.join(self.work_dir, "marker.txt")))
        self.assertFalse(
            os.path.exists(os.path.join(self.script_dir, "marker.txt")))
        self.assertFalse(os.path.exists(
            os.path.join(os.getcwd(), "marker.txt")))

    def test_cwd_is_applied_across_drives(self):
        '''
        Explicitly named so a failure points at the drive handling.
        Skipped on single-drive layouts and on POSIX.
        '''
        here = os.path.splitdrive(os.path.abspath(os.getcwd()))[0]
        there = os.path.splitdrive(os.path.abspath(self.work_dir))[0]
        if not here or here.lower() == there.lower():
            self.skipTest("cwd and temp dir are on the same drive")
        ret = self.m.do_subprocess(self.child_cmd(), cwd=self.work_dir)
        self.assertEqual(ret, 0)
        self.assertTrue(
            os.path.exists(os.path.join(self.work_dir, "marker.txt")))

    def test_returns_zero_on_success(self):
        self.assertEqual(
            self.m.do_subprocess(self.child_cmd(0), cwd=self.work_dir), 0)

    def test_returns_the_child_exit_code(self):
        '''
        os.system returned a wait status on POSIX (exit 3 -> 768);
        subprocess.call reports the exit code itself on both platforms.
        '''
        self.assertEqual(
            self.m.do_subprocess(self.child_cmd(3), cwd=self.work_dir), 3)

    def test_missing_cwd_returns_nonzero_without_raising(self):
        missing = os.path.join(self._tmp.name, "no_such_dir")
        ret = self.m.do_subprocess(self.child_cmd(), cwd=missing)
        self.assertNotEqual(ret, 0)

    def test_cwd_pointing_at_a_file_returns_nonzero(self):
        ret = self.m.do_subprocess(self.child_cmd(), cwd=self.script)
        self.assertNotEqual(ret, 0)

    def test_empty_cmd_returns_zero(self):
        self.assertEqual(self.m.do_subprocess(""), 0)
        self.assertEqual(self.m.do_subprocess(None), 0)

    def test_without_cwd_runs_in_the_current_directory(self):
        '''
        Backward compatibility: the callers that pass absolute paths and
        no cwd (rm_rf, porting_platform) must keep working.
        '''
        original = os.getcwd()
        os.chdir(self.work_dir)
        self.addCleanup(os.chdir, original)
        ret = self.m.do_subprocess(self.child_cmd())
        self.assertEqual(ret, 0)
        self.assertTrue(
            os.path.exists(os.path.join(self.work_dir, "marker.txt")))

    def test_shell_features_still_work(self):
        '''
        shell=True is retained: POSIX platform hooks are invoked as
        ./platform_prepare.sh, and some commands rely on redirection.
        '''
        marker = os.path.join(self.work_dir, "shell.txt")
        ret = self.m.do_subprocess(f'echo hello> "{marker}"',
                                   cwd=self.work_dir)
        self.assertEqual(ret, 0)
        with open(marker, encoding='utf-8') as f:
            self.assertIn("hello", f.read())

    def test_list_form_returns_the_child_exit_code(self):
        ret = self.m.do_subprocess(
            [sys.executable, self.script, "3"], cwd=self.work_dir)
        self.assertEqual(ret, 3)

    def test_list_form_applies_cwd(self):
        ret = self.m.do_subprocess(
            [sys.executable, self.script, "0"], cwd=self.work_dir)
        self.assertEqual(ret, 0)
        self.assertTrue(
            os.path.exists(os.path.join(self.work_dir, "marker.txt")))

    def test_list_form_missing_cwd_returns_nonzero_without_raising(self):
        missing = os.path.join(self._tmp.name, "no_such_dir")
        ret = self.m.do_subprocess(
            [sys.executable, self.script, "0"], cwd=missing)
        self.assertNotEqual(ret, 0)

    def test_empty_list_returns_zero(self):
        self.assertEqual(self.m.do_subprocess([]), 0)
        self.assertEqual(self.m.do_subprocess(()), 0)

    def test_list_form_does_not_go_through_a_shell(self):
        '''
        The core of the B1 fix: an argument containing shell
        metacharacters must reach the child as one literal argv entry
        (shell=False), never be interpreted/split/expanded by a shell.
        '''
        marker = os.path.join(self.work_dir, "argv.txt")
        argv_script = os.path.join(self.script_dir, "print_argv.py")
        with open(argv_script, 'w', encoding='utf-8') as f:
            f.write(
                "import sys\n"
                "with open(sys.argv[1], 'w', encoding='utf-8') as out:\n"
                "    out.write(repr(sys.argv[2:]))\n"
            )
        dangerous = "a_name; touch INJECTED && echo $(whoami)"
        ret = self.m.do_subprocess(
            [sys.executable, argv_script, marker, dangerous],
            cwd=self.work_dir)
        self.assertEqual(ret, 0)
        with open(marker, encoding='utf-8') as f:
            self.assertEqual(f.read(), repr([dangerous]))
        self.assertFalse(
            os.path.exists(os.path.join(self.work_dir, "INJECTED")))


class TestCallSitesPassCwd(unittest.TestCase):
    '''
    The build/clean paths must hand the directory to do_subprocess
    instead of prefixing the command with "cd <dir> &&", which is what
    broke cross-drive builds.
    '''

    def _assert_uses_cwd(self, module_path):
        with open(module_path, encoding='utf-8') as f:
            body = f.read()
        self.assertNotIn('f"cd ', body)
        self.assertNotIn('"cd {', body)

    def test_cli_build_has_no_cd_prefix(self):
        import tools.cli_command.cli_build as m
        self._assert_uses_cwd(m.__file__)

    def test_cli_clean_has_no_cd_prefix(self):
        import tools.cli_command.cli_clean as m
        self._assert_uses_cwd(m.__file__)

    def test_util_git_has_no_cd_prefix(self):
        import tools.cli_command.util_git as m
        self._assert_uses_cwd(m.__file__)


if __name__ == "__main__":
    unittest.main()
