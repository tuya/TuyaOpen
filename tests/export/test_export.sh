#!/usr/bin/env bash
#
# Full test suite for export.sh
# Usage: bash tests/export/test_export.sh
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

PASS=0
FAIL=0
SKIP=0
TOTAL=0

pass() {
    PASS=$((PASS + 1))
    TOTAL=$((TOTAL + 1))
    printf '  PASS  %s\n' "$1"
}

fail() {
    FAIL=$((FAIL + 1))
    TOTAL=$((TOTAL + 1))
    printf '  FAIL  %s\n' "$1"
    [ -n "${2:-}" ] && printf '        %s\n' "$2"
}

skip() {
    SKIP=$((SKIP + 1))
    TOTAL=$((TOTAL + 1))
    printf '  SKIP  %s\n' "$1"
    [ -n "${2:-}" ] && printf '        %s\n' "$2"
}

section() {
    printf '\n== %s ==\n' "$1"
}

assert_eq() {
    local name="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        pass "$name"
    else
        fail "$name" "expected='$expected' actual='$actual'"
    fi
}

assert_ok() {
    local name="$1" rc="$2"
    if [ "$rc" -eq 0 ]; then
        pass "$name"
    else
        fail "$name" "exit code $rc"
    fi
}

assert_fail() {
    local name="$1" rc="$2"
    if [ "$rc" -ne 0 ]; then
        pass "$name"
    else
        fail "$name" "expected non-zero exit"
    fi
}

assert_contains() {
    local name="$1" haystack="$2" needle="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        pass "$name"
    else
        fail "$name" "output missing '$needle'"
    fi
}

# Load export.sh functions without running main or final cleanup.
load_export_functions() {
    TUYAOPEN_EXPORT_SKIP_MAIN=1 . "$ROOT/export.sh"
}

# ---------------------------------------------------------------------------
# 1. Static analysis
# ---------------------------------------------------------------------------
section 'Static analysis'

if bash -n "$ROOT/export.sh" 2>/dev/null; then
    pass 'bash -n syntax check'
else
    fail 'bash -n syntax check'
fi

if command -v shellcheck >/dev/null 2>&1; then
    if shellcheck -e SC1091,SC2034,SC2164,SC2181,SC2155,SC2269,SC2296,SC2317 "$ROOT/export.sh" 2>/dev/null; then
        pass 'shellcheck'
    else
        fail 'shellcheck' 'see shellcheck output above'
    fi
else
    skip 'shellcheck' 'shellcheck not installed'
fi

# ---------------------------------------------------------------------------
# 2. Load-only (TUYAOPEN_EXPORT_SKIP_MAIN=1)
# ---------------------------------------------------------------------------
section 'Load-only sourcing'

load_only_rc=0
(
    cd "$ROOT"
    TUYAOPEN_EXPORT_SKIP_MAIN=1
    # shellcheck disable=SC1091
    . "$ROOT/export.sh"
) || load_only_rc=$?
assert_ok 'SKIP_MAIN from repo root' "$load_only_rc"

load_only_subdir_rc=0
(
    cd "$ROOT/examples" 2>/dev/null || cd "$ROOT"
    TUYAOPEN_EXPORT_SKIP_MAIN=1
    # shellcheck disable=SC1091
    . "$ROOT/export.sh"
) || load_only_subdir_rc=$?
assert_ok 'SKIP_MAIN from subdirectory (script-dir root)' "$load_only_subdir_rc"

# Absolute-path source resolves root via script location, not cwd.
abs_source_rc=0
(
    cd /tmp
    TUYAOPEN_EXPORT_SKIP_MAIN=1
    # shellcheck disable=SC1091
    . "$ROOT/export.sh" >/dev/null 2>&1
) || abs_source_rc=$?
assert_ok 'SKIP_MAIN via absolute path from /tmp' "$abs_source_rc"

