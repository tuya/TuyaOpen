#!/usr/bin/env python3
# -*- coding: utf-8 -*-
##
# @file set_catalog_config.py
# @brief source [board] and [src] Kconfig file
# @author Tuya
# @version 1.0.0
# @date 2023-04-10


import os
import argparse


KCONFIG = "Kconfig"


def set_catalog_config(board, src_dir, app_dir, output_config):
    app_name = "none"
    if app_dir:
        app_name = os.path.basename(app_dir)
    context = f'''# CatalogKconfig
menu "configure project"
    config PROJECT_NAME
        string "PROJECT_NAME"
        default "{app_name}"
    config PROJECT_VERSION
        string "PROJECT_VERSION"
        default "1.0.0"
    choice
        prompt "Choose framework"
        default PROJECT_FRAMEWORK_BASE

        config PROJECT_FRAMEWORK_BASE
            bool "base"

        config PROJECT_FRAMEWORK_ARDUINO
            bool "arduino"
    endchoice
    config FRAMEWORK_CHOICE
        string
        default "base" if PROJECT_FRAMEWORK_BASE
        default "arduino" if PROJECT_FRAMEWORK_ARDUINO
endmenu

'''
    if app_dir:
        config_path = os.path.join(app_dir, KCONFIG)
        config_path = config_path.replace('\\', '/')  # win
        if os.path.exists(config_path):
            context += f'source \"{config_path}\"\n'
    if board:
        config_path = os.path.join(board, KCONFIG)
        config_path = config_path.replace('\\', '/')  # win
        if os.path.exists(config_path):
            context += f'source \"{config_path}\"\n'
    config_path = os.path.join(src_dir, KCONFIG)
    config_path = config_path.replace('\\', '/')  # win
    if os.path.exists(config_path):
        context += f'source \"{config_path}\"\n'

    output_dir = os.path.dirname(output_config)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    with open(output_config, 'w', encoding='utf-8') as f:
        f.write(context)
    return True


def main():
    parse = argparse.ArgumentParser(
        usage="-b <board> -s src -o cache/Kconfig",
        description="Generate the Kconfig catalog.")
    parse.add_argument('-b', '--board', type=str, default=None,
                       help="Board name. [None]", metavar="")
    parse.add_argument('-s', '--src', type=str, default="src",
                       help="Src directory. [src]", metavar="")
    parse.add_argument('-a', '--app', type=str, default=None,
                       help="Application directory. [None]", metavar="")
    parse.add_argument('-o', '--output', type=str, default="cache/Kconfig",
                       help="Output file of Kconfig. [cache/Kconfig]",
                       metavar="")
    args = parse.parse_args()

    board = args.board
    src_dir = args.src
    app_dir = args.app
    output_config = args.output
    # print(f'board: {board}')
    # print(f'src_dir: {src_dir}')
    # print(f'app_dir: {app_dir}')
    # print(f'output_config: {output_config}')

    set_catalog_config(board, src_dir, app_dir, output_config)
    pass


if __name__ == '__main__':
    main()
    pass
