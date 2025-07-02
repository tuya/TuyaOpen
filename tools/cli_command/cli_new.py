#!/usr/bin/env python3
# coding=utf-8

import sys
import os
import click
from kconfiglib import Kconfig
from menuconfig import menuconfig

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params
)
from tools.cli_command.util_files import (
    rm_rf, move_directory, create_directory, copy_file
)
from tools.kconfiglib.set_catalog_config import set_catalog_config


def create_new_platform_path(new_platform_path):
    logger = get_logger()
    params = get_global_params()
    if os.path.exists(new_platform_path):
        bak_path = f"{new_platform_path}_bak"
        logger.note(f"Save old platform to: {bak_path}.")
        move_directory(new_platform_path, bak_path, force=True)
    rm_rf(new_platform_path)
    create_directory(os.path.join(new_platform_path, "tuyaos"))
    porting_root = params["porting_root"]
    source_kconfig = os.path.join(porting_root, "template", "Kconfig")
    target_kconfig = os.path.join(new_platform_path, "Kconfig")
    copy_file(source_kconfig, target_kconfig)
    pass


def gen_default_config(new_platform_path):
    '''
    Generate File default.config
    '''
    params = get_global_params()
    src_root = params["src_root"]
    allconfig = os.path.join(new_platform_path, "allconfig")
    set_catalog_config(new_platform_path, src_root, "", allconfig)

    default_config = os.path.join(new_platform_path, "default.config")
    os.environ['KCONFIG_CONFIG'] = default_config
    kconf = Kconfig(filename=allconfig)
    menuconfig(kconf)
    pass


@click.command(help="New platform.")
def new_platform_exec():
    logger = get_logger()
    params = get_global_params()
    logger.note("Input new platform name.")
    new_platform_name = input("input: ")
    platforms_root = params["platforms_root"]
    new_platform_path = os.path.join(platforms_root, new_platform_name)

    if os.path.exists(new_platform_path):
        logger.warn(f"[{new_platform_name}] is exists: {new_platform_path}")
        logger.warn("Do you want to update: y(es) / n(o)")
        update_input = input("input: ").upper()
        if update_input != "Y":
            logger.info("Exit.")
            sys.exit(0)

    create_new_platform_path(new_platform_path)
    gen_default_config(new_platform_path)
    pass


@click.command(help="New board.")
def new_board_exec():
    pass


@click.command(help="New project.")
def new_project_exec():
    pass


CLIS = {
    "platform": new_platform_exec,
    "board": new_board_exec,
    "project": new_project_exec
}


##
# @brief tos.py new
#
@click.command(cls=set_clis(CLIS),
               help="Create a new module.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    pass
