#!/usr/bin/env python3
# coding=utf-8

import os
import re
import sys
import shutil
import zipfile
import subprocess
from typing import Callable, List, Optional, Tuple

import click

from tools.cli_command.util import (
    get_logger, get_global_params, env_write, get_country_code,
)
from tools.cli_command.util_tyutool import _download_file

MAKE_TOOL_NAME = "make"
MAKE_VERSION = "4.4.1"
MAKE_MIN_VERSION = "3.0.0"
MAKE_ARCHIVE_FILENAME = "make-4.4.1-without-guile-w32-bin.zip"
MAKE_DOWNLOAD_URL = (
    "https://images.tuyacn.com/rms-static/"
    "f7061a90-5b36-11f1-8d53-258e63d3fe0e-1780042705593.zip"
    f"?tyName={MAKE_ARCHIVE_FILENAME}"
)

PrepareStep = Callable[[], bool]


def get_make_tool_install_dir(open_root: Optional[str] = None) -> str:
    """Return .tools/<tool_name>/<tool_version> install directory."""
    params = get_global_params()
    bin_dir = params.get("win_make_bin_dir")
    if bin_dir:
        return bin_dir
    if open_root is None:
        open_root = params["open_root"]
    return os.path.join(open_root, ".tools", MAKE_TOOL_NAME, MAKE_VERSION)


def get_make_tool_archive_path(open_root: Optional[str] = None) -> str:
    """Return .tools/archives/<tool_name>/<tool_version>/<archive_file>."""
    if open_root is None:
        open_root = get_global_params()["open_root"]
    return os.path.join(
        open_root, ".tools", "archives",
        MAKE_TOOL_NAME, MAKE_VERSION, MAKE_ARCHIVE_FILENAME,
    )


def get_windows_make_bin_dir() -> str:
    """Alias for the make install dir (historical name: bin_dir)."""
    return get_make_tool_install_dir()


def _prepend_to_path(directory: str) -> None:
    """Put an existing directory first on PATH, at most once."""
    if not os.path.isdir(directory):
        return
    path = os.environ.get("PATH", "")
    parts = path.split(os.pathsep)
    target = os.path.normcase(directory)
    if all(os.path.normcase(p) != target for p in parts):
        os.environ["PATH"] = directory + os.pathsep + path


def prepend_windows_make_to_path() -> None:
    _prepend_to_path(get_windows_make_bin_dir())


def prepend_venv_scripts_to_path() -> None:
    """Put the running interpreter's Scripts/bin dir first on PATH.

    Grandchildren resolve tools by name -- armino's makefiles call a bare
    ``python3``, and cmake/ninja are venv-provided -- so the venv has to be
    on PATH even when the caller never sourced export.
    """
    _prepend_to_path(os.path.dirname(os.path.abspath(sys.executable)))


def _find_version_string(version: str) -> str:
    pattern = r"\d+\.\d+(\.\d+)?"
    match = re.search(pattern, version)
    if not match:
        return ""
    return match.group()


def _parse_version_string(version: str) -> Optional[List[int]]:
    num_list = version.split(".")
    try:
        return [int(num) for num in num_list]
    except ValueError:
        return None


def _compare_version(actual: str, required: str) -> bool:
    actual_ver = _parse_version_string(actual)
    required_ver = _parse_version_string(required)
    if (actual_ver is None) or (required_ver is None):
        return False
    for a, r in zip(actual_ver, required_ver):
        if a > r:
            return True
        if a < r:
            return False
    return True


