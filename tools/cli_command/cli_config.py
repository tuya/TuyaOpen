#!/usr/bin/env python3
# coding=utf-8
#
# Usage examples:
#   tos.py config choice                        # interactive selection from app configs
#   tos.py config choice -d                     # interactive selection from board default configs
#   tos.py config choice -c my_board.config     # non-interactive, specify config by name
#   tos.py config choice -d -c my_board.config  # non-interactive, from board default configs
#   tos.py config choice -l                     # list all available app configs
#   tos.py config choice -d -l                  # list all board default configs
#   tos.py config menu                          # open menuconfig UI
#
#   tos.py config set CONFIG_ENABLE_LIBLVGL=y   # non-interactive set
#   tos.py config set BOARD_CHOICE_ESP32=y      # CONFIG_ prefix is optional
#   tos.py config set -u CONFIG_ENABLE_LIBLVGL  # revert to Kconfig default
#   tos.py config get CONFIG_PLATFORM_CHOICE    # print one value
#   tos.py config get -a CONFIG_ENABLE_WIFI     # type / prompt / deps
#   tos.py config list -p MBEDTLS               # filtered dump
#   tos.py config list -j                       # JSON, for scripts
#   tos.py config diff t5ai_default             # compare two configs
#   tos.py config save                          # interactive save
#   tos.py config save -n my_board -f           # non-interactive save

import os
import sys
import json
import click
from kconfiglib import Kconfig
from menuconfig import menuconfig

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params,
    check_proj_dir, list_menu
)
from tools.cli_command.util_files import (
    get_files_from_path, copy_file
)
from tools.cli_command.cli_clean import full_clean_project
from tools.kconfiglib.set_catalog_config import set_catalog_config
from tools.cli_command import util_kconfig as ukc


# Generated from using.config by tools/kconfiglib/CMakeLists.txt, which
# guards both with `if(NOT EXISTS ...)`. Changing using.config without
# removing these leaves the C macros and the component list stale.
DERIVED_ARTIFACTS = (
    ("cache", "using.cmake"),
    ("include", "tuya_kconfig.h"),
)


def _defconfig(config, dconfig=".config", kconfig="Kconfig",
               save_old=False):
    '''
    minimum config -> .config, with kconfig
    '''
    os.environ['KCONFIG_CONFIG'] = dconfig
    kconf = Kconfig(kconfig, suppress_traceback=True)
    kconf.load_config(config)
    kconf.write_config(dconfig, save_old=save_old)
    pass


def _savedefconfig(config, dconfig=".config", kconfig="Kconfig"):
    '''
    .config -> minimum config, with kconfig
    '''
    os.environ['KCONFIG_CONFIG'] = dconfig
    kconf = Kconfig(kconfig, suppress_traceback=True)
    kconf.load_config()
    kconf.write_min_config(config)
    # print(kconf.load_config())
    # print(kconf.write_min_config(config))
    pass


def init_using_config(force=False):
    '''
    1. Generate CatalogKconfig file
    2. Generate using.config file form app_default.config
    force: Forced using.config file update
    '''
    logger = get_logger()
    logger.info("Initialing using.config ...")
    params = get_global_params()
    board_path = params["boards_root"]
    src_path = params["src_root"]
    app_root = params["app_root"]
    catalog_kconfig = params["catalog_kconfig"]
    set_catalog_config(board_path, src_path, app_root, catalog_kconfig)

    app_default_config = params["app_default_config"]
    if not os.path.exists(app_default_config):
        tools_root = params["tools_root"]
        template = os.path.join(tools_root, "kconfiglib", "app_default.config")
        copy_file(template, app_default_config)
        pass

    using_config = params["using_config"]
    if force or not os.path.exists(using_config):
        _defconfig(app_default_config, using_config, catalog_kconfig)
    pass


def get_board_config_dir(board_path):
    ret = []
    for entry in os.scandir(board_path):
        if entry.is_dir():
            # TuyaOpen/boards/xxx/config
            ret.append(os.path.join(entry, "config"))

    return ret


