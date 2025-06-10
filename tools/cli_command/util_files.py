#!/usr/bin/env python3
# coding=utf-8

import os
import shutil
from typing import Union, List

from tools.cli_command.util import get_logger


def rm_rf(file_path):
    if os.path.isfile(file_path):
        os.remove(file_path)
    elif os.path.isdir(file_path):
        shutil.rmtree(file_path)
    return True


def copy_file(source, target, force=True) -> bool:
    '''
    force: Overwrite if the target file exists
    '''
    logger = get_logger()
    if not os.path.exists(source):
        logger.error(f"Not found [{source}].")
        return False
    if not force and os.path.exists(target):
        return True

    target_dir = os.path.dirname(target)
    if target_dir:
        os.makedirs(target_dir, exist_ok=True)
    shutil.copy(source, target)
    return True


def copy_directory(source, target) -> bool:
    logger = get_logger()
    if not os.path.exists(source):
        logger.error(f"Not found [{source}].")
        return False
    if target == source:
        logger.warning(f"Copy use same path [{source}].")
        return False

    os.makedirs(target, exist_ok=True)
    shutil.copytree(source, target, dirs_exist_ok=True)
    pass


def _find_files(file_type: str, target_dir: str, max_depth: int) -> List[str]:
    result = []

    def _search_dir(current_dir, current_depth):
        if max_depth != 0 and current_depth > max_depth:
            return

        for entry in os.scandir(current_dir):
            if entry.is_file() and entry.name.endswith(f'{file_type}'):
                result.append(entry.path)
            elif entry.is_dir():
                _search_dir(entry.path, current_depth + 1)

    _search_dir(target_dir, 1)
    return result


def get_files_from_path(types: Union[str, List[str]],
                        dirs: Union[str, List[str]],
                        maxdepth: int = 1) -> List[str]:
    logger = get_logger()
    types = [types] if isinstance(types, str) else types
    dirs = [dirs] if isinstance(dirs, str) else dirs

    result = []
    for dir in dirs:
        if not os.path.exists(dir):
            logger.debug(f"Not found [{dir}]")
            continue
        for tp in types:
            rst = _find_files(tp, dir, maxdepth)
            result += rst
    return result
