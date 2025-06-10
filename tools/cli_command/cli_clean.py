#!/usr/bin/env python3
# coding=utf-8

import os
import click

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    do_subprocess,
)
from tools.cli_command.util_files import rm_rf


def clean_project():
    logger = get_logger()
    params = get_global_params()
    build_path = params["app_build_path"]
    build_file = os.path.join(build_path, "build.ninja")
    if not os.path.isfile(build_file):
        logger.debug("No need clean.")
        return True

    cmd = "ninja clean_all"
    ninja_cmd = f"cd {build_path} && {cmd}"

    ret = do_subprocess(ninja_cmd)
    if 0 != ret:
        logger.error("Clean error.")
        return False

    logger.info("Clean success.")
    return True


def full_clean_project():
    logger = get_logger()
    clean_project()
    params = get_global_params()
    build_path = params["app_build_path"]
    rm_rf(build_path)
    logger.info("Fullclean success.")
    pass


##
# @brief tos.py clean
#
@click.command(help="Clean the project.")
@click.option('-f', '--full',
              is_flag=True, default=False,
              help="Full clean.")
def cli(full):
    check_proj_dir()
    if full:
        full_clean_project()
    else:
        clean_project()
    pass
