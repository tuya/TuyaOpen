#!/usr/bin/env python3
# coding=utf-8

import os
import re
import sys
import click
import subprocess

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    parse_config_file,
)
from tools.cli_command.util_tyutool import ensure_tyutool


_PROGRESS_RE = re.compile(r'^\[progress\]\s*(\d+)%\s*$')
_PHASE_RE = re.compile(r'^\[phase\]\s*(.+)$')


def _render_bar(pct: int, width: int = 30) -> str:
    filled = int(width * pct / 100)
    bar = '#' * filled + '-' * (width - filled)
    return f"\r[{bar}] {pct:3d}%"


def do_flash_subprocess(cmd: str) -> int:
    logger = get_logger()
    if not cmd:
        logger.warning("Subprocess cmd is empty.")
        return 0

    logger.info(f">>> subprocess >>>\n{cmd}")

    last_pct = -1
    current_phase = ""
    on_progress_line = False

    try:
        proc = subprocess.Popen(
            cmd, shell=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1,
        )
        for line in proc.stdout:
            line = line.rstrip('\n').rstrip('\r')

            m = _PROGRESS_RE.match(line)
            if m:
                pct = int(m.group(1))
                if pct != last_pct:
                    last_pct = pct
                    sys.stdout.write(_render_bar(pct))
                    sys.stdout.flush()
                    on_progress_line = True
                continue

            mp = _PHASE_RE.match(line)
            if mp:
                if on_progress_line:
                    sys.stdout.write('\n')
                    on_progress_line = False
                current_phase = mp.group(1)
                print(f"[phase] {current_phase}")
                last_pct = -1
                continue

            if on_progress_line:
                sys.stdout.write('\n')
                on_progress_line = False
            print(line)

        if on_progress_line:
            sys.stdout.write('\n')

        proc.wait()
        return proc.returncode
    except Exception as e:
        logger.error(f"Flash subprocess error: {e}")
        return 1


def check_bin_file(using_data) -> bool:
    logger = get_logger()
    params = get_global_params()

    bin_path = params["app_bin_path"]
    project_name = using_data["CONFIG_PROJECT_NAME"]
    project_ver = using_data["CONFIG_PROJECT_VERSION"]
    bin_file = os.path.join(
        bin_path, f"{project_name}_QIO_{project_ver}.bin")

    if not os.path.isfile(bin_file):
        logger.error("Not found bin file, please use [tos.py build].")
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


_DEVICE_ALIAS = {
    "T5AI": "t5",
    "BK7231X": "bk7231n",
}


def _normalize_device(name: str) -> str:
    return _DEVICE_ALIAS.get(name.upper(), name.lower())


def get_flash_cmd(using_data,
                  debug: bool,
                  port: str,
                  baudrate: int) -> str:
    '''
    tyutool_cli --debug write -d xxx -f xxx -p xxx -b xxx
    '''
    params = get_global_params()
    tyutool_bin = params["tyutool_bin"]
    cmd = f'"{tyutool_bin}"'

    if debug:
        cmd = f"{cmd} --debug"
    cmd = f"{cmd} write"

    platform = using_data["CONFIG_PLATFORM_CHOICE"]
    chip = using_data.get("CONFIG_CHIP_CHOICE", "")
    device = _normalize_device(chip if chip else platform)
    cmd = f"{cmd} -d {device}"

    bin_path = params["app_bin_path"]
    project_name = using_data["CONFIG_PROJECT_NAME"]
    project_ver = using_data["CONFIG_PROJECT_VERSION"]
    bin_file = os.path.join(
        bin_path, f"{project_name}_QIO_{project_ver}.bin")
    cmd = f'{cmd} -f "{bin_file}"'

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
        sys.exit(1)

    if not ensure_tyutool():
        sys.exit(1)

    baudrate = get_configure_baudrate(
        using_data, "CONFIG_FLASH_BAUDRATE", baud)

    cmd = get_flash_cmd(using_data, debug, port, baudrate)
    logger.info(f"Flash command: {cmd}")

    ret = do_flash_subprocess(cmd)

    if ret != 0:
        logger.error("Flash failed.")
        sys.exit(1)

    sys.exit(0)
