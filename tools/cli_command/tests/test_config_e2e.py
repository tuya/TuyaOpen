#!/usr/bin/env python3
# coding=utf-8
#
# End-to-end tests for the non-interactive `tos.py config` subcommands.
# These load the real boards/ + src/ Kconfig tree, so they cover the
# kconfiglib behaviour that the mini-fixture unit tests cannot. No
# compilation and no toolchain is involved.
#
# Run from the repo root:
#   TUYAOPEN_E2E=1 python -m unittest \
#       tools.cli_command.tests.test_config_e2e -v
# PowerShell:
#   $env:TUYAOPEN_E2E="1"

import os
import sys
import json
import shutil
import filecmp
import hashlib
import tempfile
import subprocess
import unittest


OPEN_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TOS_PY = os.path.join(OPEN_ROOT, "tos.py")
SOURCE_APP = os.path.join(OPEN_ROOT, "apps", "tuya_cloud", "switch_demo")


def _digest(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


@unittest.skipUnless(os.environ.get("TUYAOPEN_E2E") == "1",
                     "set TUYAOPEN_E2E=1 to run the end-to-end tests")
class ConfigE2EBase(unittest.TestCase):
    '''
    Runs every command against a throwaway copy of switch_demo.

    The copy is mandatory: switch_demo/app_default.config is tracked by
    git and `config set` rewrites it. Running from a temp dir also
    proves the commands are cwd-independent, since open_root comes from
    sys.argv[0] and CatalogKconfig sources absolute paths.
    '''

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.app = os.path.join(self._tmp.name, "switch_demo")
        os.makedirs(self.app)
        # Only these two files matter to the config commands: the
        # CMakeLists.txt satisfies check_proj_dir() and the config is
        # the state under test. switch_demo has no app Kconfig.
        for name in ("CMakeLists.txt", "app_default.config"):
            shutil.copy(os.path.join(SOURCE_APP, name),
                        os.path.join(self.app, name))
        self.app_default = os.path.join(self.app, "app_default.config")
        self.using = os.path.join(self.app, ".build", "cache",
                                  "using.config")

    def run_config(self, *args, **kwargs):
        cmd = [sys.executable, TOS_PY, "config"] + list(args)
        return subprocess.run(cmd, cwd=self.app, capture_output=True,
                              text=True, **kwargs)

    def read(self, path):
        with open(path, encoding='utf-8') as f:
            return f.read()


class TestConfigGet(ConfigE2EBase):

    def test_get_prints_bare_value_for_single_symbol(self):
        result = self.run_config(
            "get", "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "4096")

    def test_get_json_stdout_is_not_polluted_by_logs(self):
        result = self.run_config(
            "get", "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN",
            "CONFIG_PLATFORM_CHOICE", "-j")
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(
            payload["CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN"], 4096)
        self.assertEqual(payload["CONFIG_PLATFORM_CHOICE"], "T5AI")
        # The "[INFO]: Running tos.py ..." banner must be on stderr.
        self.assertIn("Running tos.py", result.stderr)

    def test_get_accepts_name_without_config_prefix(self):
        result = self.run_config("get", "PLATFORM_CHOICE")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "T5AI")

    def test_get_all_shows_attributes(self):
        result = self.run_config("get", "-a", "CONFIG_BOARD_ENABLE_T5AI")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("bool", result.stdout)
        self.assertIn("configurable: no", result.stdout)

    def test_get_unknown_symbol_exits_nonzero(self):
        result = self.run_config("get", "CONFIG_NOPE_XYZ")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown symbol", result.stderr)

    def test_get_unknown_symbol_still_emits_valid_json(self):
        result = self.run_config("get", "CONFIG_NOPE_XYZ", "-j")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout),
                         {"CONFIG_NOPE_XYZ": None})

    def test_get_does_not_mutate_configs(self):
        self.run_config("list")
        before_default = _digest(self.app_default)
        before_using = _digest(self.using)
        self.run_config("get", "CONFIG_PLATFORM_CHOICE", "-a")
        self.assertEqual(_digest(self.app_default), before_default)
        self.assertEqual(_digest(self.using), before_using)


