#!/usr/bin/env python3
# coding=utf-8

import os
import click
import subprocess
import re
from typing import List

from tools.cli_command.util import (
    get_logger, get_country_code, get_global_params,
    env_write,
)
from tools.cli_command.util_files import copy_directory
from tools.cli_command.util_git import set_repo_mirro, download_submoudules


def copy_pre_commit():
    params = get_global_params()
    open_root = params["open_root"]
    tools_root = params["tools_root"]
    source = os.path.join(tools_root, "hooks")
    target = os.path.join(open_root, ".git", "hooks")
    copy_directory(source, target)
    pass


def _find_version_string(version: str) -> str:
    pattern = r"\d+\.\d+(\.\d+)?"
    match = re.search(pattern, version)
    if not match:
        return ""
    return match.group()


def _parse_version_string(version: str) -> List[int]:
    num_list = version.split(".")
    try:
        return [int(num) for num in num_list]
    except ValueError:
        return None
    pass


def _compare_version(actual: str, required: str) -> bool:
    actual_ver = _parse_version_string(actual)
    required_ver = _parse_version_string(required)
    if (actual_ver is None) or (required_ver is None):
        return False

    for a, r in zip(actual_ver, required_ver):
        if a > r:
            return True
        elif a < r:
            return False
    return True


def check_command_version(tool_name, min_version, ver_cmd="--version"):
    logger = get_logger()
    cmd = [tool_name, ver_cmd]
    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        cmd_out = result.stdout + result.stderr
    except (subprocess.CalledProcessError, FileNotFoundError):
        logger.error(f"[{tool_name}] not found, please install.")
        return False

    tool_ver = _find_version_string(cmd_out)
    if tool_ver == "" or _compare_version(tool_ver, min_version) is False:
        logger.warning(
            f"[{tool_name}] ({tool_ver} < {min_version}) need update.")
        return False
    logger.info(f"[{tool_name}] ({tool_ver} >= {min_version}) is ok.")
    return True


def check_base_tools():
    command_list = [
        ("git", "--version", "2.0.0"),
        ("cmake", "--version", "3.28.0"),
        ("ninja", "--version", "1.6.0"),
    ]

    for command in command_list:
        check_command_version(command[0], command[2], command[1])

    copy_pre_commit()
    pass


def update_submodules():
    params = get_global_params()
    open_root = params["open_root"]
    code = get_country_code()
    if code == "China":
        set_repo_mirro(unset=False)

    ret = download_submoudules(open_root)

    if code == "China":
        set_repo_mirro(unset=True)
    return ret


##
# @brief tos.py check
#
@click.command(help="Check the dependent tools.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    check_base_tools()
    ret = update_submodules()
    env_write("update_submodules", ret)
    pass
