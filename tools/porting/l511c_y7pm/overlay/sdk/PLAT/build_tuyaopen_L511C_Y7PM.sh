#!/bin/bash
set -e

PLATFORM_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$PLATFORM_ROOT/sdk/PLAT"

export PROJECT_NAME=tuyaos_adapter
export AUDIO_ENABLE=true
export GCCLIB_PATH="$PLATFORM_ROOT/toolchain/gcc-arm-none-eabi-10-2020-q4-major"

if [ "${1:-}" = clean ]; then
    rm -rf gccout L511C_Y7PM_download
    exit 0
fi
if [ "$#" -ne 0 ]; then
    echo "usage: $0 [clean]" >&2
    exit 2
fi

exec bash GccBuild_L511C_Y7PM.sh
