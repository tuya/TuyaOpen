#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import json
import yaml
import click
import requests
import platform
import logging
import contextlib
from typing import List


def set_clis(clis):
    class CLIClass(click.Group):
        def list_commands(self, ctx):
            return list(clis.keys())

        def get_command(self, ctx, cmd_name):
            if cmd_name not in clis.keys():
                return None
            return clis[cmd_name]
    return CLIClass


NOTE = 25  # INFO(20)  WARNING(30)
logging.addLevelName(NOTE, "NOTE")


def note(self, message, *args, **kws):
    if self.isEnabledFor(NOTE):
        self._log(NOTE, message, args, **kws)


logging.Logger.note = note


class CustomFormatter(logging.Formatter):
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'
    # Format configuration
    default_format = "[%(levelname)s]: %(message)s"

    FORMATS = {
        NOTE: GREEN + default_format + RESET,
        logging.WARNING: YELLOW + default_format + RESET,
        logging.ERROR: RED + default_format + RESET,
        logging.DEBUG: default_format,
        logging.INFO: default_format,
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno, self.default_format)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


OPEN_LOGGER = None
OPEN_LOGGER_H = None
GLOBAL_PARAMS = {}


def set_logger(level=logging.WARNING):
    global OPEN_LOGGER
    global OPEN_LOGGER_H

    logger = logging.getLogger("open_logger")
    logger.setLevel(level)
    # Output redirection must match GUI usage
    # Using stderr allows runtime errors to appear in the GUI
    lh = logging.StreamHandler(stream=sys.stderr)
    lh.setFormatter(CustomFormatter())
    logger.addHandler(lh)
    logger.debug("open_logger init done.")

    OPEN_LOGGER = logger
    OPEN_LOGGER_H = lh
    return OPEN_LOGGER


def get_logger():
    global OPEN_LOGGER
    if OPEN_LOGGER:
        return OPEN_LOGGER
    set_logger()
    return OPEN_LOGGER


def set_log_stream(stream):
    """
    Redirect logger output to the given stream (e.g. a file object).
    Only affects Python logging output; does not change process stdout/stderr FDs.
    """
    global OPEN_LOGGER_H
    if OPEN_LOGGER_H is not None and stream is not None:
        OPEN_LOGGER_H.stream = stream


@contextlib.contextmanager
def redirect_stdout_stderr_to(filepath, encoding="utf-8", append=False):
    """
    Redirect process stdout/stderr to the given file (including subprocess output).
    Redirection is at file descriptor (FD) level, so output from subprocesses
    started via os.system/subprocess (e.g. cmake, ninja) is also written to the file.
    Restored on exit from the context. append: if True open in append mode, else overwrite.
    """
    get_logger()  # Ensure logger is initialized so set_log_stream works
    mode = "a" if append else "w"
    with open(filepath, mode, encoding=encoding) as f:
        fd = f.fileno()
        # Save current stdout(1) and stderr(2) FDs for restore on exit
        saved_stdout = os.dup(1)
        saved_stderr = os.dup(2)
        try:
            # Redirect at FD level; subprocesses inherit 1 and 2, so their output goes to file
            os.dup2(fd, 1)
            os.dup2(fd, 2)
            # Python-level stdout/stderr and print() also go to the same file
            sys.stdout = sys.stderr = f
            # Logger StreamHandler writes to the same file
            set_log_stream(f)
            yield f
        finally:
            f.flush()
            # Restore FDs 1 and 2 to original terminal/pipe
            os.dup2(saved_stdout, 1)
            os.dup2(saved_stderr, 2)
            os.close(saved_stdout)
            os.close(saved_stderr)
            # Restore Python stdout/stderr and logger output target
            sys.stdout = sys.__stdout__
            sys.stderr = sys.__stderr__
            set_log_stream(sys.stderr)


