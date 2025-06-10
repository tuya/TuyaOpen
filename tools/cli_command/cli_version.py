#!/usr/bin/env python3
# coding=utf-8

import click

from tools.cli_command.util import get_logger, get_global_params
from tools.cli_command.util_git import get_git_tag_describe

OPEN_VERSION = ""


def open_version():
    global OPEN_VERSION
    if OPEN_VERSION:
        return OPEN_VERSION

    global_params = get_global_params()
    version = get_git_tag_describe(global_params["open_root"])
    if version:
        OPEN_VERSION = version
    else:
        OPEN_VERSION = "Unknow version"

    return OPEN_VERSION


##
# @brief tos.py version
#
@click.command(help="Show version.",
               context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    logger = get_logger()
    version = open_version()
    logger.info(version)
    pass
