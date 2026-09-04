#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for the environment tos.py hands to a build's children:
# the interpreter, PATH, and the region hand-off.
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_cli_build_env -v

import os
import re
import sys
import unittest
from unittest.mock import patch

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))


def read_source(rel):
    """Return a repo file's text, for asserting on call-site shape."""
    with open(os.path.join(REPO_ROOT, rel), encoding='utf-8') as f:
        return f.read()


class GlobalParamsIsolation(unittest.TestCase):
    """Restore GLOBAL_PARAMS for tests that call the real setter.

    set_global_params() derives from sys.argv[0] and cwd, which differ under
    the test runner, so leaving it mutated would leak into the modules that
    run after this one.
    """

    def setUp(self):
        import tools.cli_command.util as util
        self.util = util
        snapshot = dict(util.GLOBAL_PARAMS)

        def restore():
            util.GLOBAL_PARAMS.clear()
            util.GLOBAL_PARAMS.update(snapshot)

        self.addCleanup(restore)


class TestChildInterpreter(GlobalParamsIsolation):
    '''
    A bare "python" resolves through PATH, which on Windows reaches uv's
    base interpreter instead of .venv, and the child then dies importing
    the SDK's dependencies. Every call site must use params["python"].
    '''

    def test_defaults_to_the_running_interpreter(self):
        with patch.dict(os.environ, {}, clear=True):
            self.util.set_global_params()
        self.assertEqual(
            self.util.get_global_params()["python"], sys.executable)

    def test_never_falls_back_to_a_bare_name(self):
        with patch.dict(os.environ, {}, clear=True):
            self.util.set_global_params()
        self.assertNotEqual(self.util.get_global_params()["python"], "python")

    def test_export_wins_over_the_running_interpreter(self):
        with patch.dict(os.environ, {"OPEN_SDK_PYTHON": "/venv/bin/python"}):
            self.util.set_global_params()
        self.assertEqual(
            self.util.get_global_params()["python"], "/venv/bin/python")

    def test_empty_export_falls_back_rather_than_being_used(self):
        with patch.dict(os.environ, {"OPEN_SDK_PYTHON": ""}):
            self.util.set_global_params()
        self.assertEqual(
            self.util.get_global_params()["python"], sys.executable)


class TestCallSitesUseIt(unittest.TestCase):

    def test_no_bare_python_argv_in_python_call_sites(self):
        # An argv list opening with the literal name, e.g. ["python", ...].
        # The lookbehind excludes params["python"], which is the fix: there
        # the bracket is a subscript and follows an identifier.
        bare = re.compile(r'(?<![\w\]])\[\s*"python"\s*[,\]]')
        for rel in ("tools/cli_command/cli_build.py",
                    "tools/cli_command/cli_new.py"):
            hit = bare.search(read_source(rel))
            self.assertIsNone(hit, f"{rel}: {hit.group() if hit else ''}")

    def test_cmake_call_sites_use_tos_python(self):
        '''
        These are baked into build.ninja at configure time, so unlike the
        subprocess call sites they cannot be corrected from the environment.
        '''
        for rel in ("CMakeLists.txt",
                    "tools/kconfiglib/gen_build_param.cmake"):
            source = read_source(rel)
            self.assertNotIn("COMMAND python ", source, rel)
            self.assertNotIn("BUILD_COMMAND python ", source, rel)
            self.assertNotIn("\n    python ", source, rel)

    def test_configure_passes_the_interpreter_to_cmake(self):
        self.assertIn("-DTOS_PYTHON=",
                      read_source("tools/cli_command/cli_build.py"))

    def test_cmake_keeps_a_fallback_for_a_hand_run_configure(self):
        self.assertIn("set(TOS_PYTHON python)", read_source("CMakeLists.txt"))


