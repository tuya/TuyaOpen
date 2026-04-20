#!/usr/bin/env python3
# coding=utf-8

from typing import Optional

import click

from tools.cli_command.cli_version import open_version


HELLO_BANNER = r"""
 ______                 ____
/_  __/_ ____ _____ _  / __ \___  ___ ___
 / / / // / // / _ `/ / /_/ / _ \/ -_) _ \
/_/  \_,_/\_, /\_,_/  \____/ .__/\__/_//_/
         /___/            /_/
"""


DEFAULT_EXIT_HINT = "Exit environment: `exit` or `deactivate`."


def print_hello(show_version: bool = True, exit_hint: Optional[str] = None) -> None:
    """Print the TuyaOpen greeting banner.

    Args:
        show_version: include the TuyaOpen version line when True.
        exit_hint:    trailing "how to exit" line.
                      * None (default) -> use DEFAULT_EXIT_HINT
                      * ""             -> suppress the hint line entirely
                      * anything else  -> print that string verbatim
    """
    separator = "*" * 40
    click.echo(separator)
    click.echo(HELLO_BANNER)
    if show_version:
        click.echo(f"TuyaOpen version: {open_version()}")
    click.echo(separator)
    click.echo("tos.py Tool and TuyaOpen SDK is now ready.")
    if exit_hint is None:
        click.echo(DEFAULT_EXIT_HINT)
    elif exit_hint != "":
        click.echo(exit_hint)


##
# @brief tos.py hello
#
@click.command(help="Show TuyaOpen greeting banner.",
               context_settings=dict(help_option_names=["-h", "--help"]))
@click.option("--no-version",
              is_flag=True, default=False,
              help="Do not print the TuyaOpen version line.")
@click.option("--exit-hint",
              type=str, default=None,
              help="Override the exit hint line. Pass an empty string to "
                   "suppress the hint entirely. Useful for shell wrappers "
                   "that want to print a shell-aware hint themselves.")
def cli(no_version, exit_hint):
    print_hello(show_version=not no_version, exit_hint=exit_hint)
    pass
