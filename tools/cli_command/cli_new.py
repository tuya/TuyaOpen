#!/usr/bin/env python3
# coding=utf-8

import sys
import os
import json
import click
from kconfiglib import Kconfig
from menuconfig import menuconfig

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params,
    do_subprocess
)
from tools.cli_command.util_files import (
    rm_rf, copy_directory, create_directory,
    copy_file, replace_string_in_file
)
from tools.kconfiglib.set_catalog_config import set_catalog_config
from tools.kconfiglib.conf2param import conf2param, param2json


ABILITY_CONFIG = [
    {
        "ability": "CONFIG_ENABLE_ADC",
        "template": "adc",
    },
    {
        "ability": "CONFIG_ENABLE_ASR",
        "template": "asr",
    },
    {
        "ability": "CONFIG_ENABLE_BLUETOOTH",
        "template": "bluetooth",
    },
    {
        "ability": "CONFIG_ENABLE_DAC",
        "template": "dac",
    },
    {
        "ability": "CONFIG_ENABLE_DISPLAY",
        "template": "display",
    },
    {
        "ability": "CONFIG_ENABLE_GPIO",
        "template": "gpio",
    },
    {
        "ability": "CONFIG_ENABLE_HCI",
        "template": "hci",
    },
    {
        "ability": "CONFIG_ENABLE_I2C",
        "template": "i2c",
    },
    {
        "ability": "CONFIG_ENABLE_I2S",
        "template": "i2s",
    },
    {
        "ability": "CONFIG_MCU8080",
        "template": "mcu8080",
    },
    {
        "ability": "CONFIG_ENABLE_MEDIA",
        "template": "media",
    },
    {
        "ability": "CONFIG_ENABLE_PINMUX",
        "template": "pinmux",
    },
    {
        "ability": "CONFIG_ENABLE_PM",
        "template": "pm",
        "del_other": [
            "init/include/tkl_init_pm.h",
            "init/src/tkl_init_pm.c",
        ]
    },
    {
        "ability": "CONFIG_ENABLE_PWM",
        "template": "pwm",
    },
    {
        "ability": "CONFIG_ENABLE_QSPI",
        "template": "qspi",
    },
    {
        "ability": "CONFIG_ENABLE_REGISTER",
        "template": "register",
    },
    {
        "ability": "CONFIG_ENABLE_RGB",
        "template": "rgb",
    },
    {
        "ability": "CONFIG_ENABLE_RTC",
        "template": "rtc",
    },
    {
        "ability": "CONFIG_ENABLE_SPI",
        "template": "spi",
    },
    {
        "ability": "CONFIG_ENABLE_STORAGE",
        "template": "storage",
    },
    {
        "ability": "CONFIG_ENABLE_TIMER",
        "template": "timer",
    },
    {
        "ability": "CONFIG_ENABLE_UART",
        "template": "uart",
    },
    {
        "ability": "CONFIG_ENABLE_VAD",
        "template": "vad",
    },
    {
        "ability": "CONFIG_ENABLE_WAKEUP",
        "template": "wakeup",
    },
    {
        "ability": "CONFIG_ENABLE_WATCHDOG",
        "template": "watchdog",
    },
    {
        "ability": "CONFIG_ENABLE_WIFI",
        "template": "wifi",
        "del_other": [
            "init/include/tkl_init_wifi.h",
            "init/src/tkl_init_wifi.c",
        ]
    },
    {
        "ability": "CONFIG_ENABLE_WIRED",
        "template": "wired",
        "del_other": [
            "init/include/tkl_init_wired.h",
            "init/src/tkl_init_wired.c",
        ]
    },
]


def create_new_platform_path(new_platform_path, new_platform_name):
    '''
    If the old directory exists, save the old directory first
    '''
    logger = get_logger()
    params = get_global_params()
    logger.info("Generating platform root ...")

    # copy to bak
    if os.path.exists(new_platform_path):
        bak_path_base = f"{new_platform_path}_bak"
        for i in range(1, 100):
            bak_path = f"{bak_path_base}_{i}"
            if not os.path.exists(bak_path):
                break
        logger.note(f"Save old platform to: {bak_path}.")
        rm_rf(bak_path)
        copy_directory(new_platform_path, bak_path)

    create_directory(os.path.join(new_platform_path, "tuyaos"))
    porting_root = params["porting_root"]
    source_kconfig = os.path.join(porting_root, ".gitignore")
    target_kconfig = os.path.join(new_platform_path, ".gitignore")
    copy_file(source_kconfig, target_kconfig, force=True)
    source_kconfig = os.path.join(porting_root, "template", "Kconfig")
    target_kconfig = os.path.join(new_platform_path, "Kconfig")
    copy_file(source_kconfig, target_kconfig, force=True)
    replace_string_in_file(target_kconfig,
                           "<your-platform-name>", f"[{new_platform_name}]")
    pass


def gen_default_config(new_platform_path, default_config):
    '''
    Generate file default.config
    '''
    logger = get_logger()
    logger.info("Generating file default.config ...")
    if not os.path.exists(new_platform_path):
        logger.error(f"Platform path not foun: {new_platform_path}.")
        return False
    params = get_global_params()
    src_root = params["src_root"]
    allconfig = os.path.join(new_platform_path, "allconfig")
    set_catalog_config(new_platform_path, src_root, "", allconfig)

    os.environ['KCONFIG_CONFIG'] = default_config
    kconf = Kconfig(filename=allconfig)
    menuconfig(kconf)
    return True


