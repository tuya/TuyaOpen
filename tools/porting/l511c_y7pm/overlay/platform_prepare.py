#!/usr/bin/env python3
"""Prepare the bundled L511C-Y7PM toolchain and vendor build scripts."""

import os
import stat
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))
TOOLCHAIN = os.path.join(ROOT, "toolchain", "gcc-arm-none-eabi-10-2020-q4-major")


def main():
    compiler = os.path.join(TOOLCHAIN, "bin", "arm-none-eabi-gcc")
    if not os.path.isfile(compiler):
        raise SystemExit(f"L511C_Y7PM compiler is missing: {compiler}")
    for base, _dirs, files in os.walk(os.path.join(ROOT, "sdk", "PLAT", "tools")):
        for name in files:
            path = os.path.join(base, name)
            try:
                with open(path, "rb") as stream:
                    is_elf = stream.read(4) == b"\x7fELF"
            except OSError:
                continue
            if is_elf:
                os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    for relative in (
        "sdk/PLAT/GccBuild_L511C_Y7PM.sh",
        "sdk/PLAT/build_c_pm.sh",
        "sdk/PLAT/output_download.sh",
    ):
        path = os.path.join(ROOT, relative)
        if os.path.isfile(path):
            os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    print(f"L511C_Y7PM toolchain ready: {TOOLCHAIN}")


if __name__ == "__main__":
    main()
