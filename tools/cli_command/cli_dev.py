#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params,
    parse_config_file
)
from tools.cli_command.cli_config import get_board_config_dir
from tools.cli_command.util_files import (
    get_files_from_path, copy_file, rm_rf
)
from tools.cli_command.cli_build import build_project
from tools.cli_command.cli_clean import full_clean_project


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
def build_all_config_exec(dist, log_dir):
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
    build_result_list = []
    exit_flag = 0
    full_clean_project()

    # sort config list
    config_list.sort()

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