bad_root_rc=0
bad_root_out=""
bad_root_out=$(
    tmpdir=$(mktemp -d)
    cp "$ROOT/export.sh" "$tmpdir/export.sh"
    cd "$tmpdir"
    TUYAOPEN_EXPORT_SKIP_MAIN=1
    # shellcheck disable=SC1091
    . ./export.sh 2>&1
    rc=$?
    rm -rf "$tmpdir"
    exit "$rc"
) || bad_root_rc=$?
assert_fail 'isolated export.sh without project files fails' "$bad_root_rc"
assert_contains 'isolated export error message' "$bad_root_out" 'Unable to locate TuyaOpen project root'

# ---------------------------------------------------------------------------
# 3. Direct execution tip
# ---------------------------------------------------------------------------
section 'Direct execution'

direct_out=""
direct_out=$(bash "$ROOT/export.sh" 2>&1) || true
assert_contains 'direct run shows dot-source tip' "$direct_out" 'dot-source'

# ---------------------------------------------------------------------------
# 4. Unit logic (function behavior)
# ---------------------------------------------------------------------------
section 'Unit logic (function behavior)'

unit_out=""
unit_rc=0
unit_out=$(bash --norc --noprofile <<UNIT_EOF
set -e
ROOT='$ROOT'
cd "\$ROOT"

load_export_functions() {
    TUYAOPEN_EXPORT_SKIP_MAIN=1 . "\$ROOT/export.sh"
}
load_export_functions

if tuya_is_sdk_root "\$ROOT"; then echo OK_ROOT; else echo BAD_ROOT; fi
if tuya_is_sdk_root /tmp; then echo BAD_TMP; else echo OK_TMP; fi

key=\$(tuya_triple_manifest_key 'x86_64-unknown-linux-gnu')
echo "KEY=\$key"

tuya_load_uv_manifest "\$ROOT"
check=\$(tuya_get_uv_artifact_check 'x86_64-unknown-linux-gnu' || echo FAIL)
echo "CHECK=\$check"

TUYAOPEN_PYPI_MIRROR=1; echo "PLAN1=\$(tuya_uv_sync_plan)"
TUYAOPEN_PYPI_MIRROR=0; echo "PLAN0=\$(tuya_uv_sync_plan)"
unset TUYAOPEN_PYPI_MIRROR; echo "PLAND=\$(tuya_uv_sync_plan)"

cnt=\$(tuya_lock_pkg_count)
echo "PKG_CNT=\$cnt"

arch=\$(tuya_get_arch)
echo "ARCH=\$arch"
artifact=\$(tuya_select_uv_artifact "\$arch")
echo "ARTIFACT=\$artifact"

PATH=/usr/bin
tuya_path_add /test/bin
case ":\$PATH:" in *:/test/bin:*) echo PATH_ADD_OK ;; esac
tuya_path_remove /test/bin
case ":\$PATH:" in *:/test/bin:*) echo PATH_RM_BAD ;; *) echo PATH_RM_OK ;; esac

urls=\$(tuya_get_release_urls '0.11.18' | wc -l)
echo "URL_COUNT=\$urls"

marker=\$(tuya_is_uv_venv "\$ROOT/.venv" && echo MANAGED || echo LEGACY)
echo "VENV_STATE=\$marker"

echo "TZ_PARSE_0800=\$(tuya_parse_tz_offset_z +0800)"
echo "TZ_PARSE_0500=\$(tuya_parse_tz_offset_z -0500)"
echo "TZ_PARSE_COLON=\$(tuya_parse_tz_offset_z +08:00)"
live_tz=\$(tuya_get_utc_offset_minutes || echo FAIL)
echo "TZ_LIVE=\$live_tz"

tuya_is_in_cn_tz_range 450 && echo TZ450=1 || echo TZ450=0
tuya_is_in_cn_tz_range 480 && echo TZ480=1 || echo TZ480=0
tuya_is_in_cn_tz_range 510 && echo TZ510=1 || echo TZ510=0
tuya_is_in_cn_tz_range 449 && echo TZ449=1 || echo TZ449=0
tuya_is_in_cn_tz_range 511 && echo TZ511=1 || echo TZ511=0

cn_url=\$(tuya_get_uv_cn_url 'x86_64-unknown-linux-gnu' || true)
echo "CN_URL=\${cn_url:-EMPTY}"

