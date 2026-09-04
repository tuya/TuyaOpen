#!/usr/bin/env python
# coding=utf-8

import os
import re
import sys
import click
import shutil

from tools.cli_command.util import (
    get_logger, get_global_params, check_proj_dir,
    env_read, env_write, parse_config_file, parse_yaml,
    do_subprocess, get_country_code, export_country_code,
    redirect_stdout_stderr_to
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
        cmd = [params["python"], "platform_prepare.py"]
    else:
        cmd = ["./platform_prepare.sh"]

    logger.info(f"Preparing platform [{platform}] ...")
    # chip defaults to "" (some platforms don't take one); appended only
    # when set, since a passed-through empty string would otherwise become
    # a real (unwanted) extra argument once shell=True string interpolation
    # is replaced by an argv list.
    if chip:
        cmd.append(chip)
    ret = do_subprocess(cmd, cwd=platform_root)
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
        cmd = [params["python"], "build_setup.py"]
    else:
        cmd = ["./build_setup.sh"]

    logger.info("Build setup ...")
    cmd += [project_name, platform, framework]
    # See prepare_platform() above: chip is optional, only append when set.
    if chip:
        cmd.append(chip)
    ret = do_subprocess(cmd, cwd=platform_root)
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
    cmd = ["cmake", "-G", "Ninja", open_root]
    if verbose:
        cmd.append("-DCMAKE_VERBOSE_MAKEFILE=ON")
    # Baked into build.ninja at configure time, so -- unlike the subprocess
    # call sites -- this cannot be corrected later from the environment.
    cmd.append(f"-DTOS_PYTHON={params['python']}")

    project_name = using_data.get("CONFIG_PROJECT_NAME", "")
    app_root = params["app_root"]
    platform_name = using_data.get("CONFIG_PLATFORM_CHOICE", "")
    framework = using_data.get("CONFIG_FRAMEWORK_CHOICE", "")
    chip_name = using_data.get("CONFIG_CHIP_CHOICE", "")
    board_name = using_data.get("CONFIG_BOARD_CHOICE", "")
    # Expose an app's private esp-idf components to the platform build via the
    # TUYAOS_EXTRA_COMPONENT_DIRS env var, consumed only by ESP32's
    # platform/ESP32/tuya_open_sdk/CMakeLists.txt (each root is appended to
    # EXTRA_COMPONENT_DIRS, so every subdir under it becomes an esp-idf
    # component). Convention over configuration: an app just drops components
    # under <app_root>/esp_components (esp-prefixed since this is ESP32-only —
    # esp-idf's own components/ dir doesn't reach the app, as the idf "project"
    # is tuya_open_sdk, not the app). We merge rather than overwrite so a
    # caller/CI can pre-export extra roots. Gated on ESP32; joined with spaces
    # to match the receiver's split.
    if platform_name == "ESP32":
        comp_roots = []
        existing = os.environ.get("TUYAOS_EXTRA_COMPONENT_DIRS", "").strip()
        if existing:
            comp_roots.extend(existing.split())
        conv_dir = os.path.join(app_root, "esp_components")
        if os.path.isdir(conv_dir) and conv_dir not in comp_roots:
            comp_roots.append(conv_dir)
        if comp_roots:
            os.environ["TUYAOS_EXTRA_COMPONENT_DIRS"] = " ".join(comp_roots)
    defines = [
        f"-DTOS_PROJECT_NAME={project_name}",
        f"-DTOS_PROJECT_ROOT={app_root}",
        f"-DTOS_PROJECT_PLATFORM={platform_name}",
        f"-DTOS_FRAMEWORK={framework}",
        f"-DTOS_PROJECT_CHIP={chip_name}",
        f"-DTOS_PROJECT_BOARD={board_name}",
    ]

    cmd += defines

    build_path = params["app_build_path"]
    ret = do_subprocess(cmd, cwd=build_path)
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

    ret = do_subprocess(cmd, cwd=build_path)
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


# {app_name}_{QIO|UA|UG}_{app_ver}.bin -- every platform's
# build_example.py writes its final artifacts into .build/bin under this
# rule (see platform/*/build_example.py).
_ARTIFACT_RE = re.compile(r"^(?P<name>.+?)_(?P<kind>QIO|UA|UG)_(?P<ver>.+)\.bin$")


def _prune_stale_bins(bin_dir, app_name, app_ver):
    '''
    Drop artifacts left by an earlier build whose PROJECT_NAME or
    PROJECT_VERSION differed. Their file names carry the version, so a
    version bump creates NEW files instead of overwriting the old ones,
    and the copytree in check_bin_file would drag both versions into
    dist/<app>_<ver>/.

    Only top-level files matching <name>_<KIND>_<ver>.bin are touched;
    debug/ and any platform extras are left alone.

    os.remove rather than rm_rf: these are single files, and rm_rf
    shells out to `del /F /Q` on Windows for no benefit here.
    '''
    logger = get_logger()
    if not os.path.isdir(bin_dir):
        return []
    dropped = []
    for entry in sorted(os.listdir(bin_dir)):
        target = os.path.join(bin_dir, entry)
        if not os.path.isfile(target):
            continue
        matched = _ARTIFACT_RE.match(entry)
        if not matched:
            continue
        if (matched.group("name") == app_name
                and matched.group("ver") == app_ver):
            continue
        try:
            os.remove(target)
        except OSError as e:
            logger.warning(f"Cannot remove [{target}]: {e}")
            continue
        dropped.append(entry)
        logger.info(f"Dropped stale artifact: {entry}")
    return dropped


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

    # This build's artifacts are confirmed present; now drop what an
    # earlier PROJECT_NAME/PROJECT_VERSION left behind, or the copytree
    # below would ship several versions in one dist directory.
    _prune_stale_bins(app_bin_path, app_name, app_ver)

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


def build_project(verbose=False, log_file=None, log_file_append=False):
    """Perform a build; if log_file is provided, output will be redirected to that file. If log_file_append is True, append to the file."""

    def _run():
        logger = get_logger()
        check_proj_dir()

        # export.{sh,ps1,bat} is always the entry point and already prepares
        # the host tools; redo it here so a toolchain broken AFTER export
        # (antivirus removed cmake, an interrupted sync) or a shell that never
        # got export's PATH additions fails fast -- and self-heals -- instead
        # of surfacing much later, or not until the final packaging step.
        from tools.cli_command.cli_prepare import ensure_build_env
        if not ensure_build_env():
            logger.error("Build tools are missing or broken. "
                         "Re-run export to repair.")
            return False

        # Platforms are separate repositories that only receive argv and the
        # environment; hand them the region we just detected so they do not
        # probe for it themselves.
        export_country_code()

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

    if log_file:
        with redirect_stdout_stderr_to(log_file, append=log_file_append):
            return _run()
    return _run()


##
# @brief tos.py build
#
@click.command(help="Build the project.")
@click.option('-v', '--verbose',
              is_flag=True, default=False,
              help="Show verbose message.")
@click.option('-o', '--log-file',
              type=click.Path(),
              default=None,
              help="Write build log to the specified file instead of terminal.")
def cli(verbose, log_file):
    ok = build_project(verbose, log_file)
    sys.exit(0 if ok else 1)