def _invalidate_derived_artifacts():
    '''
    Remove the cmake/header files derived from using.config so the next
    build regenerates them. Returns the paths actually removed.

    os.remove rather than rm_rf: these are single files, and rm_rf
    shells out to `del /F /Q` on Windows for no benefit here.
    '''
    logger = get_logger()
    params = get_global_params()
    build_path = params["app_build_path"]
    removed = []
    for parts in DERIVED_ARTIFACTS:
        target = os.path.join(build_path, *parts)
        if not os.path.exists(target):
            continue
        try:
            os.remove(target)
            removed.append(target)
        except OSError as e:
            logger.warning(f"Cannot remove [{target}]: {e}")
    return removed


def _normalize_save_name(name):
    '''
    Validate a config name and append the .config suffix.
    Returns "" when the name is unusable, so the caller decides how to
    fail. Path separators and '..' are rejected: this name is joined
    onto the app config directory.
    '''
    if not name or not name.strip():
        return ""
    name = name.strip()
    if "/" in name or "\\" in name or os.sep in name:
        return ""
    if ".." in name:
        return ""
    if not name.endswith(".config"):
        name += ".config"
    return name


def _load_current_kconfig():
    '''
    Bootstrap the config state if needed, then load the Kconfig tree
    with the current using.config applied.

    Deliberately does NOT call full_clean_project(): unlike `choice`
    and `menu`, the read/modify commands must keep the .build tree they
    are about to read from.
    '''
    init_using_config(force=False)
    params = get_global_params()
    return ukc.load_kconfig(params["catalog_kconfig"],
                            params["using_config"])


def _list_known_configs():
    '''
    Every selectable minimal config: the app's own config dir plus each
    board's default config dir.
    '''
    params = get_global_params()
    configs = get_files_from_path(".config",
                                 params["app_configs_path"], 1)
    board_dirs = get_board_config_dir(params["boards_root"])
    configs += get_files_from_path(".config", board_dirs, 0)
    return sorted(configs)


def _resolve_config_path(name):
    '''
    Resolve a config reference to a path. Accepts a bare name
    ("t5ai_default"), a file name ("t5ai_default.config") or an
    existing path. Returns None when nothing matches.
    '''
    if os.path.isfile(name):
        return name
    wanted = name if name.endswith(".config") else name + ".config"
    for path in _list_known_configs():
        if os.path.basename(path) == wanted:
            return path
    return None


def _expand_min_config(min_config):
    '''
    Expand a minimal config into {CONFIG_NAME: value} through the real
    Kconfig tree. Diffing the minimal files textually would lie: the
    same effective config has several valid minimal representations.
    '''
    params = get_global_params()
    kconf = ukc.load_kconfig(params["catalog_kconfig"], min_config)
    return ukc.raw_snapshot(kconf)


@click.command(help="Choice config file.")
@click.option('-d', '--default',
              is_flag=True, default=False,
              help="Only display board default config.")
@click.option('-c', '--config',
              default=None, metavar='NAME',
              help="Specify config file name directly (e.g. my_board.config), skipping interactive selection.")
@click.option('-l', '--list', 'list_configs',
              is_flag=True, default=False,
              help="List all available config files.")
def config_choice_exec(default, config, list_configs):
    '''
    Choice config file
    from app config or board default config
    '''
    logger = get_logger()
    params = get_global_params()

    # get config files
    app_configs_path = params["app_configs_path"]
    if (not default) and os.path.exists(app_configs_path):
        logger.debug("Choice from app config")
        config_list = get_files_from_path(".config", app_configs_path, 1)
    else:
        logger.debug("Choice from board config")
        board_path = params["boards_root"]
        config_dir = get_board_config_dir(board_path)
        config_list = get_files_from_path(".config", config_dir, 0)

    config_list.sort()

    if list_configs:
        show_list = [os.path.basename(f) for f in config_list]
        for name in show_list:
            print(name)
        sys.exit(0)

    full_clean_project()

    if config is not None:
        # non-interactive: match by filename
        if not config.endswith(".config"):
            config += ".config"
        matched = [f for f in config_list if os.path.basename(f) == config]
        if not matched:
            show_list = [os.path.basename(f) for f in config_list]
            logger.error(f"Config '{config}' not found. Available: {show_list}")
            sys.exit(1)
        choice_config = matched[0]
    else:
        # interactive selection
        show_list = [os.path.basename(conf) for conf in config_list]
        _, index = list_menu("Choice config file", show_list)
        choice_config = config_list[index]

    # copy config file
    app_default_config = params["app_default_config"]
    copy_file(choice_config, app_default_config)
    init_using_config(force=True)
    logger.note(f"Choice config: {choice_config}")
    sys.exit(0)


