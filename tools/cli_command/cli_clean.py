#!/usr/bin/env python3
# coding=utf-8

import os
import click

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    do_subprocess, redirect_stdout_stderr_to
)
from tools.cli_command.util_files import rm_rf


def clean_project(log_file=None):
    """Clean build artifacts. If log_file is provided, output will be redirected to that file."""

    def _run():
        logger = get_logger()
        params = get_global_params()
        build_path = params["app_build_path"]
        build_file = os.path.join(build_path, "build.ninja")
        if not os.path.isfile(build_file):
            logger.debug("No need clean.")
            return True

        cmd = "ninja clean_all"
        ninja_cmd = f"cd {build_path} && {cmd}"

        ret = do_subprocess(ninja_cmd)
        if 0 != ret:
            logger.error("Clean error.")
            return False

        logger.note("Clean success.")
        return True

    if log_file:
        with redirect_stdout_stderr_to(log_file):
            return _run()
    return _run()


def full_clean_project(log_file=None, log_file_append=False):
    """Perform a full clean; if log_file is provided, output will be redirected to that file. If log_file_append is True, append to the file."""

    def _run():
        logger = get_logger()
        clean_project()
        params = get_global_params()
        build_path = params["app_build_path"]
        rm_rf(build_path)
        logger.note("Fullclean success.")

    if log_file:
        with redirect_stdout_stderr_to(log_file, append=log_file_append):
            _run()
    else:
        _run()


##
# @brief tos.py clean
#
@click.command(help="Clean the project.")
@click.option('-f', '--full',
              is_flag=True, default=False,
              help="Full clean.")
@click.option('-o', '--log-file',
              type=click.Path(),
              default=None,
              help="Write clean log to the specified file instead of terminal.")
@click.option('-a', '--append',
              is_flag=True, default=False,
              help="Append to log file instead of overwriting (only with -o and -f).")
def cli(full, log_file, append):
    check_proj_dir()
    if full:
        full_clean_project(log_file=log_file, log_file_append=append)
    else:
        clean_project(log_file=log_file)
    pass