#!/usr/bin/env python3
# coding=utf-8

import sys

if sys.platform == 'win32':
    try:
        from click_pwsh import support_pwsh_shell_completion
        support_pwsh_shell_completion()
    except ImportError:
        pass

import click
import logging

from tools.cli_command.util import (
    set_clis, set_logger, set_global_params
)
from tools.cli_command.cli_version import cli as version_exec
from tools.cli_command.cli_check import cli as check_exec
from tools.cli_command.cli_config import cli as config_exec
from tools.cli_command.cli_build import cli as build_exec
from tools.cli_command.cli_clean import cli as clean_exec
from tools.cli_command.cli_flash import cli as flash_exec
from tools.cli_command.cli_monitor import cli as monitor_exec
from tools.cli_command.cli_update import cli as update_exec
from tools.cli_command.cli_new import cli as new_exec
from tools.cli_command.cli_dev import cli as dev_exec
from tools.cli_command.cli_idf import cli as idf_exec
from tools.cli_command.cli_hello import cli as hello_exec
from tools.cli_command.cli_prepare import cli as prepare_exec

CLIS = {
    "version": version_exec,
    "prepare": prepare_exec,
    "check": check_exec,
    "config": config_exec,
    "build": build_exec,
    "clean": clean_exec,
    "flash": flash_exec,
    "monitor": monitor_exec,
    "update": update_exec,
    "new": new_exec,
    "dev": dev_exec,
    "idf": idf_exec,
    "hello": hello_exec,
}


@click.command(cls=set_clis(CLIS),
               help="Tuya Uart Tool.",
               context_settings=dict(help_option_names=["-h", "--help"]))
@click.option('-d', '--debug',
              is_flag=True, default=False,
              help="Show debug message")
def run(debug):
    log_level = logging.INFO
    if debug:
        log_level = logging.DEBUG
    logger = set_logger(log_level)
    set_global_params()
    logger.info("Running tos.py ...")
    pass


if __name__ == '__main__':
    run()
    pass