echo "TRIM_CR=\$(tuya_trim_manifest_value \$'https://images.tuyacn.com/test\r')"
crlf_root=\$(mktemp -d)
printf 'UV_X86_64_UNKNOWN_LINUX_GNU_DOWNLOAD_CN=https://images.tuyacn.com/crlf-test.zip\r\n' > "\$crlf_root/uv-manifest.env"
tuya_load_uv_manifest "\$crlf_root"
crlf_cn=\$(tuya_get_uv_cn_url 'x86_64-unknown-linux-gnu' || true)
echo "CRLF_CN=\${crlf_cn:-EMPTY}"
rm -rf "\$crlf_root"
tuya_load_uv_manifest "\$ROOT"

TUYAOPEN_CN_DOWNLOAD=1
export TUYAOPEN_CN_DOWNLOAD
tuya_detect_region
tuya_new_uv_context "\$ROOT"
dl_cn=\$(tuya_get_uv_download_urls | head -n1)
echo "DL_CN=\$dl_cn"

TUYAOPEN_CN_DOWNLOAD=0
export TUYAOPEN_CN_DOWNLOAD
tuya_detect_region
dl_os=\$(tuya_get_uv_download_urls | head -n1)
echo "DL_OS=\$dl_os"
first_intl=\$(tuya_get_release_urls "\$_tuya_uv_ver" | head -n1)
echo "INTL_FIRST=\$first_intl"
UNIT_EOF
) || unit_rc=$?

if [ "$unit_rc" -ne 0 ]; then
    fail 'unit logic subprocess' "exit $unit_rc"
    [ -n "$unit_out" ] && printf '%s\n' "$unit_out" | sed 's/^/        /'