class TestRegionHandoff(unittest.TestCase):
    '''
    Platforms are separate repositories that only receive argv and the
    environment. Without the hand-off they re-detect the region themselves,
    which is what pulled a network probe into T5AI's prepare step.
    '''

    def setUp(self):
        import tools.cli_command.util as util
        self.util = util

    def test_exports_a_definite_value_for_china(self):
        with patch.object(self.util, "get_country_code",
                          return_value="China"), \
                patch.dict(os.environ, {}, clear=True):
            self.assertEqual(self.util.export_country_code(), "China")
            self.assertEqual(os.environ["OPEN_COUNTRY_CODE"], "China")

    def test_exports_a_definite_value_when_overseas(self):
        '''
        "" is indistinguishable from unset, so a child could not tell
        "overseas" from "nobody told me" and would probe anyway.
        '''
        with patch.object(self.util, "get_country_code", return_value=""), \
                patch.dict(os.environ, {}, clear=True):
            self.assertEqual(self.util.export_country_code(), "Other")
            self.assertEqual(os.environ["OPEN_COUNTRY_CODE"], "Other")

    def test_build_hands_the_region_to_platform_children(self):
        self.assertIn("export_country_code()",
                      read_source("tools/cli_command/cli_build.py"))


class TestBuildPreparesTheEnvironment(unittest.TestCase):
    '''
    build used to verify cmake/ninja only, leaving the bundled GNU Make off
    PATH unless export had been sourced -- so a T5AI build compiled every
    source and only then died with "make: command not found".
    '''

    def setUp(self):
        import tools.cli_command.cli_prepare as prepare
        self.prepare = prepare

    def test_prepend_is_idempotent(self):
        target = os.path.dirname(os.path.abspath(sys.executable))
        with patch.dict(os.environ, {"PATH": ""}):
            self.prepare._prepend_to_path(target)
            self.prepare._prepend_to_path(target)
            self.assertEqual(
                os.environ["PATH"].split(os.pathsep).count(target), 1)

    def test_prepend_ignores_a_missing_directory(self):
        with patch.dict(os.environ, {"PATH": "keep"}):
            self.prepare._prepend_to_path("/no/such/dir/at/all")
            self.assertEqual(os.environ["PATH"], "keep")

    def test_venv_scripts_land_on_path(self):
        scripts = os.path.dirname(os.path.abspath(sys.executable))
        with patch.dict(os.environ, {"PATH": ""}):
            self.prepare.prepend_venv_scripts_to_path()
            self.assertIn(scripts, os.environ["PATH"].split(os.pathsep))

    def test_ensure_build_env_installs_host_tools_not_just_cmake(self):
        with patch.object(self.prepare, "download_host_tools",
                          return_value=True) as host, \
                patch.object(self.prepare, "ensure_build_tools",
                             return_value=True) as venv:
            self.assertTrue(self.prepare.ensure_build_env())
        host.assert_called_once()
        venv.assert_called_once()

    def test_host_tool_failure_warns_but_does_not_veto_the_build(self):
        '''
        Only cmake/ninja are needed by every platform. Make is not, and this
        runs before the platform is known, so a failed make install must not
        block an ESP32 or LINUX build that never calls make.
        '''
        with patch.object(self.prepare, "download_host_tools",
                          return_value=False), \
                patch.object(self.prepare, "ensure_build_tools",
                             return_value=True):
            self.assertTrue(self.prepare.ensure_build_env())

    def test_missing_cmake_or_ninja_is_still_fatal(self):
        with patch.object(self.prepare, "download_host_tools",
                          return_value=True), \
                patch.object(self.prepare, "ensure_build_tools",
                             return_value=False):
            self.assertFalse(self.prepare.ensure_build_env())

    def test_clean_prepares_the_same_environment_as_build(self):
        '''
        clean_all depends on platform_clean, which runs the platform hook --
        `make clean` for T5AI. Without this, clean fails on a missing make
        and the stale build directory survives.
        '''
        self.assertIn("ensure_build_env",
                      read_source("tools/cli_command/cli_clean.py"))

    def test_build_uses_ensure_build_env(self):
        source = read_source("tools/cli_command/cli_build.py")
        self.assertIn("ensure_build_env", source)
        self.assertNotIn("import ensure_build_tools", source)


if __name__ == '__main__':
    unittest.main()