def set_global_params():
    global GLOBAL_PARAMS

    GLOBAL_PARAMS["python"] = os.environ.get("OPEN_SDK_PYTHON", "python")
    open_root = os.path.dirname(os.path.abspath(sys.argv[0]))
    app_root = os.getcwd()

    GLOBAL_PARAMS["open_root"] = open_root
    GLOBAL_PARAMS["app_root"] = app_root
    GLOBAL_PARAMS["boards_root"] = os.path.join(
        open_root, "boards")
    GLOBAL_PARAMS["src_root"] = os.path.join(
        open_root, "src")
    tools_root = os.path.join(open_root, "tools")
    GLOBAL_PARAMS["tools_root"] = tools_root

    GLOBAL_PARAMS["app_cmakefile"] = os.path.join(
        app_root, "CMakeLists.txt")
    GLOBAL_PARAMS["app_default_config"] = os.path.join(
        app_root, "app_default.config")
    GLOBAL_PARAMS["app_configs_path"] = os.path.join(
        app_root, "config")

    build_path = os.path.join(app_root, ".build")
    GLOBAL_PARAMS["app_build_path"] = build_path
    GLOBAL_PARAMS["app_bin_path"] = os.path.join(
        build_path, "bin")

    GLOBAL_PARAMS["dist_root"] = os.path.join(app_root, "dist")

    cache_path = os.path.join(build_path, "cache")
    GLOBAL_PARAMS["app_cache_path"] = cache_path
    GLOBAL_PARAMS["catalog_kconfig"] = os.path.join(
        cache_path, "CatalogKconfig")
    GLOBAL_PARAMS["using_config"] = os.path.join(
        cache_path, "using.config")

    open_cache_path = os.path.join(open_root, ".cache")
    GLOBAL_PARAMS["env_json"] = os.path.join(
        open_cache_path, ".env.json")
    GLOBAL_PARAMS["dont_update_platform"] = os.path.join(
        open_cache_path, ".dont_prompt_update_platform")

    GLOBAL_PARAMS["build_param_root"] = os.path.join(
        build_path, "build")

    platforms_root = os.path.join(open_root, "platform")
    GLOBAL_PARAMS["platforms_root"] = platforms_root
    GLOBAL_PARAMS["platforms_yaml"] = os.path.join(
        platforms_root, "platform_config.yaml")

    tyutool_root = os.path.join(tools_root, "tyutool")
    GLOBAL_PARAMS["tyutool_root"] = tyutool_root
    tyutool_cli = os.path.join(tyutool_root, "tyutool_cli.py")
    GLOBAL_PARAMS["tyutool_cli"] = tyutool_cli

    porting_root = os.path.join(tools_root, "porting")
    GLOBAL_PARAMS["porting_root"] = porting_root

    app_template_root = os.path.join(tools_root, "app_template")
    GLOBAL_PARAMS["app_template_root"] = app_template_root

    board_template_root = os.path.join(tools_root, "board_template")
    GLOBAL_PARAMS["board_template_root"] = board_template_root

    pass


def get_global_params():
    global GLOBAL_PARAMS
    if GLOBAL_PARAMS:
        return GLOBAL_PARAMS
    set_global_params()
    return GLOBAL_PARAMS


def check_proj_dir():
    logger = get_logger()
    params = get_global_params()
    app_root = params["app_root"]
    open_root = params["open_root"]
    if app_root == open_root:
        logger.error("TuyaOpen root cannot be regarded as project root.")
        sys.exit(1)
    app_cmakefile = params["app_cmakefile"]
    if not os.path.exists(app_cmakefile):
        logger.error(f"Not project root [{app_root}].")
        sys.exit(2)
    pass


def list_menu(tittle: str, contexts: List[str]) -> (str, int):
    print("--------------------")
    for i in range(len(contexts)):
        print(f"{i+1}. {contexts[i]}")
    print("--------------------")
    print("Input \"q\" to exit.")
    while True:
        try:
            key = input(f"{tittle}: ")
            if "q" == key:
                sys.exit(0)
            num = int(key)
            if 1 <= num <= len(contexts):
                return contexts[num-1], int(num)-1
        except ValueError:
            continue
        except KeyboardInterrupt:
            sys.exit(0)
    pass


# "China" or other
COUNTRY_CODE = os.environ.get('OPEN_COUNTRY_CODE', "")


