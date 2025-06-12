#!/usr/bin/env python3
# coding=utf-8

import os
import click

from tools.cli_command.util import (
    get_logger, get_global_params, parse_yaml
)
from tools.cli_command.util_git import git_checkout


def update_platform():
    logger = get_logger()
    params = get_global_params()
    platforms_root = params["platforms_root"]
    platforms_yaml = params["platforms_yaml"]
    platforms_data = parse_yaml(platforms_yaml)
    platform_list = platforms_data.get("platforms", [])
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
            continue

        logger.info(f"Update platform [{name}] success.")
        pass


##
# @brief tos.py update
#
@click.command(help="Update TuyaOpen dependencies.")
def cli():
    update_platform()
    pass
