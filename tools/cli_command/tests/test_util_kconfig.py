#!/usr/bin/env python3
# coding=utf-8
#
# Unit tests for tools/cli_command/util_kconfig.py
#
# Run from the repo root:
#   python -m unittest tools.cli_command.tests.test_util_kconfig -v

import os
import json
import tempfile
import unittest
from unittest.mock import patch, MagicMock


# A tiny Kconfig tree that still covers every branch of the module:
# every symbol type, a prompt-less symbol, a dependency-guarded symbol
# and a choice with a derived string.
FIXTURE_KCONFIG = '''
config PROMPTED_BOOL
    bool "prompted bool"

config PROMPTLESS_BOOL
    bool
    default y

config GUARD
    bool "guard"

config GUARDED
    bool "guarded"
    depends on GUARD

config NUM
    int "num"
    default 4096

config HEXNUM
    hex "hexnum"
    default 0x10

config TEXT
    string "text"
    default "hello"

choice
    prompt "pick"
    default PICK_A

config PICK_A
    bool "A"

config PICK_B
    bool "B"

endchoice

config PICK_NAME
    string
    default "a" if PICK_A
    default "b" if PICK_B
'''

IDENTITY_KCONFIG = '''
config PLATFORM_CHOICE
    string "platform"
    default "T5AI"

config CHIP_CHOICE
    string "chip"
    default "t5ai"
'''


class KconfigFixture(unittest.TestCase):
    '''
    Base class: writes a Kconfig file into a temp dir and loads it.
    '''

    KCONFIG_TEXT = FIXTURE_KCONFIG

    def setUp(self):
        import tools.cli_command.util_kconfig as m
        self.m = m
        self._logger_patch = patch(
            'tools.cli_command.util_kconfig.get_logger',
            return_value=MagicMock())
        self._logger_patch.start()
        self.addCleanup(self._logger_patch.stop)

        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

        self.kconfig_path = os.path.join(self.tmp, "Kconfig")
        with open(self.kconfig_path, 'w', encoding='utf-8') as f:
            f.write(self.KCONFIG_TEXT)
        self.dot_config = os.path.join(self.tmp, "using.config")
        self.min_config = os.path.join(self.tmp, "min.config")

    def load(self):
        return self.m.load_kconfig(self.kconfig_path, self.dot_config)

    def sym(self, kconf, name):
        return self.m.find_symbol(kconf, name)


class TestStripPrefix(KconfigFixture):

    def test_strips_config_prefix(self):
        self.assertEqual(self.m.strip_prefix("CONFIG_FOO"), "FOO")

    def test_leaves_bare_name(self):
        self.assertEqual(self.m.strip_prefix("FOO"), "FOO")

    def test_strips_only_one_layer(self):
        self.assertEqual(self.m.strip_prefix("CONFIG_CONFIG_FOO"),
                         "CONFIG_FOO")

    def test_full_name_is_idempotent(self):
        self.assertEqual(self.m.full_name("FOO"), "CONFIG_FOO")
        self.assertEqual(self.m.full_name("CONFIG_FOO"), "CONFIG_FOO")


class TestParseAssignment(KconfigFixture):

    def test_strips_config_prefix(self):
        self.assertEqual(self.m.parse_assignment("CONFIG_A=y"), ("A", "y"))

    def test_accepts_bare_name(self):
        self.assertEqual(self.m.parse_assignment("A=y"), ("A", "y"))

    def test_rejects_token_without_equals(self):
        with self.assertRaises(ValueError):
            self.m.parse_assignment("CONFIG_A")

    def test_rejects_empty_name(self):
        with self.assertRaises(ValueError):
            self.m.parse_assignment("=y")

    def test_splits_on_first_equals_only(self):
        self.assertEqual(self.m.parse_assignment("CONFIG_S=a=b"),
                         ("S", "a=b"))

    def test_strips_surrounding_quotes(self):
        self.assertEqual(self.m.parse_assignment('CONFIG_S="v"'),
                         ("S", "v"))

    def test_keeps_inner_quotes(self):
        self.assertEqual(self.m.parse_assignment('CONFIG_S=a"b'),
                         ("S", 'a"b'))


