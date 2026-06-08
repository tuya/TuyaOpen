#!/usr/bin/env bash
# Run all export script tests.
set -uo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAIL=0

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

run 'test_export.sh' bash "$DIR/test_export.sh"
run 'test_progress.sh' bash "$DIR/test_progress.sh"

if command -v pwsh >/dev/null 2>&1; then
    run 'test_progress.ps1 (pwsh)' pwsh -NoProfile -File "$DIR/test_progress.ps1"
elif command -v powershell >/dev/null 2>&1; then
    run 'test_progress.ps1 (powershell)' powershell -NoProfile -File "$DIR/test_progress.ps1"
else
    printf '\n>> SKIP test_progress.ps1 (no pwsh/powershell)\n'
fi

exit "$FAIL"
