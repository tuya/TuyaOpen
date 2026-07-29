#!/bin/sh
# Host build + run of the power component unit tests (no hardware / SDK needed).
set -e
cd "$(dirname "$0")"
CC=${CC:-gcc}
OUT=$(mktemp -d)/test_power
"$CC" -std=gnu99 -g -O0 -Wall -Wno-unused-parameter -Wno-unused-function \
    -I stub \
    -I ../tdl_power/include \
    -I ../tdd_power/include \
    test_power_waveshare.c -o "$OUT"
"$OUT"
