#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click
import urllib
import tarfile

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    get_running_env, parse_config_file, do_subprocess,
)


def check_bin_file(using_data) -> bool:
    params = get_global_params()

    bin_path = params["app_bin_path"]
    project_name = using_data["CONFIG_PROJECT_NAME"]
    project_ver = using_data["CONFIG_PROJECT_VERSION"]
    bin_file = os.path.join(
        bin_path, f"{project_name}_QIO_{project_ver}.bin")

    if not os.path.isfile(bin_file):
        return False
    return True


def download_tyutool():
    logger = get_logger()
    params = get_global_params()
    tyutool_cli = params["tyutool_cli"]
    if os.path.exists(tyutool_cli):
        logger.debug("tyutool_cli is exists.")
        return True

    host = "https://images.tuyacn.com/smart/embed/\
package/vscode/data/ide_serial"
    running_env = get_running_env()
    if running_env == "linux":
        package = "tyutool_cli.tar.gz"
    elif running_env == "windows":
        package = "win_tyutool_cli.tar.gz"
    else:
        package = f"{running_env}_tyutool_cli.tar.gz"

    tyutool_root = params["tyutool_root"]
    download_file = os.path.join(tyutool_root, package)
    download_url = f"{host}/{package}"

    logger.info("Downloading tyutool_cli ...")
    urllib.request.urlretrieve(download_url, download_file)
    with tarfile.open(download_file, 'r:gz') as tar:
        tar.extractall(path=tyutool_root)

    if not os.path.exists(download_file):
        return False
    return True


def get_configure_baudrate(using_data, key, baudrate: int) -> int:
    if baudrate != 0:
        return baudrate

    logger = get_logger()
    params = get_global_params()

    platform = using_data["CONFIG_PLATFORM_CHOICE"]
    board = using_data["CONFIG_BOARD_CHOICE"]
    boards_root = params["boards_root"]
    config_file = os.path.join(boards_root, platform,
                               board, "tyutool.cfg")
    if not os.path.exists(config_file):
        return baudrate

    logger.debug(f"Found {config_file}")
    tyutool_data = parse_config_file(config_file)
    baudrate = tyutool_data.get(key, 0)

    return baudrate


def get_flash_cmd(using_data,
                  debug: bool,
                  port: str,
                  baudrate: int) -> str:
    '''
    tyutool_cli --debug write -d xxx -f xxx -p xxx -b xxx
    '''
    params = get_global_params()
    tyutool_cli = params["tyutool_cli"]
    cmd = tyutool_cli

    if debug:
        cmd = f"{cmd} --debug"
    cmd = f"{cmd} write"

    platform = using_data["CONFIG_PLATFORM_CHOICE"]
    chip = using_data.get("CONFIG_CHIP_CHOICE", "")
    device = chip if chip else platform
    cmd = f"{cmd} -d {device}"

    bin_path = params["app_bin_path"]
    project_name = using_data["CONFIG_PROJECT_NAME"]
    project_ver = using_data["CONFIG_PROJECT_VERSION"]
    bin_file = os.path.join(
        bin_path, f"{project_name}_QIO_{project_ver}.bin")
    cmd = f"{cmd} -f {bin_file}"

    if port:
        cmd = f"{cmd} -p {port}"

    if baudrate:
        cmd = f"{cmd} -b {baudrate}"

    return cmd


##
# @brief tos.py flash
#
@click.command(help="Flash the firmware.")
@click.option('-d', '--debug',
              is_flag=True, default=False,
              help="Show flash debug message.")
@click.option('-p', '--port',
              type=str, default="",
              help="Target port.")
@click.option('-b', '--baud',
              type=int, default=0,
              help="Uart baud rate.")
def cli(debug, port, baud):
    logger = get_logger()
    check_proj_dir()

    params = get_global_params()
    using_config = params["using_config"]
    using_data = parse_config_file(using_config)

    if not check_bin_file(using_data):
        logger.error("Not found bin file, please use [tos.py build].")
        sys.exit(1)

    if not download_tyutool():
        logger.error("Download tyutool_cli error.")
        sys.exit(1)

    baudrate = get_configure_baudrate(
        using_data, "CONFIG_FLASH_BAUDRATE", baud)

    cmd = get_flash_cmd(using_data, debug, port, baudrate)
    logger.info(f"Flash command: {cmd}")

    ret = do_subprocess(cmd)

    if ret != 0:
        logger.error("Flash failed.")
        sys.exit(1)

    sys.exit(0)