class TestNormalizeValue(KconfigFixture):

    def test_bool_yes_variants(self):
        kconf = self.load()
        sym = self.sym(kconf, "PROMPTED_BOOL")
        for raw in ("y", "Y", "yes", "true", "TRUE", "1", "on"):
            self.assertEqual(self.m.normalize_value(sym, raw), (True, "y"),
                             msg=raw)

    def test_bool_no_variants(self):
        kconf = self.load()
        sym = self.sym(kconf, "PROMPTED_BOOL")
        for raw in ("n", "N", "no", "false", "0", "off"):
            self.assertEqual(self.m.normalize_value(sym, raw), (True, "n"),
                             msg=raw)

    def test_bool_rejects_garbage(self):
        kconf = self.load()
        sym = self.sym(kconf, "PROMPTED_BOOL")
        ok, _ = self.m.normalize_value(sym, "maybe")
        self.assertFalse(ok)

    def test_bool_rejects_module_value(self):
        kconf = self.load()
        sym = self.sym(kconf, "PROMPTED_BOOL")
        ok, _ = self.m.normalize_value(sym, "m")
        self.assertFalse(ok)

    def test_int_accepts_decimal(self):
        kconf = self.load()
        sym = self.sym(kconf, "NUM")
        self.assertEqual(self.m.normalize_value(sym, " 8192 "),
                         (True, "8192"))

    def test_int_rejects_non_numeric(self):
        kconf = self.load()
        sym = self.sym(kconf, "NUM")
        ok, _ = self.m.normalize_value(sym, "abc")
        self.assertFalse(ok)

    def test_int_rejects_hex_form(self):
        kconf = self.load()
        sym = self.sym(kconf, "NUM")
        ok, _ = self.m.normalize_value(sym, "0x10")
        self.assertFalse(ok)

    def test_hex_accepts_prefixed_and_bare(self):
        kconf = self.load()
        sym = self.sym(kconf, "HEXNUM")
        self.assertEqual(self.m.normalize_value(sym, "0x20"), (True, "0x20"))
        self.assertEqual(self.m.normalize_value(sym, "20"), (True, "20"))

    def test_hex_rejects_non_hex(self):
        kconf = self.load()
        sym = self.sym(kconf, "HEXNUM")
        ok, _ = self.m.normalize_value(sym, "zz")
        self.assertFalse(ok)

    def test_string_passthrough(self):
        kconf = self.load()
        sym = self.sym(kconf, "TEXT")
        self.assertEqual(self.m.normalize_value(sym, " keep me "),
                         (True, " keep me "))


class TestValueMatches(KconfigFixture):

    def test_hex_compares_numerically(self):
        kconf = self.load()
        sym = self.sym(kconf, "HEXNUM")
        self.assertTrue(self.m.value_matches(sym, "0x10"))
        self.assertTrue(self.m.value_matches(sym, "10"))
        self.assertFalse(self.m.value_matches(sym, "0x11"))

    def test_int_compares_numerically(self):
        kconf = self.load()
        sym = self.sym(kconf, "NUM")
        self.assertTrue(self.m.value_matches(sym, "4096"))
        self.assertFalse(self.m.value_matches(sym, "4097"))

    def test_string_compares_literally(self):
        kconf = self.load()
        sym = self.sym(kconf, "TEXT")
        self.assertTrue(self.m.value_matches(sym, "hello"))
        self.assertFalse(self.m.value_matches(sym, "Hello"))


class TestFindSymbol(KconfigFixture):

    def test_finds_with_and_without_prefix(self):
        kconf = self.load()
        self.assertIsNotNone(self.sym(kconf, "NUM"))
        self.assertIsNotNone(self.sym(kconf, "CONFIG_NUM"))

    def test_unknown_symbol_is_none(self):
        kconf = self.load()
        self.assertIsNone(self.sym(kconf, "NOPE_XYZ"))

    def test_lowercase_name_resolves(self):
        kconf = self.load()
        self.assertIsNotNone(self.sym(kconf, "num"))