def set_country_code():
    logger = get_logger()
    global COUNTRY_CODE
    if len(COUNTRY_CODE):
        return COUNTRY_CODE

    try:
        response = requests.get('http://www.ip-api.com/json', timeout=5)
        response.raise_for_status()
        logger.debug(response.elapsed)

        result = response.json()
        country = result.get("country", "")
        logger.debug(f"country code: {country}")

        COUNTRY_CODE = country
    except requests.exceptions.RequestException as e:
        logger.warning(f"country code error: {e}")

    return COUNTRY_CODE


def get_country_code():
    global COUNTRY_CODE
    if len(COUNTRY_CODE):
        return COUNTRY_CODE
    return set_country_code()


RUNNING_ENV = ""


def set_running_env():
    global RUNNING_ENV
    _env = platform.system().lower()
    if "linux" in _env:
        RUNNING_ENV = "linux"
    elif "darwin" in _env:
        machine = "x86" if "x86" in platform.machine().lower() else "arm64"
        RUNNING_ENV = f"darwin_{machine}"
    else:
        RUNNING_ENV = "windows"
    return RUNNING_ENV


def get_running_env():
    """
    Returns one of: linux, darwin_x86, darwin_arm64, windows.
    """
    global RUNNING_ENV
    if len(RUNNING_ENV):
        return RUNNING_ENV
    return set_running_env()


def env_read(key: str, default_value=None):
    logger = get_logger()
    params = get_global_params()
    env_json = params["env_json"]
    if not os.path.exists(env_json):
        return default_value

    env_data = {}
    try:
        f = open(env_json, 'r', encoding='utf-8')
        env_data = json.load(f)
        f.close()
    except Exception as e:
        logger.error(f"Read env json error: {e}")
    return env_data.get(key, default_value)


def env_write(key: str, value):
    logger = get_logger()
    params = get_global_params()
    env_json = params["env_json"]
    env_data = {}
    if os.path.exists(env_json):
        try:
            f = open(env_json, 'r', encoding='utf-8')
            env_data = json.load(f)
            f.close()
        except Exception as e:
            logger.error(f"Open env json error: {e}")

    env_data[key] = value
    json_str = json.dumps(env_data, indent=4, ensure_ascii=False)
    os.makedirs(os.path.dirname(env_json), exist_ok=True)
    with open(env_json, 'w') as f:
        f.write(json_str)
    pass


def _conf2param(conf_list, params_data):
    conf_lines = []
    for f in conf_list:
        conf_f = open(f, 'r', encoding="utf-8")
        conf_lines += conf_f.readlines()
        conf_f.close()

    for cl in conf_lines:
        cl = cl.strip()
        if not cl.startswith("CONFIG_"):
            continue
        ori_key = cl.split('=', 1)[0]
        ori_value = cl.split('=', 1)[1]
        if ori_value.startswith('\"'):  # str
            params_data[ori_key] = ori_value.strip("\"")
        elif ori_value == "y":  # bool
            params_data[ori_key] = True
        elif ori_value.isdigit():  # int
            params_data[ori_key] = int(ori_value)
        else:  # hex
            params_data[ori_key] = f"{ori_value}"
    pass


def parse_config_file(config):
    params_data = {}
    if os.path.exists(config):
        _conf2param([config], params_data)
    return params_data


def parse_yaml(yaml_file: str) -> dict:
    logger = get_logger()
    if not os.path.exists(yaml_file):
        logger.error(f"Not found [{yaml_file}]")
        return {}
    try:
        with open(yaml_file, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
        return data
    except yaml.YAMLError as e:
        logger.error(f"YAML error: {e}")
        return {}


def do_subprocess(cmd: str) -> int:
    logger = get_logger()

    if not cmd:
        logger.warning("Subprocess cmd is empty.")
        return 0

    logger.info(f">>> subprocess >>>\n{cmd}")

    ret = 1  # 0 means success
    try:
        ret = os.system(cmd)
    except Exception as e:
        logger.error(f"Do subprocess error: {str(e)}")
        logger.info(f"do subprocess: {cmd}")
        return 1
    return ret