else
    assert_contains 'unit: is_sdk_root positive' "$unit_out" 'OK_ROOT'
    assert_contains 'unit: is_sdk_root negative' "$unit_out" 'OK_TMP'

    key_val=$(echo "$unit_out" | sed -n 's/^KEY=//p')
    assert_eq 'unit: triple manifest key' 'X86_64_UNKNOWN_LINUX_GNU' "$key_val"

    check_val=$(echo "$unit_out" | sed -n 's/^CHECK=//p' | head -n1)
    if [[ "$check_val" != FAIL && "$check_val" == *" "* ]]; then
        pass 'unit: uv artifact check from manifest'
    else
        fail 'unit: uv artifact check from manifest' "got '$check_val'"
    fi

    plan1=$(echo "$unit_out" | sed -n 's/^PLAN1=//p')
    plan0=$(echo "$unit_out" | sed -n 's/^PLAN0=//p')
    pland=$(echo "$unit_out" | sed -n 's/^PLAND=//p')
    assert_eq 'unit: sync plan mirror=1' 'sync mirror' "$plan1"
    assert_eq 'unit: sync plan mirror=0' 'sync --frozen' "$plan0"
    assert_eq 'unit: sync plan default' 'sync --frozen' "$pland"

    pkg_cnt=$(echo "$unit_out" | sed -n 's/^PKG_CNT=//p')
    if [ "${pkg_cnt:-0}" -ge 1 ] 2>/dev/null; then
        pass 'unit: lock package count >= 1'
    else
        fail 'unit: lock package count >= 1' "got '$pkg_cnt'"
    fi

    arch_val=$(echo "$unit_out" | sed -n 's/^ARCH=//p')
    artifact_val=$(echo "$unit_out" | sed -n 's/^ARTIFACT=//p')
    if [ -n "$arch_val" ] && [[ "$artifact_val" == uv-*.tar.gz ]]; then
        pass "unit: platform artifact ($arch_val -> $artifact_val)"
    else
        fail 'unit: platform artifact selection' "arch='$arch_val' artifact='$artifact_val'"
    fi

    assert_contains 'unit: path_add' "$unit_out" 'PATH_ADD_OK'
    assert_contains 'unit: path_remove' "$unit_out" 'PATH_RM_OK'

    url_count=$(echo "$unit_out" | sed -n 's/^URL_COUNT=//p')
    if [ "${url_count:-0}" -ge 2 ] 2>/dev/null; then
        pass 'unit: release base URLs (>=2 mirrors)'
    else
        fail 'unit: release base URLs' "count=$url_count"
    fi

    venv_state=$(echo "$unit_out" | sed -n 's/^VENV_STATE=//p')
    if [ "$venv_state" = MANAGED ] || [ "$venv_state" = LEGACY ]; then
        pass "unit: venv marker probe ($venv_state)"
    else
        fail 'unit: venv marker probe' "got '$venv_state'"
    fi

    assert_eq 'unit: parse +0800 offset' '480' "$(echo "$unit_out" | sed -n 's/^TZ_PARSE_0800=//p')"
    assert_eq 'unit: parse -0500 offset' '-300' "$(echo "$unit_out" | sed -n 's/^TZ_PARSE_0500=//p')"
    assert_eq 'unit: parse +08:00 offset' '480' "$(echo "$unit_out" | sed -n 's/^TZ_PARSE_COLON=//p')"

    tz_live=$(echo "$unit_out" | sed -n 's/^TZ_LIVE=//p' | head -n1)
    if [ "$tz_live" != FAIL ] && [ -n "$tz_live" ]; then
        pass "unit: live tz offset readable ($tz_live)"
    else
        fail 'unit: live tz offset readable' "got '$tz_live'"
    fi

    for tz_marker in TZ450=1 TZ480=1 TZ510=1 TZ449=0 TZ511=0; do
        assert_contains "unit: tz range $tz_marker" "$unit_out" "$tz_marker"
    done

    cn_url_val=$(echo "$unit_out" | sed -n 's/^CN_URL=//p' | head -n1)
    if [[ "$cn_url_val" == *images.tuyacn.com* ]]; then
        pass 'unit: CN download URL parsed from manifest'
    else
        fail 'unit: CN download URL parsed from manifest' "got '$cn_url_val'"
    fi

    trim_cr_val=$(echo "$unit_out" | sed -n 's/^TRIM_CR=//p' | head -n1)
    if [[ "$trim_cr_val" == 'https://images.tuyacn.com/test' ]]; then
        pass 'unit: trim manifest strips CR'
    else
        fail 'unit: trim manifest strips CR' "got '$trim_cr_val'"
    fi

    crlf_cn_val=$(echo "$unit_out" | sed -n 's/^CRLF_CN=//p' | head -n1)
    if [[ "$crlf_cn_val" == 'https://images.tuyacn.com/crlf-test.zip' ]]; then
        pass 'unit: CRLF manifest CN URL has no trailing CR'
    else
        fail 'unit: CRLF manifest CN URL has no trailing CR' "got '$crlf_cn_val'"
    fi

    dl_cn_val=$(echo "$unit_out" | sed -n 's/^DL_CN=//p' | head -n1)
    if [[ "$dl_cn_val" == *images.tuyacn.com* ]]; then
        pass 'unit: CN override uses tuyacn first'
    else
        fail 'unit: CN override uses tuyacn first' "got '$dl_cn_val'"
    fi

    dl_os_val=$(echo "$unit_out" | sed -n 's/^DL_OS=//p' | head -n1)
    if [[ "$dl_os_val" != *images.tuyacn.com* ]] && [[ "$dl_os_val" == http* ]]; then
        pass 'unit: overseas override skips tuyacn'
    else
        fail 'unit: overseas override skips tuyacn' "got '$dl_os_val'"
    fi

    intl_first=$(echo "$unit_out" | sed -n 's/^INTL_FIRST=//p' | head -n1)
    if [[ "$intl_first" == *github.com* ]]; then
        pass 'unit: overseas default mirror is GitHub first'
    else
        fail 'unit: overseas default mirror is GitHub first' "got '$intl_first'"
    fi
fi

legacy_venv_out=""
legacy_venv_out=$(bash --norc --noprofile <<LEGACY_EOF
ROOT='$ROOT'
TUYAOPEN_EXPORT_SKIP_MAIN=1 . "\$ROOT/export.sh"
# Create a fake non-uv-managed .venv (no .tuyaopen-uv marker file)
tmpvenv=\$(mktemp -d)
trap 'rm -rf "\$tmpvenv"' EXIT
OPEN_SDK_ROOT="\$tmpvenv"
mkdir -p "\$tmpvenv/.venv/bin"
# No .tuyaopen-uv marker — simulates a legacy pip venv
tuya_migrate_legacy_venv 2>&1
LEGACY_EOF
)
assert_contains 'migrate_legacy_venv: prints migration notice' \
    "$legacy_venv_out" 'Detected legacy Python venv'
