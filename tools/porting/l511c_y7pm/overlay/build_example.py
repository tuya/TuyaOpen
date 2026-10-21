#!/usr/bin/env python3
"""Build the TuyaOpen libraries with the L511C-Y7PM vendor target."""

import errno
import json
import os
import shutil
import subprocess
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))
PLAT = os.path.join(ROOT, "sdk", "PLAT")
TOOLCHAIN = os.path.join(ROOT, "toolchain", "gcc-arm-none-eabi-10-2020-q4-major")
BUILD_SCRIPT = os.path.join(ROOT, "sdk", "PLAT", "build_tuyaopen_L511C_Y7PM.sh")
VENDOR_BUILD_SCRIPT = os.path.join(PLAT, "build_c_pm.sh")
DOWNLOAD_DIR = os.path.join(PLAT, "L511C_Y7PM_download")
PACKAGE = os.path.join(DOWNLOAD_DIR, "L511C_Y7PM.binpkg")


def read_params(build_param_dir):
    path = os.path.join(build_param_dir, "build_param.json")
    with open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def build_env(param, build_param_dir):
    env = os.environ.copy()
    env["GCCLIB_PATH"] = TOOLCHAIN
    env["PATH"] = os.pathsep.join((os.path.join(TOOLCHAIN, "bin"), env.get("PATH", "")))
    env["TUYA_LIB_DIR"] = param.get("OPEN_LIBS_DIR", "")
    env["TUYA_LIBS"] = param.get("PLATFORM_NEED_LIBS", "")
    env["TUYA_APP_KCONFIG_DIR"] = os.path.normpath(os.path.join(build_param_dir, "..", "include"))
    return env


def run(build_param_dir, action):
    if action not in ("build", "clean"):
        raise SystemExit(f"unsupported action: {action}")
    param = read_params(build_param_dir)
    if action == "build" and not os.path.isfile(os.path.join(PLAT, "tools", "fcelf")):
        raise SystemExit("L511C_Y7PM vendor packager sdk/PLAT/tools/fcelf is missing")
    if action == "build" and not os.path.isfile(VENDOR_BUILD_SCRIPT):
        raise SystemExit(f"L511C_Y7PM vendor build script is missing: {VENDOR_BUILD_SCRIPT}")

    if action == "build" and os.path.isfile(PACKAGE):
        os.remove(PACKAGE)
    command = ["bash", BUILD_SCRIPT]
    if action == "clean":
        command.append("clean")
    print(f"Run {' '.join(command)}")
    result = subprocess.run(command, cwd=PLAT, env=build_env(param, build_param_dir))
    if action == "clean":
        raise SystemExit(result.returncode)
    if not os.path.isfile(PACKAGE):
        raise SystemExit(f"L511C_Y7PM vendor build failed (exit {result.returncode}); missing {PACKAGE}")

    app_name = param.get("CONFIG_PROJECT_NAME", "")
    app_ver = param.get("CONFIG_PROJECT_VERSION", "")
    out_dir = param.get("BIN_OUTPUT_DIR", "")
    if not app_name or not app_ver or not out_dir:
        raise SystemExit("build parameters lack project name, version, or output directory")
    os.makedirs(out_dir, exist_ok=True)
    shutil.copytree(DOWNLOAD_DIR, os.path.join(out_dir, os.path.basename(DOWNLOAD_DIR)), dirs_exist_ok=True)
    for suffix in ("bin", "binpkg"):
        shutil.copy2(PACKAGE, os.path.join(out_dir, f"{app_name}_QIO_{app_ver}.{suffix}"))
    elf = os.path.join(DOWNLOAD_DIR, "ap_tuyaos_adapter.elf")
    if os.path.isfile(elf):
        shutil.copy2(elf, os.path.join(out_dir, f"{app_name}_{app_ver}.elf"))
    print(f"Output: {out_dir}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} <build_param_dir> <build|clean>")
    run(os.path.abspath(sys.argv[1]), sys.argv[2])


if __name__ == "__main__":
    main()
