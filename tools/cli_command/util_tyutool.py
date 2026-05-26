#!/usr/bin/env python3
# coding=utf-8

import os
import sys
import time
import shutil
import hashlib
import tarfile
import zipfile
import platform
import subprocess

import requests

from tools.cli_command.util import (
    get_logger, get_global_params,
    env_read, env_write, get_country_code,
)

CHECK_INTERVAL = 86400  # 24 hours
LATEST_JSON_URL = (
    "https://github.com/tuya/tyutool/releases/latest/download/latest.json"
)
_GITHUB_PREFIX = "https://github.com/tuya/tyutool"
_GITEE_PREFIX = "https://gitee.com/tuya-open/tyutool"


def get_platform_key() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()

    if machine in ('x86_64', 'amd64'):
        arch = 'x86_64'
    elif machine in ('aarch64', 'arm64'):
        arch = 'aarch64'
    else:
        raise RuntimeError(f"Unsupported architecture: {machine}")

    if 'linux' in system:
        os_name = 'linux'
    elif 'darwin' in system:
        os_name = 'darwin'
    elif 'windows' in system:
        os_name = 'windows'
        arch = 'x86_64'
    else:
        raise RuntimeError(f"Unsupported system: {system}")

    return f"{os_name}-{arch}"


def _parse(v: str) -> tuple:
    try:
        return tuple(int(x) for x in v.lstrip('v').split('.')[:3])
    except (ValueError, AttributeError):
        return (0, 0, 0)


def compare_versions(v1: str, v2: str) -> int:
    a, b = _parse(v1), _parse(v2)
    return (a > b) - (a < b)


def should_check_update() -> bool:
    last_check = env_read("tyutool_last_check", 0)
    return (time.time() - float(last_check)) >= CHECK_INTERVAL


def fetch_latest_json() -> dict | None:
    logger = get_logger()
    try:
        resp = requests.get(LATEST_JSON_URL, timeout=10)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        logger.debug(f"fetch_latest_json failed: {e}")
        return None


def get_local_version() -> str | None:
    return env_read("tyutool_version", None)


def _make_gitee_url(github_url: str) -> str:
    return github_url.replace(_GITHUB_PREFIX, _GITEE_PREFIX)