assert_contains 'migrate_legacy_venv: prints removal notice' \
    "$legacy_venv_out" 'Old .venv removed'

# ---------------------------------------------------------------------------
# 5. Guard active
# ---------------------------------------------------------------------------
section 'Guard active'

guard_not_active_out=""
guard_not_active_rc=0
guard_not_active_out=$(
    bash --norc --noprofile <<GUARD_EOF
ROOT='$ROOT'
TUYAOPEN_EXPORT_SKIP_MAIN=1 . "\$ROOT/export.sh"
OPEN_SDK_ROOT="\$ROOT"
unset TUYAOPEN_ENV_ACTIVE
tuya_guard_active && echo GUARDED || echo NOT_GUARDED
GUARD_EOF
) || guard_not_active_rc=$?
assert_ok    'guard_active: not-active returns 1 (continues)' "$guard_not_active_rc"
assert_contains 'guard_active: not-active prints nothing' "$guard_not_active_out" 'NOT_GUARDED'

guard_active_out=""
guard_active_rc=0
guard_active_out=$(bash --norc --noprofile <<GUARD2_EOF
ROOT='$ROOT'
TUYAOPEN_EXPORT_SKIP_MAIN=1 . "\$ROOT/export.sh"
OPEN_SDK_ROOT="\$ROOT"
TUYAOPEN_ENV_ACTIVE=1
export OPEN_SDK_ROOT TUYAOPEN_ENV_ACTIVE
# create a dummy venv python to satisfy the executable check
mkdir -p "\$ROOT/.venv/bin"
touch "\$ROOT/.venv/bin/python" && chmod +x "\$ROOT/.venv/bin/python"
out=\$(tuya_guard_active 2>&1) && rc=0 || rc=\$?
echo "RC=\$rc"
echo "\$out"
GUARD2_EOF
) || guard_active_rc=$?
assert_contains 'guard_active: active prints already-active message' \
    "$guard_active_out" '[TuyaOpen] Environment is already active.'
assert_contains 'guard_active: active prints re-activate hint' \
    "$guard_active_out" 'To re-activate:'

# ---------------------------------------------------------------------------
# 5b. PYTHONHOME hygiene
# ---------------------------------------------------------------------------
section 'PYTHONHOME hygiene'

# An inherited PYTHONHOME (conda / another Python install active in the
# launching shell) breaks startup of every python the export flow runs.
# tuya_clear_pythonhome must clear it (saving the old value) and
# tuya_teardown must restore it, mirroring standard venv activate scripts.
pythonhome_out=""
pythonhome_out=$(bash --norc --noprofile <<PYHOME_EOF
ROOT='$ROOT'
export PYTHONHOME=/foreign/python
TUYAOPEN_EXPORT_SKIP_MAIN=1 . "\$ROOT/export.sh"
tuya_clear_pythonhome
[ -z "\${PYTHONHOME:-}" ] && echo "CLEARED=1"
[ "\${_OLD_TUYA_PYTHONHOME:-}" = '/foreign/python' ] && echo "SAVED=1"
tuya_teardown --silent 2>/dev/null
[ "\${PYTHONHOME:-}" = '/foreign/python' ] && echo "RESTORED=1"
PYHOME_EOF
)
assert_contains 'pythonhome: cleared for the session' "$pythonhome_out" 'CLEARED=1'
assert_contains 'pythonhome: old value saved' "$pythonhome_out" 'SAVED=1'
assert_contains 'pythonhome: restored by teardown' "$pythonhome_out" 'RESTORED=1'

# ---------------------------------------------------------------------------
# 6. Full integration (subshell)
# ---------------------------------------------------------------------------
section 'Full integration'

