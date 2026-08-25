#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for the input validation added to
# tools/cli_command/cli_new.py::new_platform_exec (B1 fix).
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_cli_new -v

import unittest


class TestPlatformNameWhitelist(unittest.TestCase):
    '''
    new_platform_name is interpolated into a shell/argv command
    (do_subprocess) and joined into filesystem paths, so it must be
    restricted to a safe character set. This closes both the
    shell-injection vector and a path-traversal vector (e.g. "../../etc")
    in one place.
    '''

    def setUp(self):
        import tools.cli_command.cli_new as m
        self.pattern = m._PLATFORM_NAME_RE

    def test_accepts_alphanumeric(self):
        self.assertIsNotNone(self.pattern.match("T5AI_v2"))

    def test_accepts_hyphen_and_underscore(self):
        self.assertIsNotNone(self.pattern.match("ESP32-S3_custom"))

    def test_rejects_empty(self):
        self.assertIsNone(self.pattern.match(""))

    def test_rejects_whitespace(self):
        self.assertIsNone(self.pattern.match("my platform"))

    def test_rejects_shell_metacharacters(self):
        for bad in ["a;rm -rf ~", "a&&b", "a|b", "a`whoami`",
                   "a$(whoami)", "a>out.txt", "a<in.txt"]:
            self.assertIsNone(self.pattern.match(bad), bad)

    def test_rejects_path_traversal(self):
        self.assertIsNone(self.pattern.match("../../etc"))
        self.assertIsNone(self.pattern.match("a/b"))
        self.assertIsNone(self.pattern.match("a\\b"))


if __name__ == "__main__":
    unittest.main()