def _download_file(url: str, dest: str) -> bool:
    logger = get_logger()
    try:
        resp = requests.get(url, stream=True, timeout=60)
        resp.raise_for_status()
        total = int(resp.headers.get('content-length', 0))
        downloaded = 0
        with open(dest, 'wb') as f:
            for chunk in resp.iter_content(chunk_size=1024 * 1024):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    mb = downloaded // (1024 * 1024)
                    if total:
                        total_mb = total // (1024 * 1024)
                        logger.info(f"  {mb}MB / {total_mb}MB")
                    else:
                        logger.info(f"  {mb}MB downloaded")
        return True
    except Exception as e:
        logger.debug(f"Download failed ({url}): {e}")
        return False


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def download_tyutool_bin(latest_data: dict) -> bool:
    logger = get_logger()
    params = get_global_params()
    tyutool_bin_dir = params["tyutool_bin_dir"]
    tyutool_bin = params["tyutool_bin"]

    platform_key = get_platform_key()
    cli_info = latest_data.get("cli", {}).get(platform_key)
    if not cli_info:
        logger.error(f"No CLI binary available for platform: {platform_key}")
        return False

    github_url = cli_info["url"]
    expected_sha256 = cli_info["sha256"]
    asset_name = github_url.split('/')[-1]
    bin_name = "tyutool_cli.exe" if sys.platform == "win32" else "tyutool_cli"

    gitee_url = _make_gitee_url(github_url)
    if "China" in get_country_code():
        urls = [gitee_url, github_url]
    else:
        urls = [github_url, gitee_url]

    tmp_dir = os.path.join(tyutool_bin_dir, ".tmp")
    extract_dir = os.path.join(tyutool_bin_dir, ".tmp_extract")
    os.makedirs(tmp_dir, exist_ok=True)
    archive_path = os.path.join(tmp_dir, asset_name)

    try:
        logger.info(
            f"Downloading tyutool {latest_data['version']} for {platform_key} ..."
        )
        downloaded = False
        for url in urls:
            logger.info(f"  Trying: {url}")
            if _download_file(url, archive_path):
                downloaded = True
                break
        if not downloaded:
            logger.error("Failed to download tyutool from all sources.")
            return False

        actual = _sha256_file(archive_path)
        if actual != expected_sha256:
            logger.error(f"SHA256 mismatch: expected {expected_sha256}, got {actual}")
            return False

        logger.info("Download complete, extracting ...")
        if os.path.exists(extract_dir):
            shutil.rmtree(extract_dir)
        os.makedirs(extract_dir)

        if asset_name.endswith('.tar.gz'):
            with tarfile.open(archive_path, 'r:gz') as tf:
                try:
                    tf.extractall(extract_dir, filter='data')
                except TypeError:
                    tf.extractall(extract_dir)
        elif asset_name.endswith('.zip'):
            with zipfile.ZipFile(archive_path, 'r') as zf:
                zf.extractall(extract_dir)

        extracted_bin = os.path.join(extract_dir, bin_name)
        if not os.path.isfile(extracted_bin):
            logger.error(f"Binary '{bin_name}' not found after extraction.")
            return False

        if sys.platform != "win32":
            os.chmod(extracted_bin, 0o755)
        if sys.platform == "darwin":
            subprocess.run(
                ["xattr", "-d", "com.apple.quarantine", extracted_bin],
                capture_output=True,
            )

        os.makedirs(tyutool_bin_dir, exist_ok=True)
        shutil.move(extracted_bin, tyutool_bin)
        env_write("tyutool_version", latest_data["version"])
        logger.info(f"tyutool {latest_data['version']} installed successfully.")
        return True

    finally:
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir)
        if os.path.exists(extract_dir):
            shutil.rmtree(extract_dir)


def prompt_update(local_ver: str, latest_ver: str, latest_data: dict) -> bool:
    logger = get_logger()
    if not sys.stdin.isatty():
        logger.info(
            f"New tyutool {latest_ver} available (current: {local_ver}), "
            "skipping prompt (non-interactive)."
        )
        return True
    logger.warning(
        f"New tyutool version {latest_ver} is available (current: {local_ver})"
    )
    while True:
        logger.note("Update now? y(es) / n(o)")
        try:
            ret = input("input: ").strip().upper()
        except (EOFError, KeyboardInterrupt):
            return True
        if ret == "Y":
            if not download_tyutool_bin(latest_data):
                logger.warning("Update failed, continuing with current version.")
            return True
        elif ret == "N":
            return True


def ensure_tyutool() -> str | None:
    logger = get_logger()
    params = get_global_params()
    tyutool_bin = params["tyutool_bin"]
    open_root = params["open_root"]

    old_dir = os.path.join(open_root, "tools", "tyutool")
    if os.path.exists(os.path.join(old_dir, "tyutool_cli.py")):
        logger.warning("Detected outdated tyutool package, removing tools/tyutool/ ...")
        try:
            shutil.rmtree(old_dir)
            logger.info("Removed tools/tyutool/ successfully.")
        except Exception as e:
            logger.debug(f"Failed to remove old tyutool: {e}")

    if not os.path.isfile(tyutool_bin):
        latest_data = fetch_latest_json()
        if not latest_data:
            logger.error(
                "Cannot fetch tyutool version info. Please check your network."
            )
            return None
        if not download_tyutool_bin(latest_data):
            return None
        env_write("tyutool_last_check", time.time())
        return tyutool_bin

    if not should_check_update():
        return tyutool_bin

    latest_data = fetch_latest_json()
    if latest_data is None:
        return tyutool_bin

    latest_ver = latest_data.get("version", "")
    local_ver = get_local_version() or ""
    if compare_versions(latest_ver, local_ver) > 0:
        prompt_update(local_ver, latest_ver, latest_data)

    env_write("tyutool_last_check", time.time())
    return tyutool_bin