integration_out=""
integration_rc=0
integration_out=$(bash --norc --noprofile <<INTEGRATION_EOF
set -e
ROOT='$ROOT'
cd "\$ROOT"
mkdir -p .cache && touch .cache/.dont_prompt_update_platform

# shellcheck disable=SC1091
. "\$ROOT/export.sh"

[ -n "\${OPEN_SDK_ROOT:-}" ] && echo "HAS_ROOT=1"
[ -n "\${OPEN_SDK_UV:-}" ] && echo "HAS_UV=1"
[ -n "\${OPEN_SDK_PYTHON:-}" ] && echo "HAS_PYTHON=1"
[ -n "\${OPEN_SDK_PIP:-}" ] && echo "HAS_PIP=1"
[ "\${TUYAOPEN_ENV_ACTIVE:-}" = '1' ] && echo "ACTIVE=1"
[ -n "\${VIRTUAL_ENV:-}" ] && echo "HAS_VENV=1"
[ "\$OPEN_SDK_ROOT" = "\$ROOT" ] && echo "ROOT_MATCH=1"
[ -x "\$OPEN_SDK_UV" ] && echo "UV_EXEC=1"
[ -x "\$OPEN_SDK_PYTHON" ] && echo "PY_EXEC=1"
[ -f "\$ROOT/.venv/.tuyaopen-uv" ] && echo "VENV_MARKER=1"

"\$OPEN_SDK_UV" --version >/dev/null && echo "UV_VERSION_OK=1"
"\$OPEN_SDK_PYTHON" --version 2>&1 | grep -q '3.12' && echo "PY312_OK=1"
"\$OPEN_SDK_PYTHON" "\$ROOT/tos.py" --help >/dev/null 2>&1 && echo "TOS_HELP=1"
command -v tos.py >/dev/null && echo "TOS_PATH=1"

# shellcheck disable=SC1091
. "\$ROOT/export.sh"
[ "\${TUYAOPEN_ENV_ACTIVE:-}" = '1' ] && echo "REFRESH_OK=1"

type deactivate >/dev/null 2>&1 && echo "HAS_DEACTIVATE=1"
deactivate
[ -z "\${TUYAOPEN_ENV_ACTIVE:-}" ] && echo "DEACTIVATED=1"
type deactivate >/dev/null 2>&1 && echo "DEACTIVATE_REMOVED=0" || echo "DEACTIVATE_REMOVED=1"
INTEGRATION_EOF
) || integration_rc=$?

if [ "$integration_rc" -ne 0 ]; then
    fail 'integration subprocess' "exit $integration_rc"
    [ -n "$integration_out" ] && printf '%s\n' "$integration_out" | sed 's/^/        /'
else
    for marker in HAS_ROOT HAS_UV HAS_PYTHON HAS_PIP ACTIVE HAS_VENV ROOT_MATCH \
                  UV_EXEC PY_EXEC VENV_MARKER UV_VERSION_OK PY312_OK TOS_HELP TOS_PATH \
                  REFRESH_OK HAS_DEACTIVATE DEACTIVATED DEACTIVATE_REMOVED=1; do
        assert_contains "integration: $marker" "$integration_out" "$marker"
    done
fi

# ---------------------------------------------------------------------------
# 6. Verbose mode smoke
# ---------------------------------------------------------------------------
section 'Verbose mode'

verbose_rc=0
verbose_out=$(
    bash --norc --noprofile <<VERBOSE_EOF
set -e
ROOT='$ROOT'
cd "\$ROOT"
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
TUYAOPEN_EXPORT_VERBOSE=1
# shellcheck disable=SC1091
. "\$ROOT/export.sh" 2>&1
echo VERBOSE_DONE=1
deactivate 2>/dev/null || true
VERBOSE_EOF
) || verbose_rc=$?
assert_ok 'verbose export completes' "$verbose_rc"
if [ "$verbose_rc" -eq 0 ]; then
    assert_contains 'verbose export banner' "$verbose_out" 'OPEN_SDK_ROOT'
    assert_contains 'verbose export ready' "$verbose_out" 'Ready'
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
section 'Summary'
printf 'Total: %d  Pass: %d  Fail: %d  Skip: %d\n' "$TOTAL" "$PASS" "$FAIL" "$SKIP"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
