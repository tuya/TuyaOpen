#!/bin/sh
# Host tests for the P2P transport. No board and no cross toolchain needed:
# the congestion control is plain integer arithmetic over the ikcpcb, and the
# pacer is exercised against a simulated bottleneck, so both run natively and
# much faster than over a live link.
#
#   ./tests/p2p/run.sh
set -e

here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/tuya_p2p/base_ice
out=${TMPDIR:-/tmp}/tuya_p2p_tests
mkdir -p "$out"

cc=${CC:-gcc}
transport="$src/src/ikcp.c $src/src/ikcp_cong.c $src/src/ikcp_pacing.c $src/src/ikcp_minmax.c"

for t in test_ikcp_cong test_ikcp_pacing test_ikcp_drop; do
    # shellcheck disable=SC2086
    $cc -O1 -g -Wall -I"$src/src" -I"$src/include" \
        -o "$out/$t" "$here/$t.c" "$here/stubs.c" $transport
done

fail=0
for t in test_ikcp_cong test_ikcp_pacing test_ikcp_drop; do
    echo "--- $t"
    "$out/$t" || fail=1
done

exit $fail
