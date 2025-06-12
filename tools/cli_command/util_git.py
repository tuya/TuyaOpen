#!/usr/bin/env python3
# coding=utf-8


import os
from git import Repo, Git
from git.exc import GitCommandError
from git import RemoteProgress

from tools.cli_command.util import get_logger


MIRROR_LIST = [
    "armink/FlashDB",
    "littlefs-project/littlefs",
    "DaveGamble/cJSON",
    "FreeRTOS/backoffAlgorithm",
    "fukuchi/libqrencode",
    "tuya/TuyaOpen-T2",
    "tuya/TuyaOpen-T3",
    "tuya/TuyaOpen-ubuntu",
    "tuya/TuyaOpen-T5AI",
    "tuya/TuyaOpen-esp32",
    "tuya/TuyaOpen-ln882h",
    "tuya/TuyaOpen-bk7231x",
    "tuya/arduino-tuyaopen",
]


def set_repo_mirro(unset=False):
    logger = get_logger()
    g = Git()
    for target in MIRROR_LIST:
        repo_name = target.split('/')[1]
        gitee = f"https://gitee.com/tuya-open/{repo_name}"
        github = f"https://github.com/{target}"
        try:
            if unset:
                g.config(
                    "--global",
                    "--unset",
                    f"url.{gitee}.insteadOf")
                g.config(
                    "--global",
                    "--unset",
                    f"url.{gitee}.git.insteadOf")
            else:
                g.config(
                    "--global",
                    f"url.{gitee}.insteadOf",
                    f"{github}")
                g.config(
                    "--global",
                    f"url.{gitee}.git.insteadOf",
                    f"{github}")
        except Exception as e:
            logger.warning(f"Set repo mirro error: {e}")
    pass


def get_git_tag_describe(repo_path):
    cmd = f"git -C {repo_path} describe --tags"
    try:
        repo = Repo(repo_path)
        describe_output = repo.git.describe('--tags')
        return describe_output
    except Exception as e:
        get_logger().error(f"[{cmd}]: {e}")
        return ""


class GitProgress(RemoteProgress):
    last_len = 0

    def show(self, msg):
        space = self.last_len - len(msg)
        self.last_len = len(msg) + 1
        print(f"\r{msg}{' '*space}", end="")

    def update(self, op_code, cur_count, max_count=None, message=''):
        if message:
            self.show(f"{cur_count}/{max_count or '?'} - {message}")


def git_clone(repo_url, target_path):
    logger = get_logger()
    if not repo_url or not target_path:
        logger.error(f"Git clone parameters error:\n\
repo_url: {repo_url}\n\
target_path: {target_path}")
        return False

    if os.path.exists(target_path):
        logger.debug(f"git clone path is exists [{target_path}]")
        return True

    logger.info(f"Git clone {repo_url} {target_path}")
    os.makedirs(os.path.dirname(target_path), exist_ok=True)
    try:
        Repo.clone_from(repo_url, target_path, progress=GitProgress())
        print("Success.")
        return True
    except Exception as e:
        logger.error(f"Git clone error: {str(e)}")
        return False


def git_pull(repo_path, remote_name='origin', branch=""):
    logger = get_logger()
    if not repo_path or not remote_name:
        logger.error(f"Git pull parameters error:\n\
repo_path: {repo_path}\n\
remote_name: {remote_name}")
        return False

    if not os.path.exists(repo_path):
        logger.error(f"Not found [{repo_path}]")
        return False

    repo = Repo(repo_path)
    if repo.bare:
        logger.error(f"[{repo_path}] is bare repository.")
        return False

    try:
        remote = repo.remote(name=remote_name)
    except ValueError:
        logger.error(f"Remote not found [{remote_name}]")
        return False

    try:
        current_branch = repo.active_branch
        pull_branch = branch if branch else current_branch.name
        logger.info(f"Git pull {remote_name} {pull_branch}")
        remote.pull(refspec=pull_branch, progress=GitProgress())
        logger.info(f"HEAD: {repo.head.commit.hexsha[:7]}...")
        return True
    except Exception as e:
        logger.error(f"Git pull error: {str(e)}")
        return False


def git_checkout(repo_path, target):
    logger = get_logger()
    if not repo_path or not target:
        logger.error(f"Git checkout parameters error:\n\
repo_path: {repo_path}\n\
target: {target}")
        return False

    if not os.path.exists(repo_path):
        logger.error(f"Not found [{repo_path}]")
        return False

    repo = Repo(repo_path)
    if repo.bare:
        logger.error(f"[{repo_path}] is bare repository.")
        return False

    try:
        repo.git.checkout(target)
        logger.info(f"Git checkout {target}.")
        return True
    except Exception as e:
        logger.error(f"Git checkout erorr: {str(e)}.")
        return False
        

def git_get_commit(repo_path):
    logger = get_logger()
    repo = Repo(repo_path)
    if repo.bare:
        logger.error(f"[{repo_path}] is bare repository.")
        return ""

    return repo.head.commit.hexsha


def download_submoudules(repo_path):
    logger = get_logger()
    repo = Repo(repo_path)
    if repo.bare:
        logger.warning(f"[{repo_path}] is bare repository.")
        return True

    gitmodules_path = os.path.join(repo_path, ".gitmodules")
    if not os.path.exists(gitmodules_path):
        logger.warning(f"Not found [{gitmodules_path}].")
        return True

    submodules = repo.submodules
    if not submodules:
        logger.warning("Not found submodules.")
        return True

    ret = True
    logger.info("Downloading submoudules ...")
    for submodule in submodules:
        try:
            logger.debug(f">>>{submodule.name}")
            submodule.update(init=True, recursive=True, progress=None)
        except GitCommandError as e:
            logger.error(f"Download [{submodule.name}]: {e}")
            ret = False
            continue
    if ret:
        logger.info("Download submoudules successfully.")
    return ret
