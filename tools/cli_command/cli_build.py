#!/usr/bin/env python
# coding=utf-8

import os
import sys
import click
import shutil

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    env_read, env_write, parse_config_file, parse_yaml,
    do_subprocess, get_country_code
)
from tools.cli_command.util_git import (
    git_clone, git_checkout, set_repo_mirro, git_get_commit
)
from tools.cli_command.util_files import rm_rf
from tools.cli_command.cli_check import update_submodules
from tools.cli_command.cli_config import init_using_config


def env_check():
    before_updated = env_read("update_submodules", default_value=False)
    if before_updated:
        return True
    ret = update_submodules()
    env_write("update_submodules", ret)
    return ret


def get_platform_info(platform):
    logger = get_logger()
    params = get_global_params()
    platforms_yaml = params["platforms_yaml"]
    platforms_data = parse_yaml(platforms_yaml)
    platform_list = platforms_data.get("platforms", [])
    for p in platform_list:
        if p.get("name", "") == platform:
            return p
    else:
        logger.error(f"Not found platform [{platform}] in yaml.")
        return {}


def check_platform_commit(repo_path, commit):
    logger = get_logger()
    params = get_global_params()
    if not os.path.exists(repo_path):
        logger.error(f"Not found {repo_path}")
        return False

    if not commit:
        # Maybe a newly created platform
        return True

    dont_update_platform = params["dont_update_platform"]
    need_prompt = not os.path.exists(dont_update_platform)

    real_commit = git_get_commit(repo_path)
    if need_prompt and real_commit != commit:
        logger.warning(f"The commit required by the platform is {commit},")
        logger.warning(f"but currently {real_commit} is being used.")
        logger.info("Update the platform to the required commit?")
        logger.note("y(es) / n(o) / d(on't prompt again)")
        ret = input("input: ").upper()
        if ret == "Y":
            if not git_checkout(repo_path, commit):
                logger.error("Update platform error. Please try again.")
                return False
            logger.note("Platform updated successfully.")
        elif ret == "N":
            logger.info("Use command [tos.py update] to update the platform.")
        elif ret == "D":
            with open(dont_update_platform, 'w') as f:
                f.write("1")

    return True


def download_platform(platform):
    '''
    When the platform path does not exist,
    git clone the repository and switch to the commit
    '''
    logger = get_logger()
    params = get_global_params()
    platforms_root = params["platforms_root"]
    platform_root = os.path.join(platforms_root, platform)
    platform_info = get_platform_info(platform)
    repo = platform_info.get("repo", "")
    commit = platform_info.get("commit", "")

    if os.path.exists(platform_root):
        logger.info(f"Platform [{platform}] is exists.")
        if not check_platform_commit(platform_root, commit):
            return False
        return True

    logger.info(f"Downloading platform [{platform}] ...")

    code = get_country_code()
    if code == "China":
        set_repo_mirro(unset=False)

    try:
        if not git_clone(repo, platform_root) \
                or not git_checkout(platform_root, commit):
            return False
        return True
    finally:
        if code == "China":
            set_repo_mirro(unset=True)

def prepare_platform(platform, chip=""):
    '''
    Execute:
    python ./platform/xxx/platform_prepare.py $CHIP
    or
    ./platform_prepare.sh $CHIP
    '''
    logger = get_logger()
    params = get_global_params()
    platforms_root = params["platforms_root"]
    platform_root = os.path.join(platforms_root, platform)
    prepare_py = os.path.join(platform_root, "platform_prepare.py")
    prepare_sh = os.path.join(platform_root, "platform_prepare.sh")
    if not os.path.exists(prepare_py) and not os.path.exists(prepare_sh):
        logger.debug("no need platform prepare.")
        return True

    if os.path.exists(prepare_py):
        parpare_cmd = "python platform_prepare.py"
    else:
        parpare_cmd = "./platform_prepare.sh"

    logger.info(f"Preparing platform [{platform}] ...")
    cmd = f"cd {platform_root} && {parpare_cmd} {chip}"
    ret = do_subprocess(cmd)
    if 0 != ret:
        return False
    return True


