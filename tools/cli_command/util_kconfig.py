#!/usr/bin/env python3
# coding=utf-8
##
# @file util_kconfig.py
# @brief Non-interactive Kconfig helpers built on kconfiglib.
#
# Pure logic only: no click, no sys.exit(), no path discovery. Callers
# pass every path in. This keeps the module unit-testable without the
# global state that tools/cli_command/util.py owns.

import os
import fnmatch
import contextlib

import kconfiglib
from kconfiglib import Kconfig, expr_str, TYPE_TO_STR, TRI_TO_STR

from tools.cli_command.util import get_logger


CONFIG_PREFIX = "CONFIG_"

# Symbols that pick the toolchain. cmake_configure() bakes these into
# CMakeCache.txt as -DTOS_PROJECT_*, so changing one of them makes an
# incremental reconfigure unreliable and requires a full clean.
IDENTITY_SYMS = ("PLATFORM_CHOICE", "CHIP_CHOICE",
                 "BOARD_CHOICE", "FRAMEWORK_CHOICE")

_TRI_TYPES = (kconfiglib.BOOL, kconfiglib.TRISTATE)

_BOOL_WORDS = {
    "y": "y", "yes": "y", "true": "y", "1": "y", "on": "y",
    "n": "n", "no": "n", "false": "n", "0": "n", "off": "n",
    "m": "m", "module": "m",
}


@contextlib.contextmanager
def kconfig_config_env(dot_config):
    '''
    Point KCONFIG_CONFIG at dot_config for the block, then restore it.
    kconfiglib falls back to this variable whenever a filename is
    omitted; leaving it set leaks into every later subprocess (cmake,
    ninja, platform scripts), so it must not escape.
    '''
    had = "KCONFIG_CONFIG" in os.environ
    old = os.environ.get("KCONFIG_CONFIG")
    os.environ["KCONFIG_CONFIG"] = dot_config
    try:
        yield
    finally:
        if had:
            os.environ["KCONFIG_CONFIG"] = old
        else:
            os.environ.pop("KCONFIG_CONFIG", None)


def strip_prefix(name):
    '''
    CONFIG_FOO -> FOO ; FOO -> FOO. Strips one layer only, so a real
    symbol literally named CONFIG_FOO stays reachable as CONFIG_CONFIG_FOO.
    '''
    name = name.strip()
    if name.startswith(CONFIG_PREFIX):
        return name[len(CONFIG_PREFIX):]
    return name


def full_name(name):
    return CONFIG_PREFIX + strip_prefix(name)


def parse_assignment(token):
    '''
    'CONFIG_A=y' -> ('A', 'y')

    Splits on the first '=' only, so string values may contain '='.
    One layer of surrounding double quotes is stripped so both
    CONFIG_S="v" and CONFIG_S=v work. Raises ValueError when malformed.
    '''
    if "=" not in token:
        raise ValueError(f"no '=' in assignment: '{token}'")
    name, value = token.split("=", 1)
    name = strip_prefix(name)
    if not name:
        raise ValueError(f"empty symbol name: '{token}'")
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        value = value[1:-1]
    return name, value


def load_kconfig(catalog_kconfig, dot_config):
    '''
    Load the Kconfig tree and the current dot-config on top of it.
    Warnings are collected instead of printed so that --json output
    stays clean; they are re-emitted at debug level.
    '''
    logger = get_logger()
    with kconfig_config_env(dot_config):
        kconf = Kconfig(catalog_kconfig, warn_to_stderr=False,
                        suppress_traceback=True)
        if os.path.exists(dot_config):
            kconf.load_config(dot_config)
    for warning in kconf.warnings:
        logger.debug(warning)
    return kconf


