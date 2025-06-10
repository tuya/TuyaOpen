#!/usr/bin/env python3
# coding=utf-8

import os
import sys
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


def _defconfig(config, dconfig=".config", kconfig="Kconfig"):
    '''
    minimum config -> .config, with kconfig
    '''
    os.environ['KCONFIG_CONFIG'] = dconfig
    kconf = Kconfig(kconfig, suppress_traceback=True)
    kconf.load_config(config)
    kconf.write_config()
    # print(kconf.load_config(config))
    # print(kconf.write_config())
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
    logger.info("Intialing using.config ...")
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


def _get_board_config_dir(board_path):
    ret = []
    for entry in os.scandir(board_path):
        if entry.is_dir():
            # TuyaOpen/boards/xxx/config
            ret.append(os.path.join(entry, "config"))

    return ret


@click.command(help="Choice config file.")
@click.option('-d', '--default',
              is_flag=True, default=False,
              help="Only display board default config.")
def config_choice_exec(default):
    '''
    Choice config file
    from app config or board default config
    '''
    # get congi files
    logger = get_logger()
    params = get_global_params()
    full_clean_project()
    app_configs_path = params["app_configs_path"]
    if (not default) and os.path.exists(app_configs_path):
        logger.debug("Choice from app config")
        config_list = get_files_from_path(".config", app_configs_path, 1)
    else:
        logger.debug("Choice from board config")
        board_path = params["boards_root"]
        config_dir = _get_board_config_dir(board_path)
        config_list = get_files_from_path(".config", config_dir, 0)

    # choice config file
    show_list = [os.path.basename(conf) for conf in config_list]
    _, index = list_menu("Choice config file", show_list)
    choice_config = config_list[index]

    # copy config file
    app_default_config = params["app_default_config"]
    copy_file(choice_config, app_default_config)
    init_using_config(force=True)
    logger.info(f"Choice config: {choice_config}")
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


@click.command(help="Save minimal config.")
def config_save_exec():
    '''
    1. Copy: app_default.config -> $APP_TOOT/config/xxx.config
    '''
    logger = get_logger()
    params = get_global_params()
    app_default_config = params["app_default_config"]

    if not os.path.exists(app_default_config):
        logger.error("Please run [tos.py menuconfig] first.")
        sys.exit(1)

    saveconfig_name = input("Input save config name: ")
    if not saveconfig_name.endswith(".config"):
        saveconfig_name += ".config"

    app_configs_path = params["app_configs_path"]
    saveconfig = os.path.join(app_configs_path, saveconfig_name)
    copy_file(app_default_config, saveconfig)
    logger.info(f"Success save [{saveconfig}].")
    sys.exit(0)


CLIS = {
    "choice": config_choice_exec,
    "menu": config_menu_exec,
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