def build_setup(platform, project_name, framework, chip=""):
    '''
    Execute:
    python ./platform/xxx/build_setup.py
    $PROJ_NAME $PLATFORM $FRAMEWORK $CHIP
    or
    ./build_setup.sh $PROJ_NAME $PLATFORM $FRAMEWORK $CHIP
    '''
    logger = get_logger()
    params = get_global_params()
    platforms_root = params["platforms_root"]
    platform_root = os.path.join(platforms_root, platform)
    setup_py = os.path.join(platform_root, "build_setup.py")
    setup_sh = os.path.join(platform_root, "build_setup.sh")
    if not os.path.exists(setup_py) and not os.path.exists(setup_sh):
        logger.debug("no need build setup.")
        return True

    if os.path.exists(setup_py):
        setup_cmd = "python build_setup.py"
    else:
        setup_cmd = "./build_setup.sh"

    logger.info("Build setup ...")
    cmd = f"cd {platform_root} && {setup_cmd} "
    cmd += f"{project_name} {platform} {framework} {chip}"
    ret = do_subprocess(cmd)
    if 0 != ret:
        return False
    return True


def cmake_configure(using_data, verbose=False):
    '''
    cmake -G Ninja $CMAKE_VERBOSE $OPEN_SDK_ROOT
    -DTOS_PROJECT_NAME=$PROJ
    -DTOS_PROJECT_ROOT=$PROJECT_ROOT
    -DTOS_PROJECT_PLATFORM=$PROJECT_PLATFORM
    -DTOS_FRAMEWORK=$PROJECT_FRAMEWORK
    -DTOS_PROJECT_CHIP=$PROJECT_CHIP
    -DTOS_PROJECT_BOARD=$PROJECT_BOARD
    '''
    params = get_global_params()
    open_root = params["open_root"]
    cmd = f"cmake -G Ninja {open_root} "
    if verbose:
        cmd += "-DCMAKE_VERBOSE_MAKEFILE=ON "

    project_name = using_data.get("CONFIG_PROJECT_NAME", "")
    app_root = params["app_root"]
    platform_name = using_data.get("CONFIG_PLATFORM_CHOICE", "")
    framework = using_data.get("CONFIG_FRAMEWORK_CHOICE", "")
    chip_name = using_data.get("CONFIG_CHIP_CHOICE", "")
    board_name = using_data.get("CONFIG_BOARD_CHOICE", "")
    defines = [
        f"-DTOS_PROJECT_NAME={project_name}",
        f"-DTOS_PROJECT_ROOT={app_root}",
        f"-DTOS_PROJECT_PLATFORM={platform_name}",
        f"-DTOS_FRAMEWORK={framework}",
        f"-DTOS_PROJECT_CHIP={chip_name}",
        f"-DTOS_PROJECT_BOARD={board_name}",
    ]

    cmd += " ".join(defines)

    build_path = params["app_build_path"]
    cmake_cmd = f"cd {build_path} && {cmd}"
    ret = do_subprocess(cmake_cmd)
    if 0 != ret:
        return False
    return True


def ninja_build(build_path, verbose=False):
    build_file = os.path.join(build_path, "build.ninja")
    if not os.path.isfile(build_file):
        return False

    cmd = "ninja example "
    if verbose:
        cmd += "--verbose "

    ninja_cmd = f"cd {build_path} && {cmd}"
    ret = do_subprocess(ninja_cmd)
    if 0 != ret:
        return False
    return True