def find_symbol(kconf, name):
    '''
    Look up a symbol by name, with or without the CONFIG_ prefix.
    Returns None for unknown names and for symbols that are merely
    referenced by an expression but never defined (no menu nodes).
    '''
    sym = kconf.syms.get(strip_prefix(name))
    if sym is None:
        sym = kconf.syms.get(strip_prefix(name).upper())
    if sym is None or not sym.nodes:
        return None
    return sym


def is_configurable(sym):
    '''
    True when the symbol has a prompt somewhere, i.e. a user can
    actually assign it. Prompt-less symbols are derived values or are
    forced by `default`/`select` in a board or platform Kconfig.
    '''
    return any(node.prompt for node in sym.nodes)


def normalize_value(sym, raw):
    '''
    Coerce a command-line string into the form kconfiglib expects.
    Returns (ok, value); ok is False when raw cannot be a value of
    this symbol's type.
    '''
    if sym.orig_type in _TRI_TYPES:
        word = _BOOL_WORDS.get(raw.strip().lower())
        if word is None:
            return False, raw
        if word == "m" and sym.orig_type is kconfiglib.BOOL:
            return False, raw
        return True, word

    if sym.orig_type is kconfiglib.INT:
        try:
            return True, str(int(raw.strip(), 10))
        except ValueError:
            return False, raw

    if sym.orig_type is kconfiglib.HEX:
        text = raw.strip()
        try:
            int(text, 16)
        except ValueError:
            return False, raw
        return True, text

    if sym.orig_type is kconfiglib.STRING:
        return True, raw

    return False, raw


def value_matches(sym, wanted):
    '''
    Type-aware comparison of a requested value against the symbol's
    effective value. int/hex compare numerically so that 0x10, 10 and
    16 do not read as three different values.
    '''
    current = sym.str_value
    if sym.orig_type is kconfiglib.INT:
        try:
            return int(current, 10) == int(wanted, 10)
        except ValueError:
            return False
    if sym.orig_type is kconfiglib.HEX:
        try:
            return int(current, 16) == int(wanted, 16)
        except ValueError:
            return False
    return current == wanted


def _short_expr(expression, limit=160):
    '''
    Dependency expressions in this tree can run to several hundred
    characters (every board OR'd together), which drowns the actual
    error. Keep the head and point at `config get -a` for the rest.
    '''
    text = expr_str(expression)
    if len(text) <= limit:
        return text
    return text[:limit].rstrip() + " ... (see 'config get -a')"


def diagnose_symbol(kconf, name, wanted):
    '''
    Explain why an assignment did not take effect. Only called for
    values that failed the verify pass, so the message may assume
    something is wrong.
    '''
    sym = find_symbol(kconf, name)
    if sym is None:
        return "unknown symbol"

    if not is_configurable(sym):
        return ("not user-configurable (no prompt); it is a derived "
                "value or is forced by a board/platform default")

    if sym.visibility == 0:
        return ("hidden by unmet dependencies: "
                f"{_short_expr(sym.direct_dep)}")

    if sym.orig_type in _TRI_TYPES:
        tri = kconfiglib.STR_TO_TRI.get(wanted)
        if tri is not None and tri not in sym.assignable:
            allowed = "/".join(TRI_TO_STR[t] for t in sym.assignable)
            reason = (f"'{wanted}' is not assignable here "
                      f"(allowed: {allowed or 'none'})")
            if sym.rev_dep is not kconf.n:
                reason += f"; selected by {_short_expr(sym.rev_dep)}"
            return reason

    type_name = TYPE_TO_STR[sym.orig_type]
    return (f"assignment did not take effect: still '{sym.str_value}' "
            f"(type {type_name})")


def apply_unsets(kconf, names):
    '''
    Revert symbols to their Kconfig defaults by dropping the user
    value. Returns (unset, failed); nothing is written to disk.
      unset  = [(CONFIG_NAME, old_str_value, new_str_value)]
      failed = [(CONFIG_NAME, reason)]
    '''
    unset = []
    failed = []
    touched = []
    for name in names:
        sym = find_symbol(kconf, name)
        if sym is None:
            failed.append((full_name(name), "unknown symbol"))
            continue
        old = sym.str_value
        sym.unset_value()
        touched.append((full_name(name), sym, old))
    for cfg_name, sym, old in touched:
        unset.append((cfg_name, old, sym.str_value))
    return unset, failed


