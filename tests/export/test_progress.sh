#!/usr/bin/env bash
# Progress protocol parser tests for export.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIXTURES="$(cd "$(dirname "${BASH_SOURCE[0]}")/fixtures" && pwd)"
PASS=0
FAIL=0

pass() { PASS=$((PASS + 1)); printf '  PASS  %s\n' "$1"; }
fail() { FAIL=$((FAIL + 1)); printf '  FAIL  %s\n' "$1"; [ -n "${2:-}" ] && printf '        %s\n' "$2"; }

section() { printf '\n== %s ==\n' "$1"; }

TUYAOPEN_EXPORT_SKIP_MAIN=1
# shellcheck disable=SC1091
. "$ROOT/export.sh"

section 'Cold start kind'
kind=$(tuya_export_cold_start_kind)
case "$kind" in
    full|venv_only|warm) pass "cold start kind returns valid value ($kind)" ;;
    *) fail 'cold start kind returns valid value' "got '$kind'" ;;
esac

section 'IDE host gate'
unset TUYAOPEN_EXPORT_IDE
if tuya_is_ide_host; then
    fail 'tuya_is_ide_host false when unset'
else
    pass 'tuya_is_ide_host false when unset'
fi
TUYAOPEN_EXPORT_IDE=1
export TUYAOPEN_EXPORT_IDE
if tuya_is_ide_host; then
    pass 'tuya_is_ide_host true when set'
else
    fail 'tuya_is_ide_host true when set'
fi
unset TUYAOPEN_EXPORT_IDE

section 'Sync line parser'
_tuya_sync_pkg_total=10
_tuya_sync_current=0
_tuya_sync_last_name=''
_tuya_prog_last_text=''
_tuya_prog_last_at=0
_tuya_prog_last_pct=-1
out=$(
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        tuya_parse_uv_sync_line "$line" 10
    done <"$FIXTURES/uv_sync_lines.txt" 2>&1
)
if echo "$out" | grep -q 'Syncing dependencies'; then
    pass 'sync parser emits progress bar'
else
    fail 'sync parser emits progress bar' "$out"
fi
if echo "$out" | grep -q 'pydantic'; then
    pass 'sync parser includes package name'
else
    fail 'sync parser includes package name'
fi

section 'Python install line parser'
_tuya_py_artifact='cpython'
_tuya_py_total_mib=0
_tuya_py_recv_mib=-1
_tuya_prog_last_text=''
_tuya_prog_last_at=0
_tuya_prog_last_pct=-1
out=$(
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        tuya_parse_python_install_line "$line"
    done <"$FIXTURES/python_install_lines.txt" 2>&1
)
if echo "$out" | grep -q 'Installing Python'; then
    pass 'python parser emits Installing Python line'
else
    fail 'python parser emits Installing Python line' "$out"
fi
if echo "$out" | grep -qE '[0-9.]+\s*/\s*[0-9.]+\s*MB'; then
    pass 'python parser emits MB progress'
else
    fail 'python parser emits MB progress'
fi

section 'tuya_uv_run_stream exit code'

# Create a minimal fake uv that prints one line and exits non-zero
fake_uv_dir=$(mktemp -d)
fake_uv="$fake_uv_dir/uv"
cat >"$fake_uv" <<'FAKEUV'
#!/usr/bin/env sh
echo "fake output line"
exit 42
FAKEUV
chmod +x "$fake_uv"

stream_rc=0
stream_lines=''
_noop_line() { stream_lines="$stream_lines|$1"; }
_orig_uv="${OPEN_SDK_UV:-}"
OPEN_SDK_UV="$fake_uv"
tuya_uv_run_stream _noop_line version || stream_rc=$?
OPEN_SDK_UV="${_orig_uv:-}"
rm -rf "$fake_uv_dir"

if [ "$stream_rc" -eq 42 ]; then
    pass 'tuya_uv_run_stream propagates non-zero exit code'
else
    fail 'tuya_uv_run_stream propagates non-zero exit code' "got $stream_rc, expected 42"
fi
if echo "$stream_lines" | grep -q 'fake output line'; then
    pass 'tuya_uv_run_stream still calls on_line callback on failure'
else
    fail 'tuya_uv_run_stream calls on_line callback on failure'
fi

# Also verify success path returns 0
fake_uv_dir2=$(mktemp -d)
fake_uv2="$fake_uv_dir2/uv"
cat >"$fake_uv2" <<'FAKEUV2'
#!/usr/bin/env sh
echo "success line"
exit 0
FAKEUV2
chmod +x "$fake_uv2"
stream_rc2=1
_orig_uv2="${OPEN_SDK_UV:-}"
OPEN_SDK_UV="$fake_uv2"
tuya_uv_run_stream _noop_line version && stream_rc2=0 || stream_rc2=$?
OPEN_SDK_UV="${_orig_uv2:-}"
rm -rf "$fake_uv_dir2"
if [ "$stream_rc2" -eq 0 ]; then
    pass 'tuya_uv_run_stream returns 0 on success'
else
    fail 'tuya_uv_run_stream returns 0 on success' "got $stream_rc2"
fi

section 'Non-IDE smoke'
if declare -f tuya_uv >/dev/null 2>&1 && declare -f tuya_install_prompt >/dev/null 2>&1; then
    pass 'interactive helpers still defined'
else
    fail 'interactive helpers still defined'
fi

section 'Summary'
printf 'Pass: %d  Fail: %d\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