def copy_compile_commands(build_path):
    '''
    copy compile_commands.json from build directory to project root
    '''
    logger = get_logger()
    params = get_global_params()
    
    src_file = os.path.join(build_path, "compile_commands.json")
    dst_file = os.path.join(params["open_root"], "compile_commands.json")

    logger.debug(f"Copying compile_commands.json from {src_file} to {dst_file}")
    
    if not os.path.exists(src_file):
        logger.debug(f"compile_commands.json not found in {build_path}")
        return True
    
    try:
        if os.path.exists(dst_file):
            os.remove(dst_file)
            logger.debug(f"Removed existing {dst_file}")
        shutil.copy2(src_file, dst_file)
        logger.debug(f"Copied compile_commands.json to {dst_file}")
        return True
    except Exception as e:
        logger.warning(f"Failed to copy compile_commands.json: {str(e)}")
        return True  # Don't fail the build if copy fails


def check_bin_file(using_data,):
    '''
    check bin file exists
    and copy to dist
    '''
    logger = get_logger()
    params = get_global_params()

    app_bin_path = params["app_bin_path"]
    dist_root = params["dist_root"]

    app_name = using_data.get("CONFIG_PROJECT_NAME", "")
    app_ver = using_data.get("CONFIG_PROJECT_VERSION", "")

    bin_name = f"{app_name}_QIO_{app_ver}.bin"
    app_bin_file = os.path.join(app_bin_path, bin_name)
    if not os.path.exists(app_bin_file):
        logger.error(f"Not found {app_bin_file}")
        return False

    dist_path = os.path.join(dist_root, f"{app_name}_{app_ver}")
    os.makedirs(dist_root, exist_ok=True)
    rm_rf(dist_path)
    shutil.copytree(app_bin_path, dist_path)

    platform_name = using_data.get("CONFIG_PLATFORM_CHOICE", "")
    framework = using_data.get("CONFIG_FRAMEWORK_CHOICE", "")
    chip_name = using_data.get("CONFIG_CHIP_CHOICE", "")
    board_name = using_data.get("CONFIG_BOARD_CHOICE", "")

    build_info = f'''
====================[ BUILD SUCCESS ]===================
 Target    : {bin_name}
 Output    : {dist_path}
 Platform  : {platform_name}
 Chip      : {chip_name}
 Board     : {board_name}
 Framework : {framework}
========================================================
    '''

    logger.note(f"{build_info}")
    return True


def build_project(verbose=False):
    logger = get_logger()
    check_proj_dir()

    if not env_check():
        logger.error("Env check error.")
        return False

    init_using_config(force=False)
    params = get_global_params()
    using_config = params["using_config"]
    using_data = parse_config_file(using_config)
    platform_name = using_data.get("CONFIG_PLATFORM_CHOICE", "")
    if not platform_name:
        logger.error("Not fount platform name.")
        return False

    if not download_platform(platform_name):
        logger.error("Download platform error.")
        return False
    logger.info(f"Platform [{platform_name}] downloaded successfully.")

    chip_name = using_data.get("CONFIG_CHIP_CHOICE", "")
    if not prepare_platform(platform_name, chip_name):
        logger.error("Prepare platform error.")
        return False
    logger.info(f"Platform [{platform_name}] prepared successfully.")

    project_name = using_data.get("CONFIG_PROJECT_NAME", "")
    framework = using_data.get("CONFIG_FRAMEWORK_CHOICE", "")
    if not build_setup(platform_name, project_name, framework, chip_name):
        logger.error("Build setup error.")
        return False
    logger.info(f"Build setup for [{project_name}] success.")

    if not cmake_configure(using_data, verbose):
        logger.error("Cmake configure error.")
        return False
    logger.info("Cmake configure success.")

    build_path = params["app_build_path"]
    if not ninja_build(build_path, verbose):
        logger.error("Build error.")
        return False

    copy_compile_commands(build_path)

    if not check_bin_file(using_data,):
        return False

    return True


##
# @brief tos.py build
#
@click.command(help="Build the project.")
@click.option('-v', '--verbose',
              is_flag=True, default=False,
              help="Show verbose message.")
def cli(verbose):
    if not build_project(verbose):
        sys.exit(1)
    sys.exit(0)
