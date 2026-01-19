#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import click
import shlex

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    parse_config_file
)


def run_idf_command(idf_command, idf_flags=""):
    """
    Run arbitrary idf.py commands with optional flags.
    
    Args:
        idf_command: Tuple of command arguments (e.g., ('clean',) or ('set-target', 'esp32'))
        idf_flags: Optional flags to pass to idf.py (e.g., "-v" or "-D CONFIG_EXAMPLE=1")
    
    Returns:
        bool: True if successful, False otherwise
    """
    logger = get_logger()
    params = get_global_params()
    
    # Get platform information
    using_config = params.get("using_config")
    if not using_config or not os.path.exists(using_config):
        logger.error("Project not configured. Please run 'tos.py config' first.")
        return False
    
    using_data = parse_config_file(using_config)
    platform_name = using_data.get("CONFIG_PLATFORM_CHOICE", "")
    
    if platform_name != "ESP32":
        logger.error(f"The 'idf' command is only supported for ESP32 platform, current platform: {platform_name}")
        return False
    
    # Import ESP32-specific utilities
    platforms_root = params["platforms_root"]
    platform_root = os.path.join(platforms_root, platform_name)
    sys.path.insert(0, os.path.join(platform_root, "tools"))
    
    try:
        from util import execute_idf_commands
    except ImportError:
        logger.error("Failed to import ESP32 utilities. Please ensure the platform is properly set up.")
        return False
    
    # Build the complete idf.py command
    cmd_parts = ["idf.py"]
    
    # Add flags if provided
    if idf_flags:
        # Properly split flags while preserving quoted arguments
        flags_list = shlex.split(idf_flags)
        cmd_parts.extend(flags_list)
    
    # Add the command arguments
    cmd_parts.extend(list(idf_command))
    
    # Create the full command string
    cmd = " ".join(cmd_parts)
    
    # Determine the working directory
    # For most idf.py commands, we want to run in the tuya_open_sdk directory
    directory = os.path.join(platform_root, "tuya_open_sdk")
    if not os.path.exists(directory):
        logger.error(f"Directory not found: {directory}")
        return False
    
    logger.info(f"Running: {cmd}")
    
    # Execute the command - Now with correct number of arguments
    if not execute_idf_commands(platform_root, cmd, directory):
        logger.error("Command failed.")
        return False
    
    logger.note("Command completed successfully.")
    return True


##
# @brief tos.py idf
#
@click.command(help="Run arbitrary idf.py commands (ESP32 only).")
@click.argument('idf_command', nargs=-1, required=True)
@click.option('--idf-flags',
              default="",
              help="Additional flags to pass to idf.py (e.g., '-v' or '-D CONFIG_EXAMPLE=1').")
def cli(idf_command, idf_flags):
    """
    Run arbitrary idf.py commands.
    
    Examples:
        tos.py idf clean
        tos.py idf --idf-flags="-v" menuconfig
        tos.py idf --idf-flags="-D IDF_TARGET=esp32s3" set-target esp32s3
        tos.py idf --idf-flags="-p /dev/ttyUSB0" flash
    """
    check_proj_dir()
    
    if not run_idf_command(idf_command, idf_flags):
        sys.exit(1)
    
    sys.exit(0)