#!/usr/bin/env python3
# coding=utf-8

import click

from tools.cli_command.cli_version import open_version


HELLO_BANNER = r"""
 ______                 ____
/_  __/_ ____ _____ _  / __ \___  ___ ___
 / / / // / // / _ `/ / /_/ / _ \/ -_) _ \
/_/  \_,_/\_, /\_,_/  \____/ .__/\__/_//_/
         /___/            /_/
"""


def print_hello(show_version: bool = True) -> None:
    """Print the TuyaOpen greeting banner.

    Args:
        show_version: include the TuyaOpen version line when True.
    """
    separator = "*" * 40
    click.echo(separator)
    click.echo(HELLO_BANNER)
    if show_version:
        click.echo(f"TuyaOpen version: {open_version()}")
    click.echo(separator)


##
# @brief tos.py hello
#
@click.command(help="Show TuyaOpen greeting banner.",
               context_settings=dict(help_option_names=["-h", "--help"]))
@click.option("--no-version",
              is_flag=True, default=False,
              help="Do not print the TuyaOpen version line.")
def cli(no_version):
    print_hello(show_version=not no_version)
    pass