def _make_version_ok(make_cmd: str) -> bool:
    try:
        result = subprocess.run(
            [make_cmd, "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        tool_ver = _find_version_string(result.stdout + result.stderr)
        if tool_ver == "":
            return False
        return _compare_version(tool_ver, MAKE_MIN_VERSION)
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return False


def _find_make_exe_in_tree(root: str) -> Optional[str]:
    for dirpath, _, filenames in os.walk(root):
        if "make.exe" in filenames:
            return os.path.join(dirpath, "make.exe")
    return None


def _format_byte_size(num_bytes: int) -> str:
    if num_bytes >= 1024 * 1024:
        return f"{num_bytes / (1024 * 1024):.1f}MB"
    if num_bytes >= 1024:
        return f"{num_bytes // 1024}KB"
    return f"{num_bytes}B"


def _copy_tool_files(src_dir: str, dest_dir: str) -> None:
    os.makedirs(dest_dir, exist_ok=True)
    for name in os.listdir(src_dir):
        src = os.path.join(src_dir, name)
        dest = os.path.join(dest_dir, name)
        if os.path.isfile(src):
            shutil.copy2(src, dest)


def ensure_windows_make() -> bool:
    """
    @brief Ensure GNU Make on Windows under .tools/make/<version>/
    @return True on success
    """
    logger = get_logger()
    if sys.platform != "win32":
        return True

    params = get_global_params()
    open_root = params["open_root"]
    install_dir = get_make_tool_install_dir(open_root)
    make_exe = os.path.join(install_dir, "make.exe")

    if os.path.isfile(make_exe) and _make_version_ok(make_exe):
        logger.debug(f"[prepare] make already installed: {make_exe}")
        prepend_windows_make_to_path()
        return True

    which_make = shutil.which("make")
    if which_make and _make_version_ok(which_make):
        logger.debug(f"[prepare] using make from PATH: {which_make}")
        return True

    venv_path = os.path.join(open_root, ".venv")
    if not os.path.isdir(venv_path):
        logger.error(
            "[prepare] .venv not found. Source export.ps1 / export.sh first."
        )
        return False

    archive_path = get_make_tool_archive_path(open_root)
    archive_dir = os.path.dirname(archive_path)
    tmp_dir = os.path.join(install_dir, ".tmp")
    extract_dir = os.path.join(tmp_dir, "extract")

    try:
        os.makedirs(archive_dir, exist_ok=True)
        if not os.path.isfile(archive_path):
            logger.info(
                f"[prepare] Downloading GNU Make {MAKE_VERSION} for Windows..."
            )
            if not _download_file(
                    MAKE_DOWNLOAD_URL, archive_path, log_progress=False):
                logger.error("[prepare] Failed to download GNU Make.")
                return False
            logger.info(
                "[prepare] Download complete "
                f"({_format_byte_size(os.path.getsize(archive_path))})."
            )
        else:
            logger.debug(f"[prepare] Reusing archive: {archive_path}")

        if os.path.exists(extract_dir):
            shutil.rmtree(extract_dir)
        os.makedirs(extract_dir, exist_ok=True)

        with zipfile.ZipFile(archive_path, "r") as zf:
            try:
                zf.extractall(extract_dir, filter="data")
            except TypeError:
                zf.extractall(extract_dir)
        found = _find_make_exe_in_tree(extract_dir)
        if not found:
            logger.error("[prepare] make.exe not found in downloaded archive.")
            return False

        src_bin = os.path.dirname(found)
        _copy_tool_files(src_bin, install_dir)

        if not os.path.isfile(make_exe):
            logger.error("[prepare] make.exe missing after install.")
            return False

        if not _make_version_ok(make_exe):
            logger.error("[prepare] Installed make failed version check.")
            return False

        env_write("make_version", MAKE_VERSION)
        logger.info(
            f"[prepare] GNU Make {MAKE_VERSION} extracted and installed to "
            f"{install_dir}"
        )
        prepend_windows_make_to_path()
        return True
    finally:
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)


def download_host_tools() -> bool:
    if sys.platform == "win32":
        return ensure_windows_make()
    return True


def _tool_runs_ok(exe: str) -> bool:
    """Run ``<exe> --version``; True only if it launches and exits 0.

    List form + shell=False keeps paths with spaces / non-ASCII / special
    characters safe. A present-but-broken tool (e.g. a venv launcher whose
    payload was removed by antivirus, or an incomplete install) exits non-zero
    and is thus reported as broken, not merely "missing".
    """
    try:
        subprocess.run(
            [exe, "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return False


def resolve_venv_tool(binary: str) -> Optional[str]:
    """Absolute path to a venv-provided tool (cmake/ninja), or None.

    Resolved from the running interpreter's Scripts/bin dir (so it does not
    depend on the caller's PATH being activated). ``shutil.which`` applies
    PATHEXT, so it returns the real ``.exe`` on Windows.
    """
    scripts = os.path.dirname(os.path.abspath(sys.executable))
    return shutil.which(binary, path=scripts)


def ensure_venv_tool(pkg: str, binary: str) -> Tuple[bool, Optional[str]]:
    """Ensure a venv-managed tool exists and actually runs; repair if not.

    Returns ``(ok, exe)``. When the tool is missing or broken, reinstall just
    that package via uv. ``--reinstall-package`` is required because uv treats
    an intact dist-info as "already installed" and would otherwise not restore
    files that were deleted after install. Needs OPEN_SDK_UV and a .venv.
    """
    logger = get_logger()
    exe = resolve_venv_tool(binary)
    if exe and _tool_runs_ok(exe):
        return True, exe

    uv = os.environ.get("OPEN_SDK_UV") or shutil.which("uv")
    open_root = get_global_params().get("open_root")
    if uv and open_root and os.path.isdir(os.path.join(open_root, ".venv")):
        logger.warning(
            f"[{binary}] dependency missing or broken; reinstalling {pkg} ...")
        cmd = [uv, "sync", "--frozen", "--reinstall-package", pkg]
        if get_country_code() == "China":
            cmd += ["--default-index",
                    "https://mirrors.aliyun.com/pypi/simple/"]
        try:
            subprocess.run(cmd, cwd=open_root)
        except OSError as e:
            logger.error(f"[{binary}] reinstall failed to start: {e}")
        exe = resolve_venv_tool(binary)
        if exe and _tool_runs_ok(exe):
            logger.note(f"[{binary}] repaired.")
            return True, exe

    return False, exe


def ensure_build_tools() -> bool:
    """Verify (and repair) the venv-managed build tools: cmake + ninja."""
    logger = get_logger()
    ok = True
    for pkg, binary in (("cmake", "cmake"), ("ninja", "ninja")):
        good, _ = ensure_venv_tool(pkg, binary)
        if not good:
            logger.error(
                f"[{binary}] could not be prepared. Re-run export, "
                f"or: uv sync --reinstall-package {pkg}"
            )
            ok = False
    return ok


def ensure_build_env() -> bool:
    """Prepare the environment a build's child processes will inherit.

    ``build`` used to verify cmake/ninja only, leaving the bundled GNU Make
    off PATH unless export had been sourced: a T5AI build then compiled
    every source and died with "make: command not found" in the single make
    invocation that packages it, on the last ninja edge.
    """
    if not download_host_tools():
        get_logger().error("[prepare] host tools unavailable.")
        return False
    if not ensure_build_tools():
        return False
    prepend_venv_scripts_to_path()
    return True


PREPARE_STEPS: List[Tuple[str, PrepareStep]] = [
    ("download_host_tools", download_host_tools),
    ("ensure_build_tools", ensure_build_tools),
]


def run_prepare() -> bool:
    for _name, step in PREPARE_STEPS:
        if not step():
            get_logger().error(f"[prepare] {_name} failed.")
            return False
    return True


@click.command(
    help="Prepare SDK host tools (.tools/<name>/<version>). Platform steps may no-op.",
    context_settings=dict(help_option_names=["-h", "--help"]),
)
def cli():
    if not run_prepare():
        sys.exit(1)


if __name__ == "__main__":
    cli()
