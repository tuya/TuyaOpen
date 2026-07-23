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
from typing import Optional

import requests

from tools.cli_command.util import (
    get_logger, get_global_params,
    env_read, env_write, get_country_code,
)

CHECK_INTERVAL = 86400  # 24 hours
RELEASE_JSON_URL = (
    "https://github.com/tuya/tyutool/releases/latest/download/release.json"
)


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
        parts = v.lstrip('v').split('.')[:3]
        padded = (parts + ['0', '0', '0'])[:3]
        return tuple(int(x) for x in padded)
    except (ValueError, AttributeError):
        return (0, 0, 0)


def compare_versions(v1: str, v2: str) -> int:
    a, b = _parse(v1), _parse(v2)
    return (a > b) - (a < b)


def should_check_update() -> bool:
    last_check = env_read("tyutool_last_check", 0)
    return (time.time() - float(last_check)) >= CHECK_INTERVAL


def fetch_latest_json() -> Optional[dict]:
    logger = get_logger()
    try:
        resp = requests.get(RELEASE_JSON_URL, timeout=10)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        logger.debug(f"fetch_latest_json failed: {e}")
        return None


def get_local_version() -> Optional[str]:
    return env_read("tyutool_version", None)


def _detect_installed_version(tyutool_bin: str) -> str:
    """Run the binary to detect its version when env has no record."""
    try:
        out = subprocess.check_output(
            [tyutool_bin, "--version"],
            stderr=subprocess.STDOUT,
            timeout=5,
        ).decode().strip()
        # expected output: "tyutool X.Y.Z"
        parts = out.split()
        return parts[-1] if parts else ""
    except Exception:
        return ""


def _select_download_urls(cli_info: dict) -> list:
    """Order download URLs by region.

    release.json entries carry explicit `url_tuya` (mainland China CDN) and
    `url_github` fields; mainland China prefers `url_tuya`, elsewhere prefers
    `url_github`, each falling back to the other source.
    """
    url_github = cli_info.get("url_github") or cli_info.get("url", "")
    url_tuya = cli_info.get("url_tuya") or cli_info.get("url", "")

    if "China" in get_country_code():
        urls = [url_tuya, url_github]
    else:
        urls = [url_github, url_tuya]
    # drop empties and duplicates, keep order
    return list(dict.fromkeys(u for u in urls if u))


DOWNLOAD_TIMEOUT = 300  # 5 minutes total cap for binary download


def _format_download_progress(downloaded: int, total: int) -> str:
    """Human-readable download progress (KB when total < 1 MiB)."""
    if total > 0:
        pct = min(100, downloaded * 100 // total)
        if total >= 1024 * 1024:
            d_mb = downloaded / (1024 * 1024)
            t_mb = total / (1024 * 1024)
            return f"  {d_mb:.1f}MB / {t_mb:.1f}MB ({pct}%)"
        if total >= 1024:
            return f"  {downloaded // 1024}KB / {total // 1024}KB ({pct}%)"
        return f"  {downloaded}B / {total}B ({pct}%)"
    if downloaded >= 1024 * 1024:
        return f"  {downloaded / (1024 * 1024):.1f}MB downloaded"
    if downloaded >= 1024:
        return f"  {downloaded // 1024}KB downloaded"
    if downloaded > 0:
        return f"  {downloaded}B downloaded"
    return ""


def _download_file(url: str, dest: str, *, log_progress: bool = True) -> bool:
    logger = get_logger()
    try:
        resp = requests.get(url, stream=True, timeout=(15, 30))
        resp.raise_for_status()
        total = int(resp.headers.get("content-length") or 0)
        downloaded = 0
        deadline = time.time() + DOWNLOAD_TIMEOUT
        last_logged_pct = -1
        last_logged_kb_bucket = -1
        with open(dest, "wb") as f:
            for chunk in resp.iter_content(chunk_size=1024 * 1024):
                if chunk:
                    f.write(chunk)
                    downloaded += len(chunk)
                    if log_progress:
                        if total > 0:
                            pct = min(100, downloaded * 100 // total)
                            if pct >= last_logged_pct + 10 or pct == 100:
                                msg = _format_download_progress(
                                    downloaded, total)
                                if msg:
                                    logger.info(msg)
                                last_logged_pct = pct
                        else:
                            kb_bucket = downloaded // (256 * 1024)
                            if kb_bucket > last_logged_kb_bucket:
                                msg = _format_download_progress(downloaded, 0)
                                if msg:
                                    logger.info(msg)
                                last_logged_kb_bucket = kb_bucket
                if time.time() > deadline:
                    raise TimeoutError(
                        f"Download exceeded {DOWNLOAD_TIMEOUT}s limit"
                    )
        if log_progress and downloaded > 0 and (
                total == 0 or last_logged_pct < 100):
            msg = _format_download_progress(
                downloaded, total if total > 0 else downloaded)
            if msg:
                logger.info(msg)
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

    expected_sha256 = cli_info["sha256"]
    urls = _select_download_urls(cli_info)
    if not urls:
        logger.error(f"No download URL available for platform: {platform_key}")
        return False
    asset_name = urls[0].split('/')[-1]
    bin_name = "tyutool_cli.exe" if sys.platform == "win32" else "tyutool_cli"

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


def ensure_tyutool() -> Optional[str]:
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
        env_write("tyutool_last_check", time.time())
        return tyutool_bin

    latest_ver = latest_data.get("version", "")
    local_ver = _detect_installed_version(tyutool_bin) or get_local_version() or ""
    if compare_versions(latest_ver, local_ver) > 0:
        prompt_update(local_ver, latest_ver, latest_data)

    env_write("tyutool_last_check", time.time())
    return tyutool_bin
