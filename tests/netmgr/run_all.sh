#!/usr/bin/env bash
# Run all netmgr host unit tests.
set -uo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"
SRC_DIR="$ROOT/src/tuya_cloud_service/netmgr"
CC="${CC:-cc}"

FAIL=0
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

run() {
    local name="$1"
    shift
    printf '\n>> Running %s\n' "$name"
    if "$@"; then
        printf '>> %s: OK\n' "$name"
    else
        printf '>> %s: FAILED\n' "$name" >&2
        FAIL=1
    fi
}

# Compiles netmgr_retry.c straight from src/ against the minimal type shim in
# tests/netmgr/shim/ (see shim/tuya_cloud_types.h and README.md for why),
# links it with the test driver, and runs the resulting binary. No export.sh,
# no toolchain, no build tree -- a C compiler is the only dependency. The
# shim directory is put on the include path ahead of anywhere the real
# tuya_cloud_types.h lives, and nothing here adds the real one, so the real
# header is never reachable.
build_and_run_test_netmgr_retry() {
    "$CC" -std=c11 -Wall -Wextra -Werror \
        -I "$DIR/shim" \
        -I "$SRC_DIR/include" \
        -o "$TMPDIR/test_netmgr_retry" \
        "$DIR/test_netmgr_retry.c" \
        "$SRC_DIR/netmgr_retry.c" \
        || return 1
    "$TMPDIR/test_netmgr_retry"
}

run 'test_netmgr_retry.c' build_and_run_test_netmgr_retry

exit "$FAIL"