@click.command(help="Menuconfig.")
def config_menu_exec():
    '''
    1. menuconfig
    2. Save minimal: using.config -> app_default.config
    '''
    full_clean_project()
    init_using_config(force=False)
    params = get_global_params()
    using_config = params["using_config"]
    catalog_kconfig = params["catalog_kconfig"]
    app_default_config = params["app_default_config"]

    os.environ['KCONFIG_CONFIG'] = using_config
    kconf = Kconfig(filename=catalog_kconfig)
    menuconfig(kconf)
    _savedefconfig(app_default_config, using_config, catalog_kconfig)
    sys.exit(0)


@click.command(help="Set config symbols, non-interactively. "
                    "The CONFIG_ prefix is optional. Use '--' before "
                    "a value that starts with '-'.")
@click.argument('assignments', nargs=-1, metavar='CONFIG_NAME=VALUE...')
@click.option('-u', '--unset', 'unsets',
              multiple=True, metavar='NAME',
              help="Revert a symbol to its Kconfig default. Repeatable.")
@click.option('--no-save',
              is_flag=True, default=False,
              help="Only update using.config; leave app_default.config "
                   "untouched. The change is lost on the next clean, "
                   "and a later set/menu will persist it.")
@click.option('-k', '--keep-build',
              is_flag=True, default=False,
              help="Do not invalidate generated build artifacts.")
def config_set_exec(assignments, unsets, no_save, keep_build):
    '''
    1. Apply assignments through kconfiglib (dependency-aware)
    2. using.config  <- expanded result
    3. app_default.config <- minimal result (unless --no-save)
    4. Invalidate the build artifacts derived from using.config
    '''
    logger = get_logger()

    if not assignments and not unsets:
        logger.error("Nothing to do: give CONFIG_NAME=VALUE or -u NAME.")
        sys.exit(1)

    # Parse everything before touching any state, so a typo in the last
    # token cannot leave a half-applied config behind.
    pairs = []
    bad = False
    for token in assignments:
        try:
            pairs.append(ukc.parse_assignment(token))
        except ValueError as e:
            logger.error(str(e))
            bad = True
    if bad:
        sys.exit(1)

    seen = set()
    for name, _ in pairs:
        if name in seen:
            logger.warning(f"CONFIG_{name} given more than once; "
                           "the last value wins.")
        seen.add(name)

    params = get_global_params()
    using_config = params["using_config"]
    app_default_config = params["app_default_config"]

    kconf = _load_current_kconfig()
    before = ukc.identity_values(kconf)

    # Unsets first, then assignments: the CLI cannot express an
    # interleaved order anyway, and this way an explicit value always
    # wins over an unset of the same symbol.
    changes, failed = ukc.apply_unsets(kconf, unsets)
    applied, set_failed = ukc.apply_assignments(kconf, pairs)
    changes += applied
    failed += set_failed

    if failed:
        for name, reason in failed:
            logger.error(f"{name}: {reason}")
        logger.note("Run [tos.py config get -a NAME] to inspect a symbol.")
        sys.exit(1)

    for name, old, new in changes:
        if old == new:
            logger.info(f"{name} already {new}")
        else:
            logger.note(f"{name}: {old} -> {new}")

    after = ukc.identity_values(kconf)
    identity_changed = before != after
    identity_diff = ", ".join(k for k in after
                              if before.get(k) != after[k])

    # Switching platform/board forces a full clean, and a full clean
    # rebuilds using.config from app_default.config -- which --no-save
    # leaves holding the old board. The combination would silently
    # discard the change, so refuse it before anything is written.
    if identity_changed and no_save:
        logger.error(f"Platform/board identity changed ({identity_diff}); "
                     "--no-save cannot express that, because the full "
                     "clean it requires would discard the change.")
        logger.note("Drop --no-save, or use [tos.py config choice].")
        sys.exit(1)

    try:
        ukc.write_config_files(kconf, using_config, app_default_config,
                              save_min=not no_save)
    except OSError as e:
        logger.error(f"Cannot write config: {e}")
        sys.exit(1)

    if not no_save:
        # Re-derive so using.config is byte-identical to what
        # [config choice] would produce from the new app_default.config.
        # Skipped under --no-save, where app_default.config still holds
        # the old values and would undo what we just did.
        _defconfig(app_default_config, using_config,
                   params["catalog_kconfig"])

    if keep_build:
        if identity_changed:
            logger.warning("Platform/board identity changed "
                           f"({identity_diff}) but -k was given; run "
                           "[tos.py clean -f] before building.")
    elif identity_changed:
        logger.warning("Platform/board identity changed "
                       f"({identity_diff}); full clean needed.")
        logger.warning("Existing values were carried over. Use "
                       "[tos.py config choice] for a clean switch.")
        full_clean_project()
        init_using_config(force=True)
    else:
        for path in _invalidate_derived_artifacts():
            logger.info(f"Invalidated: {path}")

    if no_save:
        logger.note(f"Updated: {using_config}")
        logger.warning("--no-save: app_default.config is unchanged, so "
                       "this only affects the next build.")
    else:
        logger.note(f"Updated: {app_default_config}")
    sys.exit(0)


