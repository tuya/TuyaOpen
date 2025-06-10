#!/usr/bin/env python3
# coding=utf-8

import sys
import click

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    parse_config_file, do_subprocess,
)
from tools.cli_command.cli_flash import (
    download_tyutool, get_configure_baudrate
)


def get_monitor_cmd(using_data,
                    port: str,
                    baudrate: int) -> str:
    '''
    tyutool_cli monitor -d xxx -p xxx -b xxx
    '''
    params = get_global_params()
    tyutool_cli = params["tyutool_cli"]
    cmd = tyutool_cli

    cmd = f"{cmd} monitor"

    platform = using_data["CONFIG_PLATFORM_CHOICE"]
    chip = using_data.get("CONFIG_CHIP_CHOICE", "")
    device = chip if chip else platform
    cmd = f"{cmd} -d {device}"

    if port:
        cmd = f"{cmd} -p {port}"

    if baudrate:
        cmd = f"{cmd} -b {baudrate}"

    return cmd


##
# @brief tos.py monitor
#
@click.command(help="Display the device log.")
@click.option('-p', '--port',
              type=str, default="",
              help="Target port.")
@click.option('-b', '--baud',
              type=int, default=0,
              help="Uart baud rate.")
def cli(port, baud):
    logger = get_logger()
    check_proj_dir()

    params = get_global_params()
    using_config = params["using_config"]
    using_data = parse_config_file(using_config)

    if not download_tyutool():
        logger.error("Download tyutool_cli error.")
        sys.exit(1)

    baudrate = get_configure_baudrate(
        using_data, "CONFIG_MONITOR_BAUDRATE", baud)

    cmd = get_monitor_cmd(using_data, port, baudrate)
    logger.info(f"Monitor command: {cmd}")

    do_subprocess(cmd)

    sys.exit(0)
