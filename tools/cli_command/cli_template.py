#!/usr/bin/env python3
# coding=utf-8

import click

from tools.cli_command.util import get_logger, get_global_params


##
# @brief tos.py debug
#
@click.command()
def cli():
    logger = get_logger()
    params = get_global_params()
    logger.info(params)
    pass
