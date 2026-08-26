#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click
import subprocess
import re
import shutil
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


def check_command_version(tool_name, min_version, ver_cmd="--version",
                          label=None):
    logger = get_logger()
    disp = label or tool_name
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
        logger.error(f"[{disp}] not found, please install.")
        return False

    tool_ver = _find_version_string(cmd_out)
    if tool_ver == "" or _compare_version(tool_ver, min_version) is False:
        logger.warning(
            f"[{disp}] ({tool_ver} < {min_version}) need update.")
        return False
    logger.note(f"[{disp}] ({tool_ver} >= {min_version}) is ok.")
    return True


def _ensure_make_for_check():
    if sys.platform != "win32":
        return
    if shutil.which("make"):
        return
    from tools.cli_command.cli_prepare import (
        ensure_windows_make, prepend_windows_make_to_path,
    )
    logger = get_logger()
    if ensure_windows_make():
        prepend_windows_make_to_path()
        return
    logger.note("Run . .\\export.ps1 (or export.sh) to prepare host tools.")


def check_base_tools():
    logger = get_logger()

    # git: system dependency.
    check_command_version("git", "2.0.0", "--version")

    # cmake / ninja: TuyaOpen-managed (installed into the venv). Resolve by
    # absolute path and repair if the venv copy is missing or broken, so we
    # never mis-report "please install" for a bundled-but-damaged tool, and
    # never silently fall back to a system cmake of an unpinned version.
    from tools.cli_command.cli_prepare import ensure_venv_tool
    for pkg, binary, min_ver in (("cmake", "cmake", "3.28.0"),
                                 ("ninja", "ninja", "1.6.0")):
        ok, exe = ensure_venv_tool(pkg, binary)
        if ok and exe:
            check_command_version(exe, min_ver, "--version", label=binary)
        else:
            logger.error(
                f"[{binary}] TuyaOpen-managed dependency is missing or broken. "
                f"Re-initialize the environment (. .\\export.ps1 / . ./export.sh) "
                f"or run: uv sync --reinstall-package {pkg}. "
                f"If it was removed by antivirus, add .venv to the allowlist."
            )

    # make: on Windows this is a downloaded .tools/make; keep the self-heal.
    _ensure_make_for_check()
    check_command_version("make", "3.0.0", "--version")

    copy_pre_commit()
    pass


def update_submodules():
    params = get_global_params()
    open_root = params["open_root"]
    code = get_country_code()
    if code == "China":
        set_repo_mirro(unset=False)

    # try/finally so an exception (e.g. a corrupted submodule repo) or a
    # Ctrl-C during the download can't skip the unset below and leave the
    # global github->gitee mirror rewrite stuck in ~/.gitconfig, affecting
    # every other git operation on this machine. Mirrors the same pattern
    # already used around download_platform() in cli_build.py.
    try:
        ret = download_submoudules(open_root)
    finally:
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
