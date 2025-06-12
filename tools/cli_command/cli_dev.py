#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params
)
from tools.cli_command.cli_config import get_board_config_dir
from tools.cli_command.util_files import get_files_from_path, copy_file
from tools.cli_command.cli_build import build_project
from tools.cli_command.cli_clean import full_clean_project

@click.command(help="Build all config.")
def build_all_config_exec():
    logger = get_logger()
    params = get_global_params()

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
            build_result_list.append(f"{config} build success.")
        else:
            build_result_list.append(f"{config} build failed.")
            exit_flag = 1
        full_clean_project()

    # print build result
    for result in build_result_list:
        logger.info(result)

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