class TestConfigList(ConfigE2EBase):

    def test_list_filtered(self):
        result = self.run_config("list", "-p", "BOARD_CHOICE")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CONFIG_BOARD_CHOICE_T5AI=y", result.stdout)

    def test_list_shows_unset_symbols(self):
        result = self.run_config("list", "-p", "BOARD_CHOICE")
        self.assertIn("is not set", result.stdout)

    def test_list_json_parses(self):
        result = self.run_config("list", "-p", "MBEDTLS", "-j")
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(
            payload["CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN"], 4096)

    def test_list_no_match_warns_but_succeeds(self):
        result = self.run_config("list", "-p", "ZZZ_NO_SUCH_THING")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "")
        self.assertIn("No symbol matched", result.stderr)


class TestConfigSet(ConfigE2EBase):

    def test_set_int_roundtrip(self):
        result = self.run_config(
            "set", "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192",
                      self.read(self.app_default))
        self.assertIn("CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192",
                      self.read(self.using))
        readback = self.run_config(
            "get", "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN", "-j")
        self.assertEqual(
            json.loads(readback.stdout)
            ["CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN"], 8192)

    def test_set_does_not_leave_a_dot_old_backup(self):
        self.run_config("set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        self.assertFalse(os.path.exists(self.using + ".old"))

    def test_set_accepts_name_without_config_prefix(self):
        result = self.run_config("set", "ENABLE_LIBLVGL=y")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CONFIG_ENABLE_LIBLVGL=y",
                      self.read(self.app_default))

    def test_min_config_stays_minimal(self):
        self.run_config("set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        lines = [ln for ln in self.read(self.app_default).splitlines()
                 if ln.strip() and not ln.startswith("#")]
        # Only the board choice and the one overridden value differ
        # from the Kconfig defaults.
        self.assertEqual(len(lines), 2, lines)

    def test_set_unknown_symbol_is_atomic(self):
        self.run_config("list")  # materialise .build first
        before = _digest(self.app_default)
        result = self.run_config("set", "CONFIG_NOPE_XYZ=y",
                                 "ENABLE_LIBLVGL=y")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown symbol", result.stderr)
        self.assertEqual(_digest(self.app_default), before)
        self.assertNotIn("CONFIG_ENABLE_LIBLVGL=y", self.read(self.using))

    def test_set_promptless_symbol_fails(self):
        before = _digest(self.app_default)
        result = self.run_config("set", "CONFIG_BOARD_ENABLE_T5AI=n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no prompt", result.stderr)
        self.assertEqual(_digest(self.app_default), before)

    def test_set_bad_token_fails(self):
        result = self.run_config("set", "CONFIG_NO_EQUALS_SIGN")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no '='", result.stderr)

    def test_set_without_arguments_fails(self):
        result = self.run_config("set")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Nothing to do", result.stderr)

    def test_set_choice_propagates_to_derived_symbols(self):
        result = self.run_config("set", "CONFIG_BOARD_CHOICE_ESP32=y")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("identity changed", result.stderr)
        body = self.read(self.app_default)
        self.assertIn("CONFIG_BOARD_CHOICE_ESP32=y", body)
        self.assertNotIn("CONFIG_BOARD_CHOICE_T5AI=y", body)
        readback = self.run_config("get", "PLATFORM_CHOICE",
                                   "CHIP_CHOICE", "-j")
        payload = json.loads(readback.stdout)
        self.assertEqual(payload["CONFIG_PLATFORM_CHOICE"], "ESP32")
        self.assertEqual(payload["CONFIG_CHIP_CHOICE"], "esp32")

    def test_unset_reverts_to_default(self):
        self.run_config("set", "ENABLE_LIBLVGL=y")
        self.assertIn("CONFIG_ENABLE_LIBLVGL=y",
                      self.read(self.app_default))
        result = self.run_config("set", "-u", "CONFIG_ENABLE_LIBLVGL")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("CONFIG_ENABLE_LIBLVGL=y",
                         self.read(self.app_default))

    def test_unset_unknown_symbol_fails(self):
        result = self.run_config("set", "-u", "CONFIG_NOPE_XYZ")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown symbol", result.stderr)

    def test_no_save_leaves_app_default_untouched(self):
        self.run_config("list")
        before = _digest(self.app_default)
        result = self.run_config(
            "set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192", "--no-save")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(_digest(self.app_default), before)
        self.assertIn("CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192",
                      self.read(self.using))

    def test_set_invalidates_derived_artifacts(self):
        self.run_config("list")  # materialise .build/cache
        cache = os.path.join(self.app, ".build", "cache")
        include = os.path.join(self.app, ".build", "include")
        os.makedirs(include, exist_ok=True)
        using_cmake = os.path.join(cache, "using.cmake")
        header = os.path.join(include, "tuya_kconfig.h")
        for path in (using_cmake, header):
            with open(path, 'w', encoding='utf-8') as f:
                f.write("stale\n")

        result = self.run_config(
            "set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(os.path.exists(using_cmake))
        self.assertFalse(os.path.exists(header))
        # The config we just wrote must survive the invalidation.
        self.assertTrue(os.path.exists(self.using))

    def test_no_save_refuses_an_identity_change(self):
        '''
        A board switch forces a full clean, which rebuilds using.config
        from app_default.config. Under --no-save that file still holds
        the old board, so the combination would silently discard the
        change. It must be refused instead.
        '''
        self.run_config("list")
        before = _digest(self.app_default)
        result = self.run_config("set", "CONFIG_BOARD_CHOICE_ESP32=y",
                                 "--no-save")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--no-save cannot express that", result.stderr)
        self.assertEqual(_digest(self.app_default), before)
        readback = self.run_config("get", "PLATFORM_CHOICE")
        self.assertEqual(readback.stdout.strip(), "T5AI")

    def test_identity_change_with_keep_build_warns_instead(self):
        result = self.run_config("set", "CONFIG_BOARD_CHOICE_ESP32=y", "-k")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("clean -f", result.stderr)

    def test_keep_build_preserves_derived_artifacts(self):
        self.run_config("list")
        using_cmake = os.path.join(self.app, ".build", "cache",
                                   "using.cmake")
        with open(using_cmake, 'w', encoding='utf-8') as f:
            f.write("stale\n")
        result = self.run_config(
            "set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192", "-k")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(os.path.exists(using_cmake))


class TestConfigDiff(ConfigE2EBase):

    def test_diff_of_equivalent_configs_reports_no_difference(self):
        '''
        switch_demo just selects the T5AI board, so its expansion is
        identical to the board default even though the two minimal
        files are not byte-identical. A textual diff would show noise
        here; an expanded diff correctly shows nothing.
        '''
        result = self.run_config("diff", "T5AI")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "")
        self.assertIn("No difference", result.stderr)

    def test_diff_reports_a_changed_symbol(self):
        self.run_config("set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        result = self.run_config("diff", "T5AI")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("a: T5AI.config", result.stdout)
        self.assertIn("b: app_default.config", result.stdout)
        self.assertIn("CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN",
                      result.stdout)
        self.assertIn("-> 8192", result.stdout)

    def test_diff_across_boards_shows_platform_change(self):
        result = self.run_config("diff", "T5AI", "ESP32-C3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CONFIG_PLATFORM_CHOICE: T5AI -> ESP32",
                      result.stdout)

    def test_diff_json_parses(self):
        self.run_config("set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        result = self.run_config("diff", "T5AI", "-j")
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["a"], "T5AI.config")
        entry = payload["diff"][
            "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN"]
        self.assertEqual(entry["b"], "8192")

    def test_diff_unknown_config_fails(self):
        result = self.run_config("diff", "no_such_config")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not found", result.stderr)


class TestConfigSave(ConfigE2EBase):

    def test_save_non_interactive(self):
        result = self.run_config("save", "-n", "ci_smoke")
        self.assertEqual(result.returncode, 0, result.stderr)
        saved = os.path.join(self.app, "config", "ci_smoke.config")
        self.assertTrue(os.path.exists(saved))
        self.assertTrue(filecmp.cmp(saved, self.app_default, shallow=False))

    def test_save_refuses_to_overwrite_without_force(self):
        self.run_config("save", "-n", "ci_smoke")
        result = self.run_config("save", "-n", "ci_smoke")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("use -f to overwrite", result.stderr)

    def test_save_force_overwrites(self):
        self.run_config("save", "-n", "ci_smoke")
        result = self.run_config("save", "-n", "ci_smoke", "-f")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_save_rejects_path_traversal(self):
        result = self.run_config("save", "-n", "../evil")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Invalid config name", result.stderr)

    def test_save_without_name_does_not_hang(self):
        result = self.run_config("save", stdin=subprocess.DEVNULL,
                                 timeout=120)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("-n NAME", result.stderr)

    def test_saved_config_is_selectable_again(self):
        self.run_config("set", "ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=8192")
        self.run_config("save", "-n", "ci_smoke")
        result = self.run_config("choice", "-c", "ci_smoke.config")
        self.assertEqual(result.returncode, 0, result.stderr)
        readback = self.run_config(
            "get", "CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN")
        self.assertEqual(readback.stdout.strip(), "8192")


if __name__ == "__main__":
    unittest.main()
