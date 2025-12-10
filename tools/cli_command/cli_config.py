#!/usr/bin/env python3
# coding=utf-8

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


def _defconfig(config, dconfig=".config", kconfig="Kconfig"):
    '''
    minimum config -> .config, with kconfig
    '''
    os.environ['KCONFIG_CONFIG'] = dconfig
    kconf = Kconfig(kconfig, suppress_traceback=True)
    kconf.load_config(config)
    kconf.write_config()
    pass


def _savedefconfig(config, dconfig=".config", kconfig="Kconfig"):
    '''
    .config -> minimum config, with kconfig
    '''
    os.environ['KCONFIG_CONFIG'] = dconfig
    kconf = Kconfig(kconfig, suppress_traceback=True)
    kconf.load_config()
    kconf.write_min_config(config)
    pass

def generate_vscode_config():
    """Generate VS Code C/C++ configuration from Kconfig"""
    logger = get_logger()
    
    # Method 1: Try to find tos.py in the path hierarchy
    def find_main_workspace_root():
        # Start from current directory and go up until we find tos.py
        current = os.path.abspath(os.getcwd())
        while current != '/':
            if os.path.exists(os.path.join(current, 'tos.py')):
                return current
            current = os.path.dirname(current)
        # Fallback: use the original approach
        current_script_dir = os.path.dirname(os.path.abspath(__file__))
        return os.path.abspath(os.path.join(current_script_dir, "../../.."))
    
    main_workspace_root = find_main_workspace_root()
    logger.debug(f"Main workspace root: {main_workspace_root}")
    
    params = get_global_params()
    using_config = params["using_config"]
    catalog_kconfig = params["catalog_kconfig"]
    
    if not os.path.exists(using_config):
        logger.warning(f"Config file {using_config} not found. Run menuconfig first.")
        return False
    
    if not os.path.exists(catalog_kconfig):
        logger.warning(f"Kconfig file {catalog_kconfig} not found.")
        return False
    
    try:
        # Parse Kconfig and load current configuration
        os.environ['KCONFIG_CONFIG'] = using_config
        kconf = Kconfig(catalog_kconfig, suppress_traceback=True)
        kconf.load_config()
        
        # Extract all configuration defines - FIXED LOGIC
        config_defines = []
        for sym in kconf.unique_defined_syms:
            if sym.name:
                str_val = sym.str_value
                # Only define symbols that are enabled (y) or have non-zero/non-empty values
                if str_val == "y":
                    config_defines.append(sym.name)  # Just the name for boolean true
                elif str_val.isdigit() and int(str_val) != 0:
                    config_defines.append(f"{sym.name}={str_val}")
                elif str_val.startswith("0x") or str_val.startswith("0X"):
                    hex_val = int(str_val, 16)
                    if hex_val != 0:
                        config_defines.append(f"{sym.name}={hex_val}")
                elif str_val and str_val != '""' and str_val != "n":  # Non-empty strings
                    config_defines.append(f'{sym.name}="{str_val}"')
        
        # Get absolute paths for include directories
        app_root = os.path.abspath(params["app_root"])
        board_path = os.path.abspath(params["boards_root"])
        src_path = os.path.abspath(params["src_root"])
        
        # Generate VS Code configuration
        vscode_config = {
            "configurations": [
                {
                    "name": "KConfig-Aware",
                    "includePath": [
                        "${workspaceFolder}/**",
                        app_root + "/**",
                        board_path + "/**", 
                        src_path + "/**"
                    ],
                    "defines": config_defines,  # Use the corrected list
                    "compilerPath": "/usr/bin/gcc",
                    "cStandard": "c99",
                    "cppStandard": "c++17",
                    "intelliSenseMode": "gcc-x64",
                    "browse": {
                        "path": [
                            "${workspaceFolder}/**",
                            app_root + "/**",
                            board_path + "/**",
                            src_path + "/**"
                        ],
                        "limitSymbolsToIncludedHeaders": True,
                        "databaseFilename": "${workspaceFolder}/.vscode/browse.vc.db"
                    },
                    "configurationProvider": "ms-vscode.cpptools"
                }
            ],
            "version": 4
        }
        
        # Write to .vscode/c_cpp_properties.json in MAIN WORKSPACE ROOT
        vscode_dir = os.path.join(main_workspace_root, ".vscode")
        if not os.path.exists(vscode_dir):
            os.makedirs(vscode_dir)
        
        config_file = os.path.join(vscode_dir, "c_cpp_properties.json")
        with open(config_file, 'w') as f:
            json.dump(vscode_config, f, indent=4)
        
        logger.note(f"VS Code configuration generated: {config_file}")
        logger.note(f"Loaded {len(config_defines)} configuration symbols")
        
        # Debug: Show some important defines
        logger.debug("Important board selection defines:")
        for define in config_defines:
            if "BOARD_CHOICE" in define or "PLATFORM" in define:
                logger.debug(f"  {define}")
        
        return True
        
    except Exception as e:
        logger.error(f"Failed to generate VS Code configuration: {e}")
        import traceback
        logger.error(traceback.format_exc())
        return False

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
    app_root = params["app_root"]  # apps/tuya_cloud/my
    catalog_kconfig = params["catalog_kconfig"]
    set_catalog_config(board_path, src_path, app_root, catalog_kconfig)

    app_default_config = params["app_default_config"]  # apps/tuya_cloud/my/app_default.config
    if not os.path.exists(app_default_config):
        tools_root = params["tools_root"]
        template = os.path.join(tools_root, "kconfiglib", "app_default.config")
        copy_file(template, app_default_config)
        pass

    using_config = params["using_config"]  # apps/tuya_cloud/my/.build/cache/using.config
    if force or not os.path.exists(using_config):
        _defconfig(app_default_config, using_config, catalog_kconfig)
    
    # Generate VS Code configuration after config change
    generate_vscode_config()
    pass


def get_board_config_dir(board_path):
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
    logger = get_logger()
    params = get_global_params()
    full_clean_project()

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

    # choice config file
    config_list.sort()
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
    
    # Generate VS Code configuration after menuconfig
    generate_vscode_config()
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
    logger.note(f"Success save: {saveconfig}")
    sys.exit(0)


@click.command(help="Update VS Code configuration.")
def config_vscode_exec():
    '''
    Update VS Code C/C++ configuration with current Kconfig defines
    '''
    logger = get_logger()
    if generate_vscode_config():
        logger.note("VS Code configuration updated successfully")
    else:
        logger.error("Failed to update VS Code configuration")
    sys.exit(0)


CLIS = {
    "choice": config_choice_exec,
    "menu": config_menu_exec,
    "save": config_save_exec,
    "vscode": config_vscode_exec  # Add new command
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