@click.command(help="Print config symbol value(s).")
@click.argument('names', nargs=-1, required=True, metavar='CONFIG_NAME...')
@click.option('-a', '--all', 'show_all',
              is_flag=True, default=False,
              help="Show type, prompt, visibility and dependencies.")
@click.option('-j', '--json', 'as_json',
              is_flag=True, default=False,
              help="Output as JSON.")
def config_get_exec(names, show_all, as_json):
    '''
    Read-only. Values go to stdout, diagnostics to stderr, so
    --json output can be piped straight into a parser.
    '''
    logger = get_logger()
    kconf = _load_current_kconfig()

    missing = []
    payload = {}
    blocks = []
    for name in names:
        cfg_name = ukc.full_name(name)
        sym = ukc.find_symbol(kconf, name)
        if sym is None:
            missing.append(cfg_name)
            payload[cfg_name] = None
            continue
        if show_all:
            payload[cfg_name] = ukc.symbol_info(sym)
            blocks.append(ukc.format_symbol_info(sym))
        else:
            payload[cfg_name] = ukc.symbol_value(sym)
            if len(names) == 1:
                blocks.append(sym.str_value)
            else:
                blocks.append(f"{cfg_name}={sym.str_value}")

    if as_json:
        print(json.dumps(payload, indent=2))
    else:
        for block in blocks:
            print(block)

    for name in missing:
        logger.error(f"{name}: unknown symbol")
    sys.exit(1 if missing else 0)


@click.command(help="List the effective config.")
@click.option('-p', '--pattern',
              default=None, metavar='PATTERN',
              help="Filter symbol names by substring, or by glob if "
                   "the pattern contains any of *?[.")
@click.option('-j', '--json', 'as_json',
              is_flag=True, default=False,
              help="Output as JSON.")
def config_list_exec(pattern, as_json):
    '''
    Read-only dump of using.config, read through kconfiglib so that
    unset symbols show up as '# CONFIG_X is not set'.
    '''
    logger = get_logger()
    kconf = _load_current_kconfig()

    if as_json:
        print(json.dumps(ukc.config_snapshot(kconf, pattern), indent=2))
        sys.exit(0)

    count = 0
    for line in ukc.iter_config_lines(kconf, pattern):
        print(line)
        count += 1
    if not count:
        logger.warning(f"No symbol matched [{pattern}].")
    sys.exit(0)


@click.command(help="Compare two configs, expanded through Kconfig.")
@click.argument('config_a', metavar='CONFIG_A')
@click.argument('config_b', required=False, metavar='[CONFIG_B]')
@click.option('-j', '--json', 'as_json',
              is_flag=True, default=False,
              help="Output as JSON.")