def apply_assignments(kconf, pairs):
    '''
    Apply every assignment in order, then verify all of them.

    Assign-then-verify rather than validate-then-assign: a symbol can
    be invisible until an earlier assignment in the same batch reveals
    it, so pre-validating would reject legitimate ordered batches such
    as `CONFIG_PARENT=y CONFIG_CHILD=y`.

    Verification is mandatory because kconfiglib's set_value() returns
    True for prompt-less symbols whose value is then discarded.

    Returns (applied, failed); nothing is written to disk.
      applied = [(CONFIG_NAME, old_str_value, new_str_value)]
      failed  = [(CONFIG_NAME, reason)]
    '''
    applied = []
    failed = []
    resolved = []

    for name, raw in pairs:
        cfg_name = full_name(name)
        sym = find_symbol(kconf, name)
        if sym is None:
            failed.append((cfg_name, "unknown symbol"))
            continue
        ok, value = normalize_value(sym, raw)
        if not ok:
            type_name = TYPE_TO_STR[sym.orig_type]
            failed.append((cfg_name, f"invalid value '{raw}' for type "
                                     f"{type_name}"))
            continue
        old = sym.str_value
        if not sym.set_value(value):
            type_name = TYPE_TO_STR[sym.orig_type]
            failed.append((cfg_name, f"invalid value '{raw}' for type "
                                     f"{type_name}"))
            continue
        resolved.append((cfg_name, sym, value, old))

    for cfg_name, sym, value, old in resolved:
        if value_matches(sym, value):
            applied.append((cfg_name, old, sym.str_value))
        else:
            failed.append((cfg_name,
                           diagnose_symbol(kconf, sym.name, value)))

    return applied, failed


def symbol_value(sym):
    '''
    Typed python value, matching the conventions of
    util.parse_config_file so scripts see one shape:
    bool for bool, int for int/hex, str for string/tristate.
    '''
    text = sym.str_value
    if sym.orig_type is kconfiglib.BOOL:
        return text == "y"
    if sym.orig_type is kconfiglib.TRISTATE:
        return text
    if sym.orig_type is kconfiglib.INT:
        try:
            return int(text, 10)
        except ValueError:
            return None
    if sym.orig_type is kconfiglib.HEX:
        try:
            return int(text, 16)
        except ValueError:
            return None
    return text


def _prompt_text(sym):
    for node in sym.nodes:
        if node.prompt:
            return node.prompt[0]
    return None


def _user_value_str(sym):
    if sym.user_value is None:
        return None
    if sym.orig_type in _TRI_TYPES:
        return TRI_TO_STR[sym.user_value]
    return sym.user_value


def symbol_info(sym):
    '''
    JSON-serialisable description of one symbol, for `config get -a`.
    '''
    kconf = sym.kconfig
    direct = expr_str(sym.direct_dep)
    return {
        "name": CONFIG_PREFIX + sym.name,
        "value": symbol_value(sym),
        "str_value": sym.str_value,
        "type": TYPE_TO_STR[sym.orig_type],
        "prompt": _prompt_text(sym),
        "visibility": TRI_TO_STR[sym.visibility],
        "user_value": _user_value_str(sym),
        "assignable": [TRI_TO_STR[t] for t in sym.assignable],
        "configurable": is_configurable(sym),
        "depends_on": None if sym.direct_dep is kconf.y else direct,
        "selected_by": (None if sym.rev_dep is kconf.n
                        else expr_str(sym.rev_dep)),
        # CatalogKconfig sources with forward slashes while rsource
        # appends the OS separator, so filenames come back mixed.
        "defined_at": [f"{n.filename}:{n.linenr}".replace("\\", "/")
                       for n in sym.nodes],
    }


def format_symbol_info(sym):
    '''
    Multi-line human-readable block for `config get -a`.
    '''
    info = symbol_info(sym)
    lines = [f"{info['name']}={info['str_value']}"]
    lines.append(f"  type        : {info['type']}")
    lines.append(f"  prompt      : {info['prompt'] or '(none)'}")
    lines.append(f"  visibility  : {info['visibility']}")
    lines.append(f"  user value  : "
                 f"{info['user_value'] if info['user_value'] else '(unset)'}")
    lines.append(f"  assignable  : "
                 f"{'/'.join(info['assignable']) or '(none)'}")
    lines.append(f"  configurable: {'yes' if info['configurable'] else 'no'}")
    if info["depends_on"]:
        lines.append(f"  depends on  : {info['depends_on']}")
    if info["selected_by"]:
        lines.append(f"  selected by : {info['selected_by']}")
    for where in info["defined_at"]:
        lines.append(f"  defined at  : {where}")
    return "\n".join(lines)


def match_name(name, pattern):
    '''
    None matches everything. A pattern containing any of *?[ is treated
    as a glob, otherwise as a case-insensitive substring.
    '''
    if not pattern:
        return True
    upper_name = name.upper()
    upper_pattern = pattern.upper()
    if any(ch in pattern for ch in "*?["):
        return fnmatch.fnmatch(upper_name, upper_pattern)
    return upper_pattern in upper_name


def iter_config_lines(kconf, pattern=None):
    '''
    Yield one dot-config line per symbol, in Kconfig declaration order.
    Uses Symbol.config_string, so unset bools appear as
    '# CONFIG_X is not set' exactly as they do in using.config.
    Menu comment headers are not reproduced.
    '''
    for sym in kconf.unique_defined_syms:
        line = sym.config_string
        if not line:
            continue
        if not match_name(CONFIG_PREFIX + sym.name, pattern):
            continue
        yield line.rstrip("\n")


def config_snapshot(kconf, pattern=None):
    '''
    {CONFIG_NAME: typed value} for every writable symbol.
    '''
    snapshot = {}
    for sym in kconf.unique_defined_syms:
        if not sym.config_string:
            continue
        name = CONFIG_PREFIX + sym.name
        if not match_name(name, pattern):
            continue
        snapshot[name] = symbol_value(sym)
    return snapshot


def raw_snapshot(kconf, pattern=None):
    '''
    {CONFIG_NAME: Kconfig string value} for every writable symbol.
    Used for diffing, where 'n -> y' reads better than 'False -> True'
    and every value is already JSON-safe.
    '''
    snapshot = {}
    for sym in kconf.unique_defined_syms:
        if not sym.config_string:
            continue
        name = CONFIG_PREFIX + sym.name
        if not match_name(name, pattern):
            continue
        snapshot[name] = sym.str_value
    return snapshot


def identity_values(kconf):
    '''
    Current values of the toolchain-selecting symbols. A change here
    means the CMake cache cannot be reused.
    '''
    values = {}
    for name in IDENTITY_SYMS:
        sym = kconf.syms.get(name)
        if sym is not None:
            values[name] = sym.str_value
    return values


def write_config_files(kconf, dot_config, min_config, save_min=True):
    '''
    Persist the in-memory configuration.

    dot_config is written first on purpose: if the second write failed
    we would rather have a fresh using.config and a stale
    app_default.config (self-healing, since `config choice` and
    `clean -f` rebuild using.config from it) than the reverse, which
    init_using_config(force=False) would never notice.

    save_old=False keeps kconfiglib from dropping a using.config.old
    backup next to the real file.
    '''
    with kconfig_config_env(dot_config):
        kconf.write_config(dot_config, save_old=False)
        if save_min and min_config:
            kconf.write_min_config(min_config)
    pass
