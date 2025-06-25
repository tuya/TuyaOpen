#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params
)
from tools.cli_command.cli_config import get_board_config_dir
from tools.cli_command.util_files import (
    get_files_from_path, copy_file, rm_rf,
    copy_directory
)
from tools.cli_command.cli_build import build_project
from tools.cli_command.cli_clean import full_clean_project


def _save_product(dist, config_file):
    logger = get_logger()
    params = get_global_params()
    app_root = params["app_root"]
    app_root_basename = os.path.basename(app_root)
    config_basename = os.path.basename(config_file)
    app_dist = os.path.join(dist, app_root_basename)
    os.makedirs(app_dist, exist_ok=True)
    bin_dist = os.path.join(app_dist, config_basename)
    if os.path.exists(bin_dist):
        rm_rf(bin_dist)
    app_bin_path = params["app_bin_path"]
    logger.info(f"Save product to {bin_dist}.")
    copy_directory(app_bin_path, bin_dist)
    pass


@click.command(help="Build all config.")
@click.option('-d', '--dist',
              type=str, default="",
              help="Save product path.")
def build_all_config_exec(dist):
    logger = get_logger()
    params = get_global_params()
    dist_abs = os.path.abspath(dist)

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
    for config in config_list:
        logger.info(f"Build with: {config}")
        copy_file(config, app_default_config)
        if build_project():
            build_result_list.append(f"{config} build success")
            if dist:
                _save_product(dist_abs, config)
        else:
            build_result_list.append(f"{config} build failed")
            exit_flag = 1
        full_clean_project()

    # print build result
    for result in build_result_list:
        if result.endswith("success"):
            logger.note(result)
        else:
            logger.error(result)

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