def config_diff_exec(config_a, config_b, as_json):
    '''
    CONFIG_A/CONFIG_B are config names (resolved against the app config
    dir and every board config dir) or paths. CONFIG_B defaults to the
    current app_default.config.
    '''
    logger = get_logger()
    init_using_config(force=False)
    params = get_global_params()

    path_a = _resolve_config_path(config_a)
    if path_a is None:
        logger.error(f"Config [{config_a}] not found.")
        sys.exit(1)

    if config_b is None:
        path_b = params["app_default_config"]
        label_b = "app_default.config"
    else:
        path_b = _resolve_config_path(config_b)
        if path_b is None:
            logger.error(f"Config [{config_b}] not found.")
            sys.exit(1)
        label_b = os.path.basename(path_b)
    label_a = os.path.basename(path_a)

    snap_a = _expand_min_config(path_a)
    snap_b = _expand_min_config(path_b)

    diff = {}
    for name in sorted(set(snap_a) | set(snap_b)):
        value_a = snap_a.get(name)
        value_b = snap_b.get(name)
        if value_a != value_b:
            diff[name] = {"a": value_a, "b": value_b}

    if as_json:
        print(json.dumps({"a": label_a, "b": label_b, "diff": diff},
                         indent=2))
        sys.exit(0)

    if not diff:
        logger.note(f"No difference between {label_a} and {label_b}.")
        sys.exit(0)

    print(f"a: {label_a}")
    print(f"b: {label_b}")
    for name, values in diff.items():
        value_a = values["a"] if values["a"] is not None else "(absent)"
        value_b = values["b"] if values["b"] is not None else "(absent)"
        print(f"{name}: {value_a} -> {value_b}")
    sys.exit(0)


@click.command(help="Save minimal config.")
@click.option('-n', '--name',
              default=None, metavar='NAME',
              help="Config name to save (e.g. my_board). "
                   "Non-interactive; prompts when omitted.")
@click.option('-f', '--force',
              is_flag=True, default=False,
              help="Overwrite an existing config file.")
def config_save_exec(name, force):
    '''
    1. Copy: app_default.config -> $APP_TOOT/config/xxx.config
    '''
    logger = get_logger()
    params = get_global_params()
    app_default_config = params["app_default_config"]

    if not os.path.exists(app_default_config):
        logger.error("Please run [tos.py config menu] "
                     "or [tos.py config set] first.")
        sys.exit(1)

    interactive = name is None
    if interactive:
        # isatty() is not dependable everywhere (MSYS/Git Bash reports a
        # tty for a redirected stdin), so EOFError is the real guard
        # against hanging or dying with a bare "Aborted!" in CI.
        if not sys.stdin.isatty():
            logger.error("Not a TTY: pass the name with -n NAME.")
            sys.exit(1)
        try:
            name = input("Input save config name: ")
        except EOFError:
            logger.error("No input available: pass the name with -n NAME.")
            sys.exit(1)

    saveconfig_name = _normalize_save_name(name)
    if not saveconfig_name:
        logger.error(f"Invalid config name [{name}].")
        sys.exit(1)

    app_configs_path = params["app_configs_path"]
    saveconfig = os.path.join(app_configs_path, saveconfig_name)
    # The overwrite guard is scoped to -n so that the interactive flow
    # keeps behaving exactly as before.
    if not interactive and not force and os.path.exists(saveconfig):
        logger.error(f"[{saveconfig}] exists, use -f to overwrite.")
        sys.exit(1)

    if not copy_file(app_default_config, saveconfig):
        logger.error(f"Cannot save [{saveconfig}].")
        sys.exit(1)
    logger.note(f"Success save: {saveconfig}")
    sys.exit(0)


CLIS = {
    "choice": config_choice_exec,
    "menu": config_menu_exec,
    "set": config_set_exec,
    "get": config_get_exec,
    "list": config_list_exec,
    "diff": config_diff_exec,
    "save": config_save_exec
}


##
# @brief tos.py config
#
@click.command(cls=set_clis(CLIS),
               help="Configuration file operation.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    check_proj_dir()
    pass