class TestIsConfigurable(KconfigFixture):

    def test_prompted_symbol_is_configurable(self):
        kconf = self.load()
        self.assertTrue(self.m.is_configurable(
            self.sym(kconf, "PROMPTED_BOOL")))

    def test_promptless_symbol_is_not(self):
        kconf = self.load()
        self.assertFalse(self.m.is_configurable(
            self.sym(kconf, "PROMPTLESS_BOOL")))

    def test_derived_string_is_not(self):
        kconf = self.load()
        self.assertFalse(self.m.is_configurable(
            self.sym(kconf, "PICK_NAME")))


class TestApplyAssignments(KconfigFixture):

    def test_set_prompted_bool_succeeds(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(
            kconf, [("PROMPTED_BOOL", "y")])
        self.assertEqual(failed, [])
        self.assertEqual(applied,
                         [("CONFIG_PROMPTED_BOOL", "n", "y")])
        self.assertEqual(self.sym(kconf, "PROMPTED_BOOL").str_value, "y")

    def test_set_int_succeeds(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(kconf, [("NUM", "8192")])
        self.assertEqual(failed, [])
        self.assertEqual(self.sym(kconf, "NUM").str_value, "8192")

    def test_set_hex_succeeds(self):
        kconf = self.load()
        _, failed = self.m.apply_assignments(kconf, [("HEXNUM", "0x20")])
        self.assertEqual(failed, [])
        self.assertEqual(self.sym(kconf, "HEXNUM").str_value, "0x20")

    def test_set_string_succeeds(self):
        kconf = self.load()
        _, failed = self.m.apply_assignments(kconf, [("TEXT", "bye")])
        self.assertEqual(failed, [])
        self.assertEqual(self.sym(kconf, "TEXT").str_value, "bye")

    def test_promptless_symbol_is_reported(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(
            kconf, [("PROMPTLESS_BOOL", "n")])
        self.assertEqual(applied, [])
        self.assertEqual(len(failed), 1)
        self.assertEqual(failed[0][0], "CONFIG_PROMPTLESS_BOOL")
        self.assertIn("no prompt", failed[0][1])

    def test_hidden_symbol_reports_dependency(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(kconf,
                                                  [("GUARDED", "y")])
        self.assertEqual(applied, [])
        self.assertEqual(len(failed), 1)
        self.assertIn("GUARD", failed[0][1])

    def test_hidden_symbol_succeeds_when_guard_set_first(self):
        '''
        Regression guard for assign-then-verify: GUARDED is invisible
        until GUARD is set, so a validate-first implementation would
        wrongly reject this batch.
        '''
        kconf = self.load()
        applied, failed = self.m.apply_assignments(
            kconf, [("GUARD", "y"), ("GUARDED", "y")])
        self.assertEqual(failed, [])
        self.assertEqual(len(applied), 2)
        self.assertEqual(self.sym(kconf, "GUARDED").str_value, "y")

    def test_unknown_symbol_is_reported(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(kconf,
                                                  [("NOPE_XYZ", "y")])
        self.assertEqual(applied, [])
        self.assertEqual(failed, [("CONFIG_NOPE_XYZ", "unknown symbol")])

    def test_invalid_value_for_type_is_reported(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(kconf, [("NUM", "abc")])
        self.assertEqual(applied, [])
        self.assertEqual(len(failed), 1)
        self.assertIn("invalid value", failed[0][1])
        self.assertIn("int", failed[0][1])

    def test_choice_sibling_is_deselected(self):
        kconf = self.load()
        _, failed = self.m.apply_assignments(kconf, [("PICK_B", "y")])
        self.assertEqual(failed, [])
        self.assertEqual(self.sym(kconf, "PICK_A").str_value, "n")
        self.assertEqual(self.sym(kconf, "PICK_B").str_value, "y")

    def test_derived_string_follows_choice(self):
        kconf = self.load()
        self.assertEqual(self.sym(kconf, "PICK_NAME").str_value, "a")
        self.m.apply_assignments(kconf, [("PICK_B", "y")])
        self.assertEqual(self.sym(kconf, "PICK_NAME").str_value, "b")

    def test_unchanged_value_still_counts_as_applied(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(kconf, [("NUM", "4096")])
        self.assertEqual(failed, [])
        self.assertEqual(applied, [("CONFIG_NUM", "4096", "4096")])

    def test_partial_batch_reports_only_the_bad_one(self):
        kconf = self.load()
        applied, failed = self.m.apply_assignments(
            kconf, [("NUM", "8192"), ("NOPE_XYZ", "y")])
        self.assertEqual(len(applied), 1)
        self.assertEqual(len(failed), 1)


class TestApplyUnsets(KconfigFixture):

    def test_unset_restores_default(self):
        kconf = self.load()
        self.m.apply_assignments(kconf, [("NUM", "8192")])
        unset, failed = self.m.apply_unsets(kconf, ["CONFIG_NUM"])
        self.assertEqual(failed, [])
        self.assertEqual(unset, [("CONFIG_NUM", "8192", "4096")])
        self.assertIsNone(self.sym(kconf, "NUM").user_value)

    def test_unset_unknown_symbol_is_reported(self):
        kconf = self.load()
        unset, failed = self.m.apply_unsets(kconf, ["NOPE_XYZ"])
        self.assertEqual(unset, [])
        self.assertEqual(failed, [("CONFIG_NOPE_XYZ", "unknown symbol")])


class TestKconfigConfigEnv(KconfigFixture):

    def test_env_restored_after_block(self):
        os.environ["KCONFIG_CONFIG"] = "sentinel"
        self.addCleanup(os.environ.pop, "KCONFIG_CONFIG", None)
        with self.m.kconfig_config_env("other"):
            self.assertEqual(os.environ["KCONFIG_CONFIG"], "other")
        self.assertEqual(os.environ["KCONFIG_CONFIG"], "sentinel")

    def test_env_absent_stays_absent(self):
        os.environ.pop("KCONFIG_CONFIG", None)
        with self.m.kconfig_config_env("other"):
            self.assertEqual(os.environ["KCONFIG_CONFIG"], "other")
        self.assertNotIn("KCONFIG_CONFIG", os.environ)

    def test_env_restored_on_exception(self):
        os.environ.pop("KCONFIG_CONFIG", None)
        with self.assertRaises(RuntimeError):
            with self.m.kconfig_config_env("other"):
                raise RuntimeError("boom")
        self.assertNotIn("KCONFIG_CONFIG", os.environ)

    def test_load_kconfig_does_not_leak_env(self):
        os.environ.pop("KCONFIG_CONFIG", None)
        self.load()
        self.assertNotIn("KCONFIG_CONFIG", os.environ)


class TestWriteConfigFiles(KconfigFixture):

    def test_writes_both_files(self):
        kconf = self.load()
        self.m.apply_assignments(kconf, [("NUM", "8192")])
        self.m.write_config_files(kconf, self.dot_config, self.min_config)
        self.assertTrue(os.path.exists(self.dot_config))
        self.assertTrue(os.path.exists(self.min_config))

    def test_does_not_create_dot_old(self):
        kconf = self.load()
        self.m.write_config_files(kconf, self.dot_config, self.min_config)
        self.m.write_config_files(kconf, self.dot_config, self.min_config)
        self.assertFalse(os.path.exists(self.dot_config + ".old"))

    def test_min_config_omits_defaulted_symbols(self):
        kconf = self.load()
        self.m.apply_assignments(kconf, [("NUM", "8192")])
        self.m.write_config_files(kconf, self.dot_config, self.min_config)
        with open(self.min_config, encoding='utf-8') as f:
            body = f.read()
        self.assertIn("CONFIG_NUM=8192", body)
        self.assertNotIn("CONFIG_HEXNUM", body)
        self.assertNotIn("CONFIG_TEXT", body)

    def test_save_min_false_leaves_min_config_untouched(self):
        kconf = self.load()
        with open(self.min_config, 'w', encoding='utf-8') as f:
            f.write("ORIGINAL\n")
        self.m.apply_assignments(kconf, [("NUM", "8192")])
        self.m.write_config_files(kconf, self.dot_config, self.min_config,
                                  save_min=False)
        with open(self.min_config, encoding='utf-8') as f:
            self.assertEqual(f.read(), "ORIGINAL\n")
        with open(self.dot_config, encoding='utf-8') as f:
            self.assertIn("CONFIG_NUM=8192", f.read())

    def test_roundtrip_through_min_config(self):
        kconf = self.load()
        self.m.apply_assignments(kconf, [("NUM", "8192"), ("PICK_B", "y")])
        self.m.write_config_files(kconf, self.dot_config, self.min_config)
        reloaded = self.m.load_kconfig(self.kconfig_path, self.min_config)
        self.assertEqual(self.sym(reloaded, "NUM").str_value, "8192")
        self.assertEqual(self.sym(reloaded, "PICK_NAME").str_value, "b")


class TestSymbolValueAndInfo(KconfigFixture):

    def test_symbol_value_types(self):
        kconf = self.load()
        self.assertIs(self.m.symbol_value(
            self.sym(kconf, "PROMPTED_BOOL")), False)
        self.assertEqual(self.m.symbol_value(self.sym(kconf, "NUM")), 4096)
        self.assertEqual(self.m.symbol_value(self.sym(kconf, "HEXNUM")), 16)
        self.assertEqual(self.m.symbol_value(self.sym(kconf, "TEXT")),
                         "hello")

    def test_info_includes_type_prompt_and_visibility(self):
        kconf = self.load()
        info = self.m.symbol_info(self.sym(kconf, "PROMPTED_BOOL"))
        self.assertEqual(info["name"], "CONFIG_PROMPTED_BOOL")
        self.assertEqual(info["type"], "bool")
        self.assertEqual(info["prompt"], "prompted bool")
        self.assertEqual(info["visibility"], "y")
        self.assertTrue(info["configurable"])

    def test_info_reports_dependency(self):
        kconf = self.load()
        info = self.m.symbol_info(self.sym(kconf, "GUARDED"))
        self.assertEqual(info["depends_on"], "GUARD")
        self.assertEqual(info["visibility"], "n")

    def test_info_has_no_dependency_for_free_symbol(self):
        kconf = self.load()
        info = self.m.symbol_info(self.sym(kconf, "NUM"))
        self.assertIsNone(info["depends_on"])
        self.assertIsNone(info["selected_by"])

    def test_promptless_symbol_marked_not_configurable(self):
        kconf = self.load()
        info = self.m.symbol_info(self.sym(kconf, "PROMPTLESS_BOOL"))
        self.assertFalse(info["configurable"])
        self.assertIsNone(info["prompt"])

    def test_info_is_json_serialisable(self):
        kconf = self.load()
        for name in ("PROMPTED_BOOL", "NUM", "HEXNUM", "TEXT",
                     "GUARDED", "PICK_NAME", "PROMPTLESS_BOOL"):
            info = self.m.symbol_info(self.sym(kconf, name))
            self.assertEqual(json.loads(json.dumps(info)), info)

    def test_defined_at_uses_forward_slashes(self):
        kconf = self.load()
        info = self.m.symbol_info(self.sym(kconf, "NUM"))
        self.assertTrue(info["defined_at"])
        self.assertNotIn("\\", info["defined_at"][0])

    def test_format_symbol_info_is_multiline(self):
        kconf = self.load()
        text = self.m.format_symbol_info(self.sym(kconf, "GUARDED"))
        self.assertIn("CONFIG_GUARDED=", text)
        self.assertIn("type", text)
        self.assertIn("depends on", text)


class TestMatchName(KconfigFixture):

    def test_none_matches_all(self):
        self.assertTrue(self.m.match_name("CONFIG_ANY", None))
        self.assertTrue(self.m.match_name("CONFIG_ANY", ""))

    def test_substring_is_case_insensitive(self):
        self.assertTrue(self.m.match_name("CONFIG_ENABLE_WIFI", "wifi"))
        self.assertTrue(self.m.match_name("CONFIG_ENABLE_WIFI", "WIFI"))

    def test_non_match(self):
        self.assertFalse(self.m.match_name("CONFIG_ENABLE_WIFI", "bt"))

    def test_glob_pattern(self):
        self.assertTrue(self.m.match_name("CONFIG_ENABLE_WIFI",
                                          "CONFIG_ENABLE_*"))
        self.assertFalse(self.m.match_name("CONFIG_ENABLE_WIFI",
                                           "CONFIG_BOARD_*"))


class TestIterConfigLinesAndSnapshots(KconfigFixture):

    def test_includes_not_set_comment_lines(self):
        kconf = self.load()
        lines = list(self.m.iter_config_lines(kconf))
        self.assertIn("# CONFIG_PROMPTED_BOOL is not set", lines)

    def test_respects_pattern(self):
        kconf = self.load()
        lines = list(self.m.iter_config_lines(kconf, "PICK"))
        self.assertTrue(lines)
        for line in lines:
            self.assertIn("PICK", line)

    def test_order_matches_kconfig_order(self):
        kconf = self.load()
        lines = list(self.m.iter_config_lines(kconf))
        joined = "\n".join(lines)
        self.assertLess(joined.index("PROMPTED_BOOL"), joined.index("NUM"))
        self.assertLess(joined.index("NUM"), joined.index("TEXT"))

    def test_config_snapshot_is_typed(self):
        kconf = self.load()
        snapshot = self.m.config_snapshot(kconf)
        self.assertEqual(snapshot["CONFIG_NUM"], 4096)
        self.assertEqual(snapshot["CONFIG_HEXNUM"], 16)
        self.assertIs(snapshot["CONFIG_PROMPTED_BOOL"], False)

    def test_raw_snapshot_is_strings(self):
        kconf = self.load()
        snapshot = self.m.raw_snapshot(kconf)
        self.assertEqual(snapshot["CONFIG_NUM"], "4096")
        self.assertEqual(snapshot["CONFIG_HEXNUM"], "0x10")
        self.assertEqual(snapshot["CONFIG_PROMPTED_BOOL"], "n")

    def test_snapshot_respects_pattern(self):
        kconf = self.load()
        snapshot = self.m.config_snapshot(kconf, "PICK")
        self.assertTrue(snapshot)
        for name in snapshot:
            self.assertIn("PICK", name)


class TestIdentityValuesMissing(KconfigFixture):

    def test_missing_identity_syms_are_omitted(self):
        kconf = self.load()
        self.assertEqual(self.m.identity_values(kconf), {})


class TestIdentityValuesPresent(KconfigFixture):

    KCONFIG_TEXT = IDENTITY_KCONFIG

    def test_reports_present_identity_syms(self):
        kconf = self.load()
        self.assertEqual(self.m.identity_values(kconf),
                         {"PLATFORM_CHOICE": "T5AI",
                          "CHIP_CHOICE": "t5ai"})

    def test_detects_change(self):
        kconf = self.load()
        before = self.m.identity_values(kconf)
        self.m.apply_assignments(kconf, [("PLATFORM_CHOICE", "ESP32")])
        after = self.m.identity_values(kconf)
        self.assertNotEqual(before, after)
        self.assertEqual(after["PLATFORM_CHOICE"], "ESP32")


if __name__ == "__main__":
    unittest.main()
