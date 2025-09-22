#!/usr/bin/env python3
# coding=utf-8

import os
import json
import shutil
from typing import Union, List

from tools.cli_command.util import (
    get_logger, get_running_env, do_subprocess
)


def rm_rf(file_path):
    if not os.path.exists(file_path):
        return True
    if "windows" == get_running_env():
        if os.path.isfile(file_path):
            cmd = f"del /F /Q \"{file_path}\""
        else:
            cmd = f"rmdir /S /Q \"{file_path}\""
    else:
        cmd = f"rm -rf \"{file_path}\""
    ret = do_subprocess(cmd)
    if ret != 0:
        return False
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


def move_directory(source, target, force=False) -> bool:
    logger = get_logger()
    if os.path.exists(target) and not force:
        logger.error(f"Can't move to {target}, because it already exists.")
        return False

    try:
        rm_rf(target)
        shutil.move(source, target)
    except Exception as e:
        logger.error(f"Move error: {str(e)}.")
        return False

    return True


def create_directory(target) -> bool:
    logger = get_logger()
    try:
        os.makedirs(target, exist_ok=True)
    except Exception as e:
        logger.error(f"Create {target}: {str(e)}.")
        return False

    return True


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


def get_subdir_from_path(target_path):
    ans = []
    if not os.path.isdir(target_path):
        return ans

    for entry in os.scandir(target_path):
        if entry.is_dir():
            ans.append(entry.name)

    return ans


def parser_para_file(json_file):
    logger = get_logger()
    if not os.path.isfile(json_file):
        logger.error(f"Error: Not found [{json_file}].")
        return {}
    try:
        f = open(json_file, 'r', encoding='utf-8')
        json_data = json.load(f)
        f.close()
    except Exception as e:
        print(f"Parser json error:  [{str(e)}].")
        return {}
    return json_data


def replace_string_in_file(file_path, old_str, new_str) -> bool:
    logger = get_logger()
    if not os.path.isfile(file_path):
        logger.error(f"Error: Not found [{file_path}].")
        return False
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()

        modified_content = content.replace(old_str, new_str)

        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(modified_content)

        logger.debug(f"replace [{old_str}] to [{new_str}] in [{file_path}].")
    except Exception as e:
        print(f"Replace string in {file_path} error:  [{str(e)}].")
        return False

    return True


def check_text_in_file(file_path, target_text):
    logger = get_logger()

    if not os.path.isfile(file_path):
        logger.warning(f"Not a file: {file_path}.")
        return False

    if len(target_text) == 0:
        logger.warning("Text is empty.")
        return False

    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            for line in file:
                if target_text in line:
                    return True
        return False
    except Exception as e:
        logger.error(f"Error: {str(e)}")
        return False
