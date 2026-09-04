#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params,
    parse_config_file
)
from tools.cli_command.cli_config import get_board_config_dir, init_using_config
from tools.cli_command.util_files import (
    get_files_from_path, copy_file, rm_rf
)
from tools.cli_command.cli_build import build_project
from tools.cli_command.cli_clean import full_clean_project

# Config names to skip in `tos.py dev bac` (with or without `.config` suffix).
BAC_SKIP_CONFIGS = [
    # "RaspberryPi",
    # "DNESP32S3_BOX2_WIFI",
   "GD32.config",
]


def _normalize_config_name(name):
    name = os.path.basename(name.strip())
    if name.endswith(".config"):
        name = name[:-7]
    return name


def _build_skip_set(skip_names):
    skip_set = set()
    for name in BAC_SKIP_CONFIGS + list(skip_names):
        if not name:
            continue
        for part in name.split(','):
            part = part.strip()
            if part:
                skip_set.add(_normalize_config_name(part))
    return skip_set


def _filter_config_list(config_list, skip_set):
    if not skip_set:
        return config_list, []

    filtered = []
    skipped = []
    for config in config_list:
        if _normalize_config_name(config) in skip_set:
            skipped.append(config)
        else:
            filtered.append(config)
    return filtered, skipped


def _save_product(dist, config_file):
    logger = get_logger()
    params = get_global_params()

    app_bin_path = params["app_bin_path"]
    using_config = params["using_config"]
    using_data = parse_config_file(using_config)
    app_name = using_data.get("CONFIG_PROJECT_NAME", "")
    app_ver = using_data.get("CONFIG_PROJECT_VERSION", "")
    bin_name = f"{app_name}_QIO_{app_ver}.bin"
    app_bin_file = os.path.join(app_bin_path, bin_name)
    if not os.path.exists(app_bin_file):
        logger.error(f"Not found {app_bin_file}")
        return

    config_basename = os.path.basename(config_file)
    if config_basename.endswith(".config"):
        config_basename = config_basename[:-7]
    bin_dist_name = f"{app_name}_{config_basename}_QIO_{app_ver}.bin"
    app_bin_dist_file = os.path.join(dist, bin_dist_name)
    rm_rf(app_bin_dist_file)
    copy_file(app_bin_file, app_bin_dist_file)
    pass


@click.command(help="Build all config.")
@click.option('-d', '--dist',
              type=str, default="",
              help="Save product path.")
@click.option('-o', '--log-dir',
              type=click.Path(),
              default=None,
              help="Write build log to a file in the specified directory (default: build.log).")
@click.option('-s', '--skip',
              multiple=True,
              help="Skip config(s) by name. Repeatable; comma-separated names are supported.")
@click.option('--force-clean-backup',
              is_flag=True, default=False,
              help="Discard a leftover backup from a previous interrupted "
                   "[dev bac] run and proceed. Only use this after you have "
                   "manually confirmed the leftover backup is not needed.")
