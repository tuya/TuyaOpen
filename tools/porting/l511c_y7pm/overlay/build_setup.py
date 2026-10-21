#!/usr/bin/env python3
"""Validate and select the L511C-Y7PM vendor build inputs."""

import os
import sys


ROOT = os.path.dirname(os.path.abspath(__file__))
PLAT = os.path.join(ROOT, "sdk", "PLAT")
TOOLCHAIN = os.path.join(ROOT, "toolchain", "gcc-arm-none-eabi-10-2020-q4-major")


def main():
    required = (
        os.path.join(TOOLCHAIN, "bin", "arm-none-eabi-gcc"),
        os.path.join(PLAT, "GccBuild_L511C_Y7PM.sh"),
        os.path.join(PLAT, "openlib", "L511C_Y7PM"),
        os.path.join(PLAT, "middleware", "developed"),
    )
    missing = [path for path in required if not os.path.exists(path)]
    if missing:
        raise SystemExit("Missing L511C_Y7PM build inputs:\n  " + "\n  ".join(missing))

    config = os.path.join(PLAT, "middleware", "developed", "build_config.inc")
    contents = "\n".join((
        "MBTK_PRODUCT_BOARD=EC718P_Y7PM",
        "MBTK_PRODUCT_CHIP=L511C_Y7",
        "MBTK_PRODUCT_TARGET=L511C_Y7PM",
        "",
    ))
    with open(config, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(contents)
    print("L511C_Y7PM build setup complete")


if __name__ == "__main__":
    main()
