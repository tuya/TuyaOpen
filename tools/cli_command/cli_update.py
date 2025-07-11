#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click

from tools.cli_command.util import (
    get_logger, get_global_params, parse_yaml
)
from tools.cli_command.util_git import git_checkout
from tools.cli_command.util_files import rm_rf
from tools.cli_command.cli_flash import download_tyutool


def update_tyutool(force):
    logger = get_logger()
    params = get_global_params()
    tyutool_root = params["tyutool_root"]

    if not os.path.exists(tyutool_root) and not force:
        logger.debug("Tyutool does not require an upgrade.")
        return True

    rm_rf(tyutool_root)
    return download_tyutool()


def update_platform():
    logger = get_logger()
    params = get_global_params()
    platforms_root = params["platforms_root"]
    platforms_yaml = params["platforms_yaml"]
    platforms_data = parse_yaml(platforms_yaml)
    platform_list = platforms_data.get("platforms", [])

    success = True
    for p in platform_list:
        name = p.get("name", "")
        p_root = os.path.join(platforms_root, name)
        if not os.path.exists(p_root):
            continue

        commit = p.get("commit", "")
        if not commit:
            logger.warning(f"Not found commit for platform [{name}].")
            continue

        logger.info(f"Updating platform [{name}] ...")
        if not git_checkout(p_root, commit):
            logger.error(f"Update platform [{name}] failed.")
            success = False
            continue

        logger.note(f"Update platform [{name}] success.")

    if not success:
        logger.error("Update platform has some mistakes.")
        return False

    return True


##
# @brief tos.py update
#
@click.command(help="Update TuyaOpen dependencies.")
@click.option('-t', '--tyutool',
              is_flag=True, default=False,
              help="Update tyutool only.")
def cli(tyutool):
    exit_flag = 0 if update_tyutool(tyutool) else 1
    if tyutool:
        sys.exit(exit_flag)

    if not update_platform():
        sys.exit(1)

    sys.exit(0)