def build_all_config_exec(dist, log_dir, skip, force_clean_backup):
    logger = get_logger()
    params = get_global_params()
    dist_abs = os.path.abspath(dist)

    if log_dir:
        os.makedirs(log_dir, exist_ok=True)

    # get config files
    app_configs_path = params["app_configs_path"]
    if os.path.exists(app_configs_path):
        logger.debug("Choice from app config")
        config_list = get_files_from_path(".config", app_configs_path, 1)
    else:
        logger.debug("Choice from board config")
        board_path = params["boards_root"]
        config_dir = get_board_config_dir(board_path)
        config_list = get_files_from_path(".config", config_dir, 0)

    # build all config
    app_default_config = params["app_default_config"]

    # A leftover backup means a previous `bac` run was killed mid-way
    # (SIGKILL, power loss, etc.) before its `finally` block below could
    # restore app_default.config and clean up. Refuse to proceed by
    # default: today's app_default.config may already be mid-loop test
    # content, and blindly overwriting the leftover backup with it would
    # silently destroy the last copy of the user's real original config.
    backup_path = app_default_config + ".bac_backup"
    if os.path.exists(backup_path) and not force_clean_backup:
        logger.error(
            f"Found a leftover backup from an interrupted [tos.py dev bac] run:\n"
            f"  {backup_path}\n"
            f"Not touching it automatically, to avoid overwriting your real "
            f"original config. Please resolve manually:\n"
            f"  1) If this backup IS your original config: copy it back over\n"
            f"     {app_default_config}, then delete the backup file.\n"
            f"  2) If {app_default_config} is already what you want to keep\n"
            f"     (the backup is stale): delete {backup_path}, or re-run\n"
            f"     with --force-clean-backup to skip this check."
        )
        sys.exit(1)
    if os.path.exists(backup_path):
        # --force-clean-backup: user has looked and confirmed it's safe.
        os.remove(backup_path)

    build_result_list = []
    exit_flag = 0
    full_clean_project()

    # sort config list
    config_list.sort()

    skip_set = _build_skip_set(skip)
    config_list, skipped_list = _filter_config_list(config_list, skip_set)
    if skipped_list:
        logger.info("Skipped configs:")
        for config in skipped_list:
            logger.info(f"  - {os.path.basename(config)}")

    if not config_list:
        logger.error("No config to build after filtering.")
        sys.exit(1)

    # Every config in the loop below overwrites app_default_config; back it
    # up first (unless this is a brand-new project that doesn't have one
    # yet) and restore it in `finally` so `bac` never leaves the user's real
    # config permanently replaced by whichever test config ran last.
    have_backup = os.path.exists(app_default_config)
    if have_backup:
        copy_file(app_default_config, backup_path)

    try:
        for idx, config in enumerate(config_list):
            config_file_name = os.path.basename(config)
            copy_file(config, app_default_config)
            logger.info(f"Build [{idx + 1}/{len(config_list)}] {config_file_name}")

            log_file = os.path.join(log_dir, f"{config_file_name}.log") if log_dir else None
            ok = build_project(log_file=log_file, log_file_append=False)

            if ok:
                logger.note(f"Build {config_file_name} success")
                build_result_list.append(f"Build {config_file_name} success")
                if dist:
                    _save_product(dist_abs, config)
            else:
                logger.error(f"Build {config_file_name} failed")
                build_result_list.append(f"Build {config_file_name} failed")
                exit_flag = 1
            full_clean_project(log_file=log_file, log_file_append=True)
    finally:
        if have_backup:
            if copy_file(backup_path, app_default_config):
                os.remove(backup_path)
            else:
                logger.error(
                    f"Restore failed. Your original config is still at "
                    f"{backup_path} -- restore it manually."
                )
        else:
            # app_default.config did not exist before this run (brand-new
            # project); return to that same "absent" state instead of
            # leaving the last test config behind.
            rm_rf(app_default_config)
        # using.config was left stale by whichever config ran last in the
        # loop; force it to be re-derived from the just-restored
        # app_default.config so build state matches on-disk config again.
        init_using_config(force=True)

    # print build result
    BORDER = "================================"
    logger.note(BORDER)
    logger.note("Build Result")
    logger.note(BORDER)
    for result in build_result_list:
        if result.endswith("success"):
            name = result.replace("Build ", "").replace(" success", "")
            logger.note(f"  ✓ {name}")
        else:
            name = result.replace("Build ", "").replace(" failed", "")
            logger.error(f"  ✗ {name}")
    if skipped_list:
        logger.note("Skipped:")
        for config in skipped_list:
            logger.note(f"  - {os.path.basename(config)}")
    logger.note(BORDER)

    sys.exit(exit_flag)


CLIS = {
    "bac": build_all_config_exec,
}


##
# @brief tos.py dev
#
@click.command(cls=set_clis(CLIS),
               help="Development operation.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    pass