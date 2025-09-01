#!/usr/bin/env python3
# coding=utf-8

import sys
import os
import json
import re
import click
from kconfiglib import Kconfig
from menuconfig import menuconfig

from tools.cli_command.util import (
    set_clis, get_logger, get_global_params,
    do_subprocess, list_menu
)
from tools.cli_command.util_files import (
    rm_rf, copy_directory, create_directory,
    copy_file, replace_string_in_file, get_subdir_from_path,
    check_text_in_file
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

    if not os.path.exists(default_config):
        logger.error(f"Platform config not foun: {default_config}.")
        return False

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
                            config_data,
                            tuya_root):
    '''
    Copy the template of the configurable component
    and generate the [TKL_Kconfig] file
    '''
    logger = get_logger()
    tkl_kconfig_content = ""
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
            tkl_kconfig_content += f"config {name[7:]}\n\tdefault y\n\n"
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
    value = config_data.get("CONFIG_OPERATING_SYSTEM", 3)
    if type(value) is int and value == 100:
        f_path = os.path.join(adapter_include_root,
                              "init/src/tkl_init_network.c")
        rm_rf(f_path)
        f_path = os.path.join(adapter_include_root,
                              "network")
        rm_rf(f_path)

    # generate TKL_Kconfig
    tkl_kconfig = os.path.join(tuya_root, "TKL_Kconfig")
    with open(tkl_kconfig, 'w', encoding='utf-8') as f:
        f.write(tkl_kconfig_content)
    pass


def update_platform_by_config(new_platform_path, config_data):
    params = get_global_params()

    tuya_root = os.path.join(new_platform_path, "tuyaos")
    adapter_root = os.path.join(tuya_root, "tuyaos_adapter")
    adapter_include_root = os.path.join(adapter_root, "include")
    porting_root = params["porting_root"]
    template_root = os.path.join(porting_root, "adapter")
    _copy_base_components(template_root,
                          adapter_include_root)
    _copy_config_components(template_root,
                            adapter_include_root,
                            config_data,
                            tuya_root)
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


def create_new_board_path(new_board_path,
                          new_platform_path,
                          new_platform_name,
                          config_data):
    '''
    If the old directory exists, save the old directory first
    '''
    logger = get_logger()
    logger.info("Generating platform root ...")

    # copy to bak
    if os.path.exists(new_board_path):
        bak_path_base = f"{new_board_path}_bak"
        for i in range(1, 100):
            bak_path = f"{bak_path_base}_{i}"
            if not os.path.exists(bak_path):
                break
        logger.note(f"Save old board to: {bak_path}.")
        rm_rf(bak_path)
        copy_directory(new_board_path, bak_path)

    create_directory(new_board_path)

    operating_system = config_data.get("CONFIG_OPERATING_SYSTEM", 3)
    kconfig = os.path.join(new_board_path, "Kconfig")
    kconfig_content = f'''# Ktuyaconf
config PLATFORM_CHOICE
    string
    default "{new_platform_name}"

config OPERATING_SYSTEM
    int
    default {operating_system}
    ---help---
        100     /* LINUX */
        98      /* RTOS */
        3       /* Non-OS */

rsource "./TKL_Kconfig"
rsource "./OS_SERVICE_Kconfig"
'''
    with open(kconfig, 'w', encoding='utf-8') as f:
        f.write(kconfig_content)

    os_kconfig = os.path.join(new_board_path, "OS_SERVICE_Kconfig")
    os_kconfig_content = '''config MBEDTLS_CONFIG_FILE
    string
    default "tuya_tls_config.h"
'''
    with open(os_kconfig, 'w', encoding='utf-8') as f:
        f.write(os_kconfig_content)

    tkl_kconfig = os.path.join(new_board_path, "TKL_Kconfig")
    tkl_kconfig_template = os.path.join(new_platform_path,
                                        "tuyaos", "TKL_Kconfig")
    copy_file(tkl_kconfig_template, tkl_kconfig, force=True)
    pass


def modify_board_kconfig(board_kconfig, new_platform_name):
    platform_name = new_platform_name.upper()
    platform_name_ = platform_name.replace('-', '_')
    board_enable_string = f"BOARD_ENABLE_{platform_name}"

    with open(board_kconfig, 'r', encoding='utf-8') as f:
        content = f.read()
        pattern = r'\b' + board_enable_string + r'\b'
        match = re.search(pattern, content)
        if match is not None:
            return

    board_choice_string = f"BOARD_CHOICE_{platform_name_}"
    kconfig_line = "# <new-board-kconfig: \
This line cannot be deleted or modified>"
    kconfig_content = f'''
    config {board_choice_string}
        bool "{new_platform_name}"
    if ({board_choice_string})
    rsource "./{new_platform_name}/Kconfig"
    endif

{kconfig_line}'''
    replace_string_in_file(board_kconfig, kconfig_line, kconfig_content)
    pass


def initialization_board(boards_root,
                         new_platform_path,
                         new_platform_name,
                         config_data):
    new_board_path = os.path.join(boards_root, new_platform_name)
    create_new_board_path(new_board_path,
                          new_platform_path,
                          new_platform_name,
                          config_data)

    board_kconfig = os.path.join(boards_root, "Kconfig")
    modify_board_kconfig(board_kconfig, new_platform_name)

    return True


@click.command(help="Generating chip platform \
migration support package.")
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

    conf_file_list = [default_config]
    params_data = {}
    parmas_json = os.path.join(new_platform_path, "default.json")
    conf2param(conf_file_list, params_data)
    param2json(params_data, parmas_json)
    with open(parmas_json, 'r', encoding='utf-8') as f:
        config_data = json.load(f)

    if not update_platform_by_config(new_platform_path, config_data):
        sys.exit(1)

    if not porting_platform(new_platform_path, new_platform_name):
        sys.exit(1)

    boards_root = params["boards_root"]
    if not initialization_board(boards_root,
                                new_platform_path,
                                new_platform_name,
                                config_data):
        sys.exit(1)
    sys.exit(0)


@click.command(help="New hello_world template \
app project + CMake configs for rapid setup.")
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


def _add_board_kconfig(board_kconfig, new_board_name, add_line):
    new_board_name_ = new_board_name.replace('-', '_')
    add_board_context = f'''
    config BOARD_CHOICE_{new_board_name_}
        bool "{new_board_name}"
        if (BOARD_CHOICE_{new_board_name_})
            rsource "./{new_board_name}/Kconfig"
        endif

{add_line}'''

    replace_string_in_file(board_kconfig, add_line, add_board_context)
    pass


def _new_board_kconfig(board_kconfig, new_board_name, add_line):
    new_board_name_ = new_board_name.replace('-', '_')
    new_board_context = f'''
choice
    prompt "Choice a board"

    config BOARD_CHOICE_{new_board_name_}
        bool "{new_board_name}"
        if (BOARD_CHOICE_{new_board_name_})
            rsource "./{new_board_name}/Kconfig"
        endif

{add_line}

endchoice
'''

    with open(board_kconfig, 'a', encoding='utf-8') as f:
        f.write(new_board_context)
    pass


@click.command(help="Creating board-specific BSP \
(Board Support Package) with hardware drivers.")
def new_board_exec():
    logger = get_logger()
    params = get_global_params()

    boards_root = params["boards_root"]
    boards = get_subdir_from_path(boards_root)
    boards.sort()
    logger.debug(f"sub boards: {boards}")

    platform, _ = list_menu("Choice platform", boards)
    logger.note("Input new board name.")
    new_board_name = input("input: ")

    board_path = os.path.join(boards_root, platform)
    new_board_path = os.path.join(board_path, new_board_name)
    if os.path.exists(new_board_path):
        logger.error(f"Directory already exists: {new_board_path}.")
        sys.exit(1)

    board_kconfig = os.path.join(board_path, "Kconfig")
    add_line = "# <new-board-add: This line cannot be deleted or modified>"
    if check_text_in_file(board_kconfig, add_line):
        _add_board_kconfig(board_kconfig, new_board_name, add_line)
    else:
        _new_board_kconfig(board_kconfig, new_board_name, add_line)

    board_template_root = params["board_template_root"]
    copy_directory(board_template_root, new_board_path)

    new_board_kconfig = os.path.join(new_board_path, "Kconfig")
    # Esp32 also has a layer of chip concept that needs special processing
    chip_name = "esp32s3" if platform == "ESP32" else platform
    replace_string_in_file(new_board_kconfig, "<platform-name>", chip_name)
    replace_string_in_file(new_board_kconfig, "<board-name>", new_board_name)

    sys.exit(0)


CLIS = {
    "platform": new_platform_exec,
    "project": new_project_exec,
    "board": new_board_exec,
}


##
# @brief tos.py new
#
@click.command(cls=set_clis(CLIS),
               help="Create a new module.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    pass