def _copy_base_components(template_root,
                          adapter_include_root):
    logger = get_logger()
    create_directory(adapter_include_root)
    component_list = [
        "utilities", "init", "security",
        "network", "system", "flash"
    ]

    logger.info("Copying base component ...")
    for cmp in component_list:
        logger.debug(f"copy base component: {cmp}.")
        rm_rf(os.path.join(adapter_include_root, cmp))
        copy_directory(
            os.path.join(template_root, cmp),
            os.path.join(adapter_include_root, cmp))
    pass


def _copy_config_components(template_root,
                            adapter_include_root,
                            config_data):
    logger = get_logger()
    logger.info("Processing config component ...")
    for ability in ABILITY_CONFIG:
        name = ability["ability"]
        value = config_data.get(name, False)
        if type(value) is not bool:
            continue
        template = ability["template"]
        temp_path = os.path.join(template_root, template)
        target_path = os.path.join(adapter_include_root, template)
        rm_rf(target_path)

        # enable: copy template
        if value:
            logger.debug(f"process: {name} enable.")
            copy_directory(temp_path, target_path)
            continue

        # disable: del other file
        logger.debug(f"process: {name} disable.")
        del_other = ability.get("del_other", [])
        for f in del_other:
            f_path = os.path.join(adapter_include_root, f)
            rm_rf(f_path)

    # CONFIG_ENABLE_FILE_SYSTEM
    value = config_data.get("CONFIG_ENABLE_FILE_SYSTEM", False)
    if type(value) is bool and value is False:
        f_path = os.path.join(adapter_include_root, "system/tkl_fs.h")
        rm_rf(f_path)

    # CONFIG_OPERATING_SYSTEM
    value = config_data.get("CONFIG_OPERATING_SYSTEM", 0)
    if type(value) is int and value == 100:
        f_path = os.path.join(adapter_include_root,
                              "init/src/tkl_init_network.c")
        rm_rf(f_path)
        f_path = os.path.join(adapter_include_root,
                              "network")
        rm_rf(f_path)
    pass


def update_platform_by_config(new_platform_path, default_config):
    logger = get_logger()
    params = get_global_params()
    if not os.path.exists(default_config):
        logger.error(f"Platform config not foun: {default_config}.")
        return False

    conf_file_list = [default_config]
    params_data = {}
    parmas_json = os.path.join(new_platform_path, "default.json")
    conf2param(conf_file_list, params_data)
    param2json(params_data, parmas_json)
    with open(parmas_json, 'r', encoding='utf-8') as f:
        config_data = json.load(f)

    tuya_root = os.path.join(new_platform_path, "tuyaos")
    adapter_root = os.path.join(tuya_root, "tuyaos_adapter")
    adapter_include_root = os.path.join(adapter_root, "include")
    porting_root = params["porting_root"]
    template_root = os.path.join(porting_root, "adapter")
    _copy_base_components(template_root,
                          adapter_include_root)
    _copy_config_components(template_root,
                            adapter_include_root,
                            config_data)
    return True


def porting_platform(new_platform_path, new_platform_name):
    logger = get_logger()
    params = get_global_params()

    logger.info("Porting platform ...")
    porting_root = params["porting_root"]
    porting_script = os.path.join(porting_root, "kernel_porting.py")

    cmd = f"python {porting_script} {new_platform_path} {new_platform_name}"
    ret = do_subprocess(cmd)
    if 0 != ret:
        return False
    return True


@click.command(help="New platform.")
def new_platform_exec():
    logger = get_logger()
    params = get_global_params()
    logger.note("Input new platform name.")
    new_platform_name = input("input: ")
    platforms_root = params["platforms_root"]
    new_platform_path = os.path.join(platforms_root, new_platform_name)

    # platform is exists
    if os.path.exists(new_platform_path):
        logger.warn(f"[{new_platform_name}] is exists: {new_platform_path}.")
        logger.warn("Do you want to update: y(es) / n(o)")
        update_input = input("input: ").upper()
        if update_input != "Y":
            logger.info("Exit.")
            sys.exit(0)

    default_config = os.path.join(new_platform_path, "default.config")
    create_new_platform_path(new_platform_path, new_platform_name)
    if not gen_default_config(new_platform_path, default_config):
        sys.exit(1)

    if not update_platform_by_config(new_platform_path, default_config):
        sys.exit(1)

    if not porting_platform(new_platform_path, new_platform_name):
        sys.exit(1)

    sys.exit(0)


@click.command(help="New board.")
def new_board_exec():
    pass


@click.command(help="New project.")
@click.option('-f', '--framework',
              type=click.Choice(["base", "arduino"]),
              default="base",
              help="Framework.")
def new_project_exec(framework):
    logger = get_logger()
    params = get_global_params()
    logger.note("Input new project name.")
    new_project_name = input("input: ")
    work_root = params["app_root"]
    new_project_path = os.path.join(work_root, new_project_name)

    # platform is exists
    if os.path.exists(new_project_path):
        logger.error(f"[{new_project_name}] is exists: {new_project_path}.")
        sys.exit(1)

    app_template_root = params["app_template_root"]
    template_path = os.path.join(app_template_root, framework)
    copy_directory(template_path, new_project_path)
    sys.exit(0)


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
