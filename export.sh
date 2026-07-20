#!/usr/bin/env bash
#
# Usage: . ./export.sh
#
# Set TUYAOPEN_EXPORT_VERBOSE=1 before sourcing for full diagnostic output.
# Set TUYAOPEN_EXPORT_IDE=1 when invoked by TuyaOpen IDE (stage markers for progress UI).
# Set TUYAOPEN_EXPORT_SKIP_MAIN=1 to load functions only (tests).
# Set TUYAOPEN_CN_DOWNLOAD=1 or 0 to force CN / overseas uv download mirrors (default: auto via timezone).
#
# This script must be *sourced* (not executed). It:
#   * locates the TuyaOpen project root,
#   * ensures `uv` from <root>/.tools/uv/<version>/ (uv-manifest.env),
#   * installs Python 3.12.13 via uv into <root>/.tools/python/3.12.13/,
#   * creates <root>/.venv and runs `uv sync --frozen` (pyproject.toml + uv.lock),
#   * exports OPEN_SDK_ROOT / OPEN_SDK_UV / OPEN_SDK_PYTHON / OPEN_SDK_PIP,
#   * adds the project root to PATH so `tos.py` is runnable,
#   * runs tos.py prepare (host tools on Windows via export.ps1),
#   * registers deactivate / exit helpers and shell completion.

# ---------------------------------------------------------------------------
# Constants (aligned with export.ps1)
# ---------------------------------------------------------------------------
TUYA_UV_VERSION='0.11.18'
TUYA_UV_BASE_URL='https://github.com/astral-sh/uv/releases/download'
TUYA_UV_ASTRAL_BASE_URL='https://releases.astral.sh/github/uv/releases/download'
TUYA_PYTHON_VERSION='3.12.13'
TUYA_VENV_MARKER='.tuyaopen-uv'
TUYA_UV_DOWNLOAD_ATTEMPTS=2
TUYA_ALIYUN_PYPI_INDEX='https://mirrors.aliyun.com/pypi/simple/'
# CN mirror for uv-managed Python (python-build-standalone). Replaces the
# default GitHub base for `uv python install` via UV_PYTHON_INSTALL_MIRROR.
TUYA_PYTHON_INSTALL_MIRROR_CN='https://registry.npmmirror.com/-/binary/python-build-standalone'
TUYA_PROMPT_PREFIX='(TuyaOpen) '
TUYA_CN_TZ_OFFSET_TARGET=480
TUYA_CN_TZ_OFFSET_TOLERANCE=30

if [ "${TUYAOPEN_EXPORT_IDE:-}" = '1' ]; then
    export NO_COLOR=1 FORCE_COLOR=0 CLICOLOR=0
fi

# ---------------------------------------------------------------------------
# Locate this script (bash, zsh, POSIX sh)
# ---------------------------------------------------------------------------
if [ -n "${BASH_VERSION:-}" ]; then
    _tuya_script_dir=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
elif [ -n "${ZSH_VERSION:-}" ]; then
    _tuya_script_dir=$(realpath "$(dirname "${(%):-%x}")")
else
    _tuya_script_dir=$(realpath "$(dirname "$0")")
fi
_tuya_pwd_dir="$(pwd)"

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
tuya_info()  { echo "$@" >&2; }
tuya_debug() { [ -n "${TUYAOPEN_EXPORT_VERBOSE:-}" ] && echo "$@" >&2; return 0; }

tuya_stage() {
    tuya_is_ide_host || return 0
    tuya_info "[TuyaOpen] Stage: $1"
}

tuya_is_ide_host() {
    [ "${TUYAOPEN_EXPORT_IDE:-}" = '1' ]
}

# ---------------------------------------------------------------------------
# Region detection (CN download mirror; UTC+8 ± tolerance)
# ---------------------------------------------------------------------------
tuya_parse_tz_offset_z() {
    local z="$1" sign=1 hours=0 mins=0
    [ -n "$z" ] || return 1
    case "$z" in
        -*)
            sign=-1
            z="${z#-}"
            ;;
        +*)
            z="${z#+}"
            ;;
        *)
            return 1
            ;;
    esac
    z="${z//:/}"
    if [ "${#z}" -lt 4 ]; then
        return 1
    fi
    hours=$((10#${z:0:2}))
    mins=$((10#${z:2:2}))
    echo $(( sign * (hours * 60 + mins) ))
}

tuya_get_utc_offset_minutes() {
    local z=''
    z=$(date +%z 2>/dev/null) || return 1
    tuya_parse_tz_offset_z "$z"
}

tuya_is_in_cn_tz_range() {
    local offset="${1:-}"
    local min max
    if [ -z "$offset" ]; then
        return 1
    fi
    min=$((TUYA_CN_TZ_OFFSET_TARGET - TUYA_CN_TZ_OFFSET_TOLERANCE))
    max=$((TUYA_CN_TZ_OFFSET_TARGET + TUYA_CN_TZ_OFFSET_TOLERANCE))
    [ "$offset" -ge "$min" ] && [ "$offset" -le "$max" ]
}

tuya_is_mainland_china() {
    case "${TUYAOPEN_CN_DOWNLOAD:-}" in
        1) return 0 ;;
        0) return 1 ;;
    esac
    local offset=''
    offset=$(tuya_get_utc_offset_minutes) || return 1
    tuya_is_in_cn_tz_range "$offset"
}

tuya_detect_region() {
    local offset='' override='' msg=''
    tuya_stage region
    case "${TUYAOPEN_CN_DOWNLOAD:-}" in
        1)
            _tuya_use_cn_download=1
            override=' (override)'
            ;;
        0)
            _tuya_use_cn_download=0
            override=' (override)'
            ;;
        *)
            offset=$(tuya_get_utc_offset_minutes) || offset='unknown'
            if [ "$offset" != 'unknown' ] && tuya_is_in_cn_tz_range "$offset"; then
                _tuya_use_cn_download=1
            else
                _tuya_use_cn_download=0
            fi
            ;;
    esac
    if [ "$_tuya_use_cn_download" -eq 1 ]; then
        msg="[TuyaOpen] Region: mainland China (UTC+8±${TUYA_CN_TZ_OFFSET_TOLERANCE}min"
        if [ -n "$offset" ] && [ "$offset" != 'unknown' ]; then
            msg="${msg}, offset=${offset}"
        fi
        msg="${msg}, CN download mirror)${override}"
    else
        msg='[TuyaOpen] Region: overseas'
        if [ -n "$offset" ] && [ "$offset" != 'unknown' ]; then
            msg="${msg} (offset=${offset})"
        fi
        msg="${msg} (GitHub/Astral download source)${override}"
    fi
    # Remember the decision (reason + which source) but don't print it here:
    # it's surfaced at uv download time so a warm start (nothing downloaded)
    # stays quiet.  tuya_debug still shows it under TUYAOPEN_EXPORT_VERBOSE.
    _tuya_region_msg="$msg"
    tuya_debug "$msg"
}

tuya_size_to_mib() {
    local value="$1" unit
    unit=$(echo "$2" | tr '[:lower:]' '[:upper:]')
    case "$unit" in
        KIB) awk "BEGIN {printf \"%.4f\", $value / 1024}" ;;
        MIB) echo "$value" ;;
        GIB) awk "BEGIN {printf \"%.4f\", $value * 1024}" ;;
        *) echo "$value" ;;
    esac
}

_tuya_prog_last_text=''
_tuya_prog_last_at=0
_tuya_prog_last_pct=-1

tuya_emit_if_changed() {
    local text="$1" pct="${2:--1}" min_ms="${3:-2000}" min_pct="${4:-2}"
    local now pct_delta=100 elapsed=999999
    now=$(date +%s 2>/dev/null || echo 0)
    elapsed=$((now - _tuya_prog_last_at))
    if [ "$pct" -ge 0 ] && [ "$_tuya_prog_last_pct" -ge 0 ]; then
        pct_delta=$((pct - _tuya_prog_last_pct))
        [ "$pct_delta" -lt 0 ] && pct_delta=$((-pct_delta))
    fi
    if [ "$text" = "$_tuya_prog_last_text" ] && [ "$elapsed" -lt 5 ]; then
        return 0
    fi
    if [ "$elapsed" -lt 2 ] && [ "$pct_delta" -lt "$min_pct" ]; then
        return 0
    fi
    _tuya_prog_last_text="$text"
    _tuya_prog_last_at="$now"
    _tuya_prog_last_pct="$pct"
    tuya_info "$text"
}

# uv diagnostics: keep error/cause lines from a streamed uv run so a failure
# can explain the real reason (network vs. other) instead of a bare exit code.
_tuya_uv_diag=''

tuya_uv_reset_diag() { _tuya_uv_diag=''; }

tuya_uv_capture_diag() {
    case "$1" in
        error:*|warning:*|*[Ee]rror:*|*Caused\ by:*|*[Ff]ailed\ to\ *|*[Tt]imed\ out*)
            _tuya_uv_diag="${_tuya_uv_diag}${1}
"
            ;;
    esac
}

# Best-effort: does the captured uv output look like a network/connectivity issue?
tuya_uv_diag_is_network() {
    case "$_tuya_uv_diag" in
        *[Dd]ns*|*lookup*|*"name resolution"*|*"Connection refused"*|\
        *"Connection reset"*|*"connect error"*|*"tcp connect"*|*"could not connect"*|\
        *[Tt]imed\ out*|*[Tt]imeout*|*"Failed to fetch"*|*"Failed to download"*|\
        *"error sending request"*|*"Request failed"*|*retries*|*unreachable*|\
        *certificate*|*[Ss][Ss][Ll]*|*[Tt][Ll][Ss]*|*proxy*|*network*)
            return 0
            ;;
    esac
    return 1
}

tuya_uv_print_diag() {
    [ -n "$_tuya_uv_diag" ] || return 0
    tuya_info 'uv output:'
    printf '%s' "$_tuya_uv_diag" | while IFS= read -r _line; do
        [ -n "$_line" ] && tuya_info "  $_line"
    done
}

tuya_warn_uv_lock_contention() {
    # uv serializes environment access via OS-level file locks (e.g. .venv/.lock)
    # and waits for a busy lock silently (nothing is printed without -v), which
    # looks like a hang. A leftover lock FILE after a crash is normal and
    # harmless; only a live uv process holding the lock blocks us.
    local reason="$1"
    tuya_info "[TuyaOpen] $reason"
    tuya_info "           uv waits silently for its file lock (e.g. $OPEN_SDK_ROOT/.venv/.lock)."
    tuya_info '           Likely another TuyaOpen/uv session holds it. Check: pgrep -x uv'
    tuya_info '           Close other sessions or stop stray uv processes, then re-run: . ./export.sh'
}

tuya_uv_run_stream() {
    # Run OPEN_SDK_UV with streaming line-by-line callback and correct exit code.
    # Uses a FIFO so the while loop runs in the current shell (variable mutations
    # persist) and wait() gives the real uv exit code.  Falls back to a temp
    # file when mkfifo is unavailable (no real-time streaming, but correct rc).
    # UV_NO_PROGRESS and UV_LINK_MODE are set/exported here and restored on exit.
    local on_line="$1"
    shift
    local rc=0 line="" fifo="" tmp="" uv_pid=0
    local _saved_no_prog="${UV_NO_PROGRESS:-}"
    local _saved_link="${UV_LINK_MODE:-}"
    UV_NO_PROGRESS=1
    UV_LINK_MODE="${UV_LINK_MODE:-copy}"
    export UV_NO_PROGRESS UV_LINK_MODE
    tuya_uv_reset_diag

    fifo=$(mktemp -u 2>/dev/null || printf '%s' "/tmp/tuya_uv_fifo_$$")
    if mkfifo "$fifo" 2>/dev/null; then
        "$OPEN_SDK_UV" "$@" >"$fifo" 2>&1 &
        uv_pid=$!
        # uv runs as a background job, so Ctrl+C in the sourcing shell never
        # reaches it: it would keep running and hold the environment lock,
        # silently blocking every later sync. Kill it on INT/TERM (existing
        # user traps are restored afterwards where `trap -p` is available).
        _tuya_saved_traps=$(trap -p INT TERM 2>/dev/null || true)
        trap 'kill -TERM "$uv_pid" 2>/dev/null || true' INT TERM
        # A run blocked on a busy uv lock produces no output at all: the
        # sentinel file marks the first output line; a one-shot background
        # watchdog explains total silence after 10 seconds.
        _tuya_uv_sentinel="$fifo.out"
        rm -f "$_tuya_uv_sentinel" 2>/dev/null || true
        (
            sleep 10
            [ -e "$_tuya_uv_sentinel" ] && exit 0
            kill -0 "$uv_pid" 2>/dev/null || exit 0
            tuya_warn_uv_lock_contention 'uv has produced no output for 10+ seconds; it may be waiting to acquire a lock held by another uv process.'
        ) &
        _tuya_uv_watchdog=$!
        while IFS= read -r line; do
            [ -e "$_tuya_uv_sentinel" ] || : > "$_tuya_uv_sentinel"
            tuya_uv_capture_diag "$line"
            [ -n "$line" ] && "$on_line" "$line"
        done <"$fifo"
        wait "$uv_pid" || rc=$?
        kill "$_tuya_uv_watchdog" 2>/dev/null || true
        wait "$_tuya_uv_watchdog" 2>/dev/null || true
        trap - INT TERM
        [ -n "$_tuya_saved_traps" ] && eval "$_tuya_saved_traps" 2>/dev/null
        rm -f "$_tuya_uv_sentinel" 2>/dev/null || true
        unset _tuya_saved_traps _tuya_uv_sentinel _tuya_uv_watchdog
        rm -f "$fifo" 2>/dev/null || true
    else
        tmp=$(mktemp 2>/dev/null || printf '%s' "/tmp/tuya_uv_stream_$$")
        "$OPEN_SDK_UV" "$@" >"$tmp" 2>&1 || rc=$?
        while IFS= read -r line; do
            tuya_uv_capture_diag "$line"
            [ -n "$line" ] && "$on_line" "$line"
        done <"$tmp"
        rm -f "$tmp" 2>/dev/null || true
    fi

    if [ -z "$_saved_no_prog" ]; then unset UV_NO_PROGRESS;  else UV_NO_PROGRESS="$_saved_no_prog"; export UV_NO_PROGRESS; fi
    if [ -z "$_saved_link" ];    then unset UV_LINK_MODE;     else UV_LINK_MODE="$_saved_link";      export UV_LINK_MODE;     fi
    return "$rc"
}

_tuya_sync_current=0
_tuya_sync_last_name=''
_tuya_sync_pkg_total=1

tuya_on_uv_sync_line() {
    tuya_parse_uv_sync_line "$1" "$_tuya_sync_pkg_total"
}

tuya_parse_uv_sync_line() {
    local line="$1" total="$2"
    local changed=0 pkg="" n=0
    case "$line" in
        +*)
            pkg=${line#+ }
            pkg=${pkg%% *}
            _tuya_sync_current=$((_tuya_sync_current + 1))
            [ "$_tuya_sync_current" -gt "$total" ] && _tuya_sync_current=$total
            _tuya_sync_last_name="$pkg"
            changed=1
            ;;
    esac
    case "$line" in
        [Dd]ownloading\ *)
            pkg=${line#Downloading }
            pkg=${pkg%% *}
            n=$((_tuya_sync_current + 1))
            [ "$n" -gt "$total" ] && n=$total
            if [ "$n" -gt "$_tuya_sync_current" ]; then
                _tuya_sync_current=$n
                _tuya_sync_last_name="$pkg"
                changed=1
            fi
            ;;
    esac
    case "$line" in
        *Installed\ [0-9]*\ packages*)
            n=$(echo "$line" | sed -n 's/.*Installed \([0-9][0-9]*\) packages.*/\1/p')
            if [ -n "$n" ] && [ "$n" -gt "$_tuya_sync_current" ]; then
                [ "$n" -gt "$total" ] && n=$total
                _tuya_sync_current=$n
                changed=1
            fi
            ;;
        *Audited\ [0-9]*\ packages*)
            n=$(echo "$line" | sed -n 's/.*Audited \([0-9][0-9]*\) packages.*/\1/p')
            if [ -n "$n" ] && [ "$n" -gt "$_tuya_sync_current" ]; then
                [ "$n" -gt "$total" ] && n=$total
                _tuya_sync_current=$n
                changed=1
            fi
            ;;
    esac
    if [ "$changed" -eq 1 ]; then
        local pct=0 filled=0 empty=28 bar='' text='' i=0
        [ "$total" -lt 1 ] && total=1
        pct=$((100 * _tuya_sync_current / total))
        [ "$pct" -gt 100 ] && pct=100
        filled=$((28 * _tuya_sync_current / total))
        [ "$filled" -gt 28 ] && filled=28
        empty=$((28 - filled))
        bar=$(printf '%*s' "$filled" '' | tr ' ' '#')
        bar="${bar}$(printf '%*s' "$empty" '' | tr ' ' '-')"
        text="[TuyaOpen] Syncing dependencies [$bar] ${_tuya_sync_current}/${total} (${pct}%)"
        [ -n "$_tuya_sync_last_name" ] && text="$text - $_tuya_sync_last_name"
        tuya_emit_if_changed "$text" "$pct"
    fi
}

_tuya_py_artifact='cpython'
_tuya_py_total_mib=0
_tuya_py_recv_mib=-1

tuya_parse_python_install_line() {
    local line="$1" recv_u='' total_u='' pct=0 text='' changed=0
    case "$line" in
        *[Dd]ownloading\ cpython*)
            _tuya_py_artifact=$(echo "$line" | sed -n 's/.*[Dd]ownloading \([^ (]*\).*/\1/p')
            changed=1
            ;;
    esac
    recv_u=$(echo "$line" | sed -n 's/.*\([0-9][0-9.]*\) MiB *\/ *\([0-9][0-9.]*\) MiB.*/\1/p')
    total_u=$(echo "$line" | sed -n 's/.*\([0-9][0-9.]*\) MiB *\/ *\([0-9][0-9.]*\) MiB.*/\2/p')
    if [ -n "$recv_u" ] && [ -n "$total_u" ]; then
        _tuya_py_recv_mib="$recv_u"
        _tuya_py_total_mib="$total_u"
        changed=1
    fi
    if [ "$changed" -eq 1 ]; then
        text="[TuyaOpen] Installing Python ${TUYA_PYTHON_VERSION}: ${_tuya_py_artifact}"
        if [ -n "$total_u" ] && [ -n "$recv_u" ]; then
            pct=$(awk "BEGIN {v=int(100*$recv_u/$total_u); if (v>99) v=99; print v}")
            text="$text: $recv_u / $total_u MB (${pct}%)"
            tuya_emit_if_changed "$text" "$pct"
        else
            tuya_emit_if_changed "$text" -1
        fi
    fi
}

tuya_export_cold_start_kind() {
    local venv_path="$OPEN_SDK_ROOT/.venv" install_dir python_exe=''
    if ! tuya_load_uv_manifest "$OPEN_SDK_ROOT"; then
        echo 'full'
        return 0
    fi
    if ! tuya_new_uv_context "$OPEN_SDK_ROOT"; then
        echo 'full'
        return 0
    fi
    if ! tuya_test_uv_exe "$_tuya_uv_exe"; then
        echo 'full'
        return 0
    fi
    install_dir=$(tuya_python_install_dir)
    python_exe=$(tuya_find_managed_python "$install_dir")
    if ! tuya_test_python_exe "$python_exe"; then
        echo 'full'
        return 0
    fi
    if ! tuya_is_uv_venv "$venv_path"; then
        echo 'venv_only'
        return 0
    fi
    echo 'warm'
}

tuya_write_cold_start_hint() {
    case "$1" in
        full)
            tuya_info '[TuyaOpen] First-time setup: downloading uv, Python, and dependencies (may take 3-10 minutes). Please wait...'
            ;;
        venv_only)
            tuya_info '[TuyaOpen] Rebuilding virtual environment (Python already installed)...'
            ;;
    esac
}

tuya_export_cold_start() {
    [ "$(tuya_export_cold_start_kind)" != 'warm' ]
}

tuya_error() {
    local stage="$1" summary="$2" cause="$3"
    shift 3
    tuya_info "[TuyaOpen] Error: $stage - $summary"
    [ -n "$cause" ] && tuya_info "Cause: $cause"
    if [ "$#" -gt 0 ]; then
        tuya_info 'Next:'
        while [ "$#" -gt 0 ]; do
            tuya_info "  $1"
            shift
        done
    fi
}

tuya_cleanup() {
    unset _tuya_script_dir _tuya_pwd_dir tuya_is_env_active
    unset _tuya_uv_ver _tuya_uv_triple _tuya_uv_artifact _tuya_uv_url_astral _tuya_uv_url_github _tuya_use_cn_download _tuya_region_msg
    unset _tuya_uv_dl_size _tuya_uv_dl_sha256 _tuya_uv_tools_dir
    unset _tuya_uv_archive _tuya_uv_exe
    unset _tuya_managed_python _tuya_venv_py
    unset _tuya_sync_current _tuya_sync_last_name _tuya_sync_pkg_total
    unset _tuya_py_artifact _tuya_py_total_mib _tuya_py_recv_mib
    unset _tuya_prog_last_text _tuya_prog_last_at _tuya_prog_last_pct
    unset _tuya_uv_diag
    unset -f tuya_debug tuya_error \
             tuya_is_sdk_root tuya_print_version tuya_has_cmd \
             tuya_ensure_dir tuya_path_add \
             tuya_triple_manifest_key tuya_trim_manifest_value tuya_load_uv_manifest \
             tuya_get_uv_artifact_check tuya_get_release_urls tuya_get_uv_download_urls \
             tuya_get_uv_cn_url tuya_has_uv_download_override tuya_uv_source_label \
             tuya_parse_tz_offset_z tuya_get_utc_offset_minutes tuya_is_in_cn_tz_range tuya_is_mainland_china tuya_detect_region \
             tuya_get_arch tuya_select_uv_artifact \
             tuya_check_glibc tuya_download_file tuya_verify_sha256 \
             tuya_download_file_ide \
             tuya_test_uv_exe tuya_new_uv_context \
             tuya_resolve_uv tuya_download_uv \
             tuya_extract_uv tuya_install_uv tuya_setup_uv \
             tuya_python_install_dir tuya_find_managed_python \
             tuya_test_python_exe tuya_install_python tuya_install_python_ide \
             tuya_run_python_install tuya_python_install_error \
             tuya_uv_reset_diag tuya_uv_capture_diag tuya_uv_diag_is_network tuya_uv_print_diag \
             tuya_warn_uv_lock_contention tuya_file_mtime_epoch \
             tuya_setup_python tuya_uv_sync_plan tuya_uv \
             tuya_lock_pkg_count tuya_sync_deps tuya_sync_deps_ide tuya_sync_deps_error \
             tuya_is_uv_venv tuya_migrate_legacy_venv \
             tuya_setup_venv tuya_is_env_active \
             tuya_set_env tuya_reset_cache \
             tuya_install_prompt tuya_install_completion \
             tuya_register_helpers tuya_invoke_hello \
             tuya_platform_banner tuya_guard_active tuya_finalize \
             tuya_is_ide_host tuya_size_to_mib tuya_emit_if_changed \
             tuya_uv_run_stream tuya_on_uv_sync_line \
             tuya_parse_uv_sync_line tuya_parse_python_install_line \
             tuya_export_cold_start_kind tuya_write_cold_start_hint \
             tuya_export_cold_start tuya_check_git \
             tuya_human_size tuya_cleanup 2>/dev/null || true
}

tuya_has_cmd() {
    command -v "$1" >/dev/null 2>&1
}

tuya_human_size() {
    local bytes="$1" mb kb
    mb=$((bytes / 1048576))
    [ "$mb" -ge 1 ] && { echo "${mb} MB"; return 0; }
    kb=$((bytes / 1024))
    [ "$kb" -ge 1 ] && { echo "${kb} KB"; return 0; }
    echo "${bytes} B"
}

tuya_ensure_dir() {
    local dir="$1"
    if [ -d "$dir" ]; then
        return 0
    fi
    if mkdir -p "$dir" 2>/dev/null; then
        return 0
    fi
    tuya_error Io 'Cannot create directory.' "$dir" \
        "Ensure the path is writable: $dir" 'Re-run with sufficient permissions.'
    return 1
}

tuya_path_add() {
    local dir="$1"
    case ":${PATH}:" in
        *":$dir:"*) ;;
        *) PATH="$dir:$PATH" ;;
    esac
    export PATH
}

tuya_path_remove() {
    local dir="$1" new_path="" part rest
    rest="${PATH}:"
    while [ -n "$rest" ]; do
        part="${rest%%:*}"
        rest="${rest#*:}"
        [ -z "$part" ] && continue
        [ "$part" = "$dir" ] && continue
        new_path="${new_path:+${new_path}:}${part}"
    done
    PATH="$new_path"
    export PATH
}

# ---------------------------------------------------------------------------
# Project root
# ---------------------------------------------------------------------------
tuya_is_sdk_root() {
    [ -f "$1/export.sh" ] && [ -f "$1/pyproject.toml" ] && [ -f "$1/uv.lock" ] && [ -f "$1/tos.py" ]
}

if tuya_is_sdk_root "$_tuya_pwd_dir"; then
    OPEN_SDK_ROOT="$_tuya_pwd_dir"
elif tuya_is_sdk_root "$_tuya_script_dir"; then
    OPEN_SDK_ROOT="$_tuya_script_dir"
else
    tuya_error Entry 'Unable to locate TuyaOpen project root.' \
        'export.sh + pyproject.toml + uv.lock + tos.py not found.' \
        'Run from the project root or use the absolute path.'
    tuya_cleanup
    return 1
fi
export OPEN_SDK_ROOT

_tuya_missing=""
for _f in export.sh pyproject.toml uv.lock tos.py; do
    if [ ! -f "$OPEN_SDK_ROOT/$_f" ]; then
        _tuya_missing="$_tuya_missing $_f"
    fi
done
unset _f
if [ -n "$_tuya_missing" ]; then
    tuya_error Entry 'Required project files are missing.' "${_tuya_missing# }" \
        'Use a complete TuyaOpen clone.' "Missing under: $OPEN_SDK_ROOT"
    unset _tuya_missing
    tuya_cleanup
    return 1
fi
unset _tuya_missing

# ---------------------------------------------------------------------------
# Git availability (hard dependency: platform update / submodules / version)
# ---------------------------------------------------------------------------
tuya_check_git() {
    if tuya_has_cmd git; then
        return 0
    fi
    tuya_error Git 'git not found. It may not be installed.' \
        'Open a terminal and install, e.g.: sudo apt install git  (or: brew install git)' \
        'Then restart your terminal and re-run: . ./export.sh'
    return 1
}

# ---------------------------------------------------------------------------
# Git version banner
# ---------------------------------------------------------------------------
tuya_print_version() {
    local root="$1" ver="" tag="" short="" dirty="" status_out=""
    if ! tuya_has_cmd git; then
        echo "TuyaOpen version: (git not found)"
        return 0
    fi
    if [ ! -e "$root/.git" ]; then
        echo "TuyaOpen version: (not a git checkout)"
        return 0
    fi
    status_out=$(git -C "$root" status --porcelain 2>/dev/null) || status_out=""
    if [ -n "$status_out" ]; then
        dirty="-dirty"
    fi
    ver=$(git -C "$root" describe --tags --exact-match HEAD 2>/dev/null) || ver=""
    if [ -z "$ver" ]; then
        tag=$(git -C "$root" describe --tags --abbrev=0 HEAD 2>/dev/null) || tag=""
        short=$(git -C "$root" rev-parse --short=8 HEAD 2>/dev/null) || short=""
        if [ -n "$tag" ] && [ -n "$short" ]; then
            ver="${tag}-${short}"
        elif [ -n "$short" ]; then
            ver="$short"
        else
            ver="unknown"
        fi
    fi
    echo "TuyaOpen version: ${ver}${dirty}"
}

# ---------------------------------------------------------------------------
# uv manifest (uv-manifest.env + env overrides; see export.ps1)
# ---------------------------------------------------------------------------
tuya_triple_manifest_key() {
    echo "$1" | tr '[:lower:]' '[:upper:]' | tr '-' '_'
}

tuya_trim_manifest_value() {
    local v="$1"
    v="${v%$'\r'}"
    v="${v#"${v%%[![:space:]]*}"}"
    v="${v%"${v##*[![:space:]]}"}"
    printf '%s' "$v"
}

tuya_load_uv_manifest() {
    local root="$1"
    local env_file="$root/uv-manifest.env"
    _tuya_uv_ver="$TUYA_UV_VERSION"
    _tuya_uv_url_github="$TUYA_UV_BASE_URL"
    _tuya_uv_url_astral="$TUYA_UV_ASTRAL_BASE_URL"

    if [ -f "$env_file" ]; then
        local key val
        while IFS= read -r line || [ -n "$line" ]; do
            line="${line%$'\r'}"
            case "$line" in
                ''|\#*) continue ;;
                UV_VERSION=*) _tuya_uv_ver="$(tuya_trim_manifest_value "${line#UV_VERSION=}")" ;;
                UV_DOWNLOAD_SOURCE_ASTRAL=*) _tuya_uv_url_astral="$(tuya_trim_manifest_value "${line#UV_DOWNLOAD_SOURCE_ASTRAL=}")" ;;
                UV_DOWNLOAD_SOURCE_GITHUB=*) _tuya_uv_url_github="$(tuya_trim_manifest_value "${line#UV_DOWNLOAD_SOURCE_GITHUB=}")" ;;
                UV_*_DOWNLOAD_CN=*)
                    key="${line%%_DOWNLOAD_CN=*}"
                    key="${key#UV_}"
                    val="$(tuya_trim_manifest_value "${line#*=}")"
                    eval "_tuya_uv_cn_url_${key}=\$val"
                    ;;
                UV_*_SHA256=*)
                    key="${line%%_SHA256=*}"
                    key="${key#UV_}"
                    val="$(tuya_trim_manifest_value "${line#*=}")"
                    eval "_tuya_uv_sha256_${key}=\$val"
                    ;;
                UV_*_SIZE=*)
                    key="${line%%_SIZE=*}"
                    key="${key#UV_}"
                    val="$(tuya_trim_manifest_value "${line#*=}")"
                    eval "_tuya_uv_size_${key}=\$val"
                    ;;
            esac
        done < "$env_file"
    fi
}

tuya_get_uv_artifact_check() {
    local triple="$1" key size_var sha_var
    key=$(tuya_triple_manifest_key "$triple")
    size_var="_tuya_uv_size_${key}"
    sha_var="_tuya_uv_sha256_${key}"
    eval "local size=\${${size_var}:-}"
    eval "local sha=\${${sha_var}:-}"
    if [ -z "$size" ] || [ -z "$sha" ]; then
        return 1
    fi
    echo "$size $sha"
}

tuya_get_release_urls() {
    local version="$1"
    if [ -n "${UV_DOWNLOAD_URL:-}" ]; then
        echo "$UV_DOWNLOAD_URL"
        return 0
    fi
    if [ -n "${UV_INSTALLER_GHE_BASE_URL:-}" ]; then
        echo "${UV_INSTALLER_GHE_BASE_URL}/astral-sh/uv/releases/download/$version"
        return 0
    fi
    if [ -n "${UV_INSTALLER_GITHUB_BASE_URL:-}" ]; then
        echo "${UV_INSTALLER_GITHUB_BASE_URL}/astral-sh/uv/releases/download/$version"
        return 0
    fi
    echo "${_tuya_uv_url_github}/$version"
    echo "${_tuya_uv_url_astral}/$version"
}

tuya_has_uv_download_override() {
    [ -n "${UV_DOWNLOAD_URL:-}" ] \
        || [ -n "${UV_INSTALLER_GHE_BASE_URL:-}" ] \
        || [ -n "${UV_INSTALLER_GITHUB_BASE_URL:-}" ]
}

tuya_get_uv_cn_url() {
    local triple="$1" key url_var url=''
    key=$(tuya_triple_manifest_key "$triple")
    url_var="_tuya_uv_cn_url_${key}"
    eval "url=\${${url_var}:-}"
    if [ -n "$url" ]; then
        echo "$url"
    fi
}

tuya_get_uv_download_urls() {
    local base cn_url=''
    if tuya_has_uv_download_override; then
        for base in $(tuya_get_release_urls "$_tuya_uv_ver"); do
            echo "${base%/}/$_tuya_uv_artifact"
        done
        return 0
    fi
    if [ "${_tuya_use_cn_download:-0}" -eq 1 ]; then
        cn_url=$(tuya_get_uv_cn_url "$_tuya_uv_triple")
        if [ -n "$cn_url" ]; then
            echo "$cn_url"
        fi
    fi
    for base in $(tuya_get_release_urls "$_tuya_uv_ver"); do
        echo "${base%/}/$_tuya_uv_artifact"
    done
}

# ---------------------------------------------------------------------------
# Platform / artifact selection (from uv-installer.sh)
# ---------------------------------------------------------------------------
tuya_check_glibc() {
    local min_major="$1" min_minor="$2" local_glibc major minor
    if ! tuya_has_cmd ldd; then
        return 1
    fi
    local_glibc=$(ldd --version 2>/dev/null | awk 'FNR<=1 {print $NF}')
    major=$(echo "$local_glibc" | awk -F. '{print $1}')
    minor=$(echo "$local_glibc" | awk -F. '{print $2}')
    if [ "$major" = "$min_major" ] && [ "${minor:-0}" -ge "$min_minor" ] 2>/dev/null; then
        return 0
    fi
    tuya_debug "System glibc ($local_glibc) is below ${min_major}.${min_minor}; trying musl fallback."
    return 1
}

tuya_get_arch() {
    local ostype cputype clibtype bitness current_exe _arch
    ostype=$(uname -s)
    cputype=$(uname -m)
    clibtype='gnu'

    case "$ostype" in
        Linux)
            if ldd --version 2>&1 | grep -q musl; then
                clibtype='musl-dynamic'
            fi
            if [ -r /proc/self/exe ]; then
                current_exe=/proc/self/exe
            elif [ -n "${SHELL:-}" ]; then
                current_exe=$SHELL
            else
                current_exe=/bin/sh
            fi
            if tuya_has_cmd head; then
                case "$(head -c 5 "$current_exe" 2>/dev/null)" in
                    $'\177ELF\001') bitness=32 ;;
                    $'\177ELF\002') bitness=64 ;;
                    *) bitness=64 ;;
                esac
            else
                bitness=64
            fi
            ostype="unknown-linux-$clibtype"
            ;;
        Darwin)
            ostype='apple-darwin'
            if [ "$cputype" = i386 ] && sysctl hw.optional.x86_64 2>/dev/null | grep -q ': 1'; then
                cputype=x86_64
            elif [ "$cputype" = x86_64 ] && sysctl hw.optional.arm64 2>/dev/null | grep -q ': 1'; then
                cputype=arm64
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            tuya_error Entry 'Use export.ps1 on Windows.' "$(uname -s)" \
                'Run: . .\export.ps1'
            return 1
            ;;
        *)
            tuya_error Entry 'Unsupported host OS.' "$ostype" \
                'Use Linux or macOS with export.sh.'
            return 1
            ;;
    esac

    case "$cputype" in
        x86_64|x64|amd64) cputype=x86_64 ;;
        aarch64|arm64) cputype=aarch64 ;;
        armv7l|armv8l) cputype=armv7; ostype="${ostype}eabihf" ;;
        i386|i686|x86) cputype=i686 ;;
        riscv64) cputype=riscv64gc ;;
        *)
            tuya_error Uv 'Unknown CPU type.' "$cputype" \
                'Open an issue with uname -m output.'
            return 1
            ;;
    esac

    if [ "$ostype" = 'unknown-linux-gnu' ] && [ "${bitness:-64}" -eq 32 ] && [ "$cputype" = x86_64 ]; then
        cputype=i686
    fi

    _arch="${cputype}-${ostype}"
    echo "$_arch"
}

tuya_select_uv_artifact() {
    local true_arch="$1" archive=""

    case "$true_arch" in
        x86_64-unknown-linux-gnu)
            archive='uv-x86_64-unknown-linux-gnu.tar.gz'
            tuya_check_glibc 2 17 || archive='uv-x86_64-unknown-linux-musl.tar.gz'
            ;;
        x86_64-unknown-linux-musl-dynamic|x86_64-unknown-linux-musl-static)
            archive='uv-x86_64-unknown-linux-musl.tar.gz'
            ;;
        aarch64-unknown-linux-gnu)
            archive='uv-aarch64-unknown-linux-gnu.tar.gz'
            tuya_check_glibc 2 28 || archive='uv-aarch64-unknown-linux-musl.tar.gz'
            ;;
        aarch64-unknown-linux-musl-dynamic|aarch64-unknown-linux-musl-static)
            archive='uv-aarch64-unknown-linux-musl.tar.gz'
            ;;
        x86_64-apple-darwin)
            archive='uv-x86_64-apple-darwin.tar.gz'
            ;;
        aarch64-apple-darwin)
            archive='uv-aarch64-apple-darwin.tar.gz'
            ;;
        *)
            tuya_error Uv 'No uv build for this platform.' "$true_arch" \
                'See https://github.com/astral-sh/uv/releases'
            return 1
            ;;
    esac
    echo "$archive"
}

# ---------------------------------------------------------------------------
# Download / verify / extract uv
# ---------------------------------------------------------------------------
tuya_download_file_ide() {
    local url="$1" dest="$2" expected="$3" label="$4" token="${UV_GITHUB_TOKEN:-}"
    local tmp="${dest}.part" pid=0 rc=0 received=0 total_mib=0 recv_mib=0
    local last_text='' line=''
    total_mib=$(awk "BEGIN {printf \"%.1f\", $expected / 1048576}")
    rm -f "$tmp" 2>/dev/null || true
    if tuya_has_cmd curl; then
        if [ -n "$token" ]; then
            curl -fL -sS --header "Authorization: Bearer $token" "$url" -o "$tmp" &
        else
            curl -fL -sS "$url" -o "$tmp" &
        fi
        pid=$!
        while kill -0 "$pid" 2>/dev/null; do
            if [ -f "$tmp" ]; then
                received=$(wc -c <"$tmp" 2>/dev/null || echo 0)
                recv_mib=$(awk "BEGIN {printf \"%.1f\", $received / 1048576}")
                line="${label}: ${recv_mib} / ${total_mib} MB"
                if [ "$line" != "$last_text" ]; then
                    tuya_info "$line"
                    last_text="$line"
                fi
            fi
            sleep 1
        done
        wait "$pid" || rc=$?
        if [ "$rc" -eq 0 ]; then
            mv -f "$tmp" "$dest"
            line="${label}: ${total_mib} / ${total_mib} MB"
            [ "$line" != "$last_text" ] && tuya_info "$line"
        else
            rm -f "$tmp" 2>/dev/null || true
        fi
        return "$rc"
    fi
    tuya_download_file "$url" "$dest"
}

tuya_download_file() {
    local url="$1" dest="$2" expected="${3:-0}" label="${4:-[TuyaOpen] Downloading}"
    local token="${UV_GITHUB_TOKEN:-}"
    if tuya_is_ide_host && [ "$expected" -gt 0 ] 2>/dev/null; then
        tuya_download_file_ide "$url" "$dest" "$expected" "$label"
        return $?
    fi
    if tuya_has_cmd curl; then
        if [ -n "$token" ]; then
            curl -fL --progress-bar --header "Authorization: Bearer $token" "$url" -o "$dest"
        else
            curl -fL --progress-bar "$url" -o "$dest"
        fi
        return $?
    fi
    if tuya_has_cmd wget; then
        if [ -n "$token" ]; then
            wget --header "Authorization: Bearer $token" "$url" -O "$dest"
        else
            wget "$url" -O "$dest"
        fi
        return $?
    fi
    tuya_error Uv 'curl or wget is required.' 'Neither found on PATH.' \
        'Install curl or wget and re-run: . ./export.sh'
    return 1
}

tuya_verify_sha256() {
    local file="$1" expected="$2" actual=""
    if ! tuya_has_cmd sha256sum; then
        tuya_debug 'Skipping SHA256 verification (sha256sum not found).'
        return 0
    fi
    actual=$(sha256sum -b "$file" | awk '{print $1}')
    if [ "$actual" != "$expected" ]; then
        tuya_debug "SHA256 mismatch: got $actual want $expected"
        return 1
    fi
    return 0
}

tuya_test_uv_exe() {
    local exe="$1"
    [ -x "$exe" ] && "$exe" --version >/dev/null 2>&1
}

tuya_new_uv_context() {
    local root="$1" true_arch triple artifact check size sha
    true_arch=$(tuya_get_arch) || return 1
    artifact=$(tuya_select_uv_artifact "$true_arch") || return 1
    triple="${artifact#uv-}"
    triple="${triple%.tar.gz}"

    check=$(tuya_get_uv_artifact_check "$triple") || {
        local manifest_key
        manifest_key=$(tuya_triple_manifest_key "$triple")
        tuya_error Uv 'Missing uv artifact metadata.' \
            "UV_${manifest_key}_SIZE / UV_${manifest_key}_SHA256 in uv-manifest.env" \
            'Add checksums for this platform to uv-manifest.env.' \
            'Re-run: . ./export.sh'
        return 1
    }
    size="${check%% *}"
    sha="${check#* }"

    _tuya_uv_triple="$triple"
    _tuya_uv_artifact="$artifact"
    _tuya_uv_dl_size="$size"
    _tuya_uv_dl_sha256="$sha"
    _tuya_uv_tools_dir="$root/.tools/uv/$_tuya_uv_ver"
    _tuya_uv_archive="$root/.tools/archives/uv/$_tuya_uv_ver/$artifact"
    _tuya_uv_exe="$_tuya_uv_tools_dir/uv"
}

# Map a download URL to a short, friendly source name (github/astral/tuyacn).
tuya_uv_source_label() {
    case "$1" in
        *github.com*) echo github ;;
        *astral.sh*) echo astral ;;
        *tuyacn.com*) echo tuyacn ;;
        *)
            local host="${1#*://}"
            host="${host%%/*}"
            echo "${host:-unknown}"
            ;;
    esac
}

tuya_download_uv() {
    local url attempt mirror=0 total=0 rc=1 src=''
    total=$(tuya_get_uv_download_urls | wc -l | awk '{print $1}')
    for url in $(tuya_get_uv_download_urls); do
        mirror=$((mirror + 1))
        src=$(tuya_uv_source_label "$url")
        if [ "$total" -gt 1 ]; then
            tuya_info "[TuyaOpen] Downloading ${_tuya_uv_artifact} from ${src} (source ${mirror}/${total})"
        else
            tuya_info "[TuyaOpen] Downloading ${_tuya_uv_artifact} from ${src}"
        fi
        tuya_debug "[TuyaOpen] URL: $url"
        attempt=1
        while [ "$attempt" -le "$TUYA_UV_DOWNLOAD_ATTEMPTS" ]; do
            [ "$attempt" -gt 1 ] && tuya_info "[TuyaOpen] Retry $attempt/$TUYA_UV_DOWNLOAD_ATTEMPTS from ${src}..."
            rm -f "$_tuya_uv_archive" 2>/dev/null || true
            if tuya_download_file "$url" "$_tuya_uv_archive" "$_tuya_uv_dl_size" "[TuyaOpen] Downloading $_tuya_uv_artifact"; then
                rc=0
                break 2
            else
                rc=$?
            fi
            attempt=$((attempt + 1))
        done
        if [ "$mirror" -lt "$total" ]; then
            tuya_info "[TuyaOpen] Download from ${src} failed (exit ${rc}); trying next source..."
        else
            tuya_info "[TuyaOpen] Download from ${src} failed (exit ${rc})."
        fi
    done
    return "$rc"
}

tuya_resolve_uv() {
    local size=""
    if [ -f "$_tuya_uv_archive" ]; then
        size=$(wc -c < "$_tuya_uv_archive" 2>/dev/null | awk '{print $1}' || echo 0)
        if [ "$size" = "$_tuya_uv_dl_size" ] && tuya_verify_sha256 "$_tuya_uv_archive" "$_tuya_uv_dl_sha256"; then
            tuya_debug '[TuyaOpen] Using cached uv package.'
            return 0
        fi
        tuya_debug '[TuyaOpen] Removing invalid uv cache.'
        rm -f "$_tuya_uv_archive" 2>/dev/null || true
    fi

    tuya_ensure_dir "$(dirname "$_tuya_uv_archive")" || return 1
    local size_human
    size_human=$(tuya_human_size "${_tuya_uv_dl_size:-0}")
    [ -n "${_tuya_region_msg:-}" ] && tuya_info "$_tuya_region_msg"
    tuya_info "[TuyaOpen] Downloading uv v${_tuya_uv_ver} (${size_human})..."
    if ! tuya_download_uv; then
        tuya_error Uv 'uv download failed.' 'All mirrors and retries exhausted.' \
            'Check network or proxy.' 'See manual install below.'
        tuya_info '[TuyaOpen] Manual install:'
        tuya_info "  Save archive to: $_tuya_uv_archive"
        tuya_info "  Or extract uv, uvx to: $_tuya_uv_tools_dir"
        tuya_info '  Then re-run: . ./export.sh'
        return 1
    fi

    size=$(wc -c < "$_tuya_uv_archive" 2>/dev/null | awk '{print $1}' || echo 0)
    if [ "$size" != "$_tuya_uv_dl_size" ] || ! tuya_verify_sha256 "$_tuya_uv_archive" "$_tuya_uv_dl_sha256"; then
        rm -f "$_tuya_uv_archive" 2>/dev/null || true
        tuya_error Uv 'Downloaded package failed verification.' 'Size or SHA256 mismatch.' \
            'Delete the archive and re-run: . ./export.sh'
        return 1
    fi
    return 0
}

tuya_extract_uv() {
    local extract_dir="" bin="" installed=0
    tuya_info '[TuyaOpen] Extracting uv...'
    tuya_ensure_dir "$_tuya_uv_tools_dir" || return 1
    extract_dir=$(mktemp -d "${TMPDIR:-/tmp}/tuya_uv.XXXXXX") || return 1

    if ! tar xf "$_tuya_uv_archive" --strip-components=1 -C "$extract_dir" 2>/dev/null; then
        rm -rf "$extract_dir" 2>/dev/null || true
        tuya_error Uv 'Failed to extract uv archive.' "$_tuya_uv_archive" \
            'Remove cached archive and re-run: . ./export.sh'
        return 1
    fi

    for bin in uv uvx; do
        if [ -f "$extract_dir/$bin" ]; then
            cp "$extract_dir/$bin" "$_tuya_uv_tools_dir/$bin"
            chmod +x "$_tuya_uv_tools_dir/$bin"
            [ "$bin" = uv ] && installed=1
        fi
    done
    rm -rf "$extract_dir" 2>/dev/null || true

    if [ "$installed" -ne 1 ]; then
        tuya_error Uv 'uv binary not found in archive.' "$_tuya_uv_archive" \
            'Remove cached archive and re-run: . ./export.sh'
        return 1
    fi
    return 0
}

tuya_install_uv() {
    if tuya_test_uv_exe "$_tuya_uv_exe"; then
        tuya_debug "[TuyaOpen] uv ready: $_tuya_uv_exe"
        return 0
    fi

    local legacy_root="$OPEN_SDK_ROOT/.tools/uv"
    if [ -x "$legacy_root/uv" ] && [ "$legacy_root" != "$_tuya_uv_tools_dir" ]; then
        tuya_debug "[TuyaOpen] Migrating uv from legacy path $legacy_root"
        tuya_ensure_dir "$_tuya_uv_tools_dir" || return 1
        for bin in uv uvx; do
            [ -f "$legacy_root/$bin" ] && cp "$legacy_root/$bin" "$_tuya_uv_tools_dir/$bin" && chmod +x "$_tuya_uv_tools_dir/$bin"
        done
        if tuya_test_uv_exe "$_tuya_uv_exe"; then
            return 0
        fi
    fi

    tuya_resolve_uv || return 1
    tuya_extract_uv || return 1
    tuya_debug "[TuyaOpen] uv ready: $_tuya_uv_exe"
}

tuya_setup_uv() {
    tuya_stage uv
    tuya_load_uv_manifest "$OPEN_SDK_ROOT" || return 1
    tuya_new_uv_context "$OPEN_SDK_ROOT"   || return 1
    tuya_install_uv                        || return 1
    if ! tuya_test_uv_exe "$_tuya_uv_exe"; then
        tuya_error Uv 'uv installation failed.' 'Executable missing or not runnable.' \
            'See manual install above.' 'Re-run: . ./export.sh'
        return 1
    fi
    tuya_path_add "$_tuya_uv_tools_dir"
    OPEN_SDK_UV="$_tuya_uv_exe"
    export OPEN_SDK_UV
    tuya_platform_banner "$OPEN_SDK_ROOT"
    tuya_print_version "$OPEN_SDK_ROOT"
}

# ---------------------------------------------------------------------------
# Python (uv-managed, project-local)
# ---------------------------------------------------------------------------
tuya_python_install_dir() {
    echo "$OPEN_SDK_ROOT/.tools/python/$TUYA_PYTHON_VERSION"
}

tuya_find_managed_python() {
    local install_dir="$1" candidate=""
    [ -d "$install_dir" ] || return 1
    for candidate in "$install_dir"/cpython-*/bin/python3.12 "$install_dir"/cpython-*/bin/python3; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

tuya_test_python_exe() {
    local exe="$1" line=""
    [ -n "$exe" ] && [ -x "$exe" ] || return 1
    line=$("$exe" --version 2>&1 | head -n 1)
    case "$line" in
        *"Python $TUYA_PYTHON_VERSION"*) return 0 ;;
    esac
    return 1
}

tuya_uv() {
    local with_progress=0 saved_link="" rc=0
    if [ "$1" = --with-progress ]; then
        with_progress=1
        shift
    fi
    if [ -z "${TUYAOPEN_EXPORT_VERBOSE:-}" ] && [ "$with_progress" -eq 0 ]; then
        saved_link="${UV_LINK_MODE:-}"
        UV_LINK_MODE="${UV_LINK_MODE:-copy}"
        export UV_LINK_MODE
        UV_NO_PROGRESS=1 "$OPEN_SDK_UV" "$@" --quiet >/dev/null 2>&1 || rc=$?
        if [ -z "$saved_link" ]; then
            unset UV_LINK_MODE
        else
            UV_LINK_MODE="$saved_link"
            export UV_LINK_MODE
        fi
        return "$rc"
    fi
    "$OPEN_SDK_UV" "$@" || rc=$?
    return "$rc"
}

tuya_install_python_ide() {
    local install_dir rc=0
    install_dir=$(tuya_python_install_dir)
    _tuya_py_artifact='cpython'
    _tuya_py_total_mib=0
    _tuya_py_recv_mib=-1
    _tuya_prog_last_text=''
    _tuya_prog_last_at=0
    _tuya_prog_last_pct=-1
    tuya_uv_run_stream tuya_parse_python_install_line \
        python install "$TUYA_PYTHON_VERSION" --install-dir "$install_dir" --no-registry --no-bin || rc=$?
    return "$rc"
}

# Report a Python install failure, explaining the real cause (network vs. other)
# from the captured uv output when available.
tuya_python_install_error() {
    local install_dir="$1" cause='uv python install exited non-zero'
    if [ -n "$_tuya_uv_diag" ]; then
        if tuya_uv_diag_is_network; then
            cause='network error while downloading Python (check connection/proxy; see uv output below)'
        else
            cause='uv python install failed (see uv output below)'
        fi
    fi
    tuya_error Python "Python $TUYA_PYTHON_VERSION installation failed." \
        "$cause" \
        "Run: \"$OPEN_SDK_UV\" python install $TUYA_PYTHON_VERSION --install-dir \"$install_dir\"" \
        'Re-run: . ./export.sh'
    tuya_uv_print_diag
}

# Run one `uv python install` attempt, announcing the source it downloads from
# (so the origin is visible in logs for later diagnosis).
tuya_run_python_install() {
    local install_dir="$1" src="$2" rc=0
    tuya_uv_reset_diag
    tuya_info "[TuyaOpen] Installing Python $TUYA_PYTHON_VERSION from ${src}..."
    if tuya_is_ide_host; then
        tuya_install_python_ide || rc=$?
    else
        tuya_uv --with-progress python install "$TUYA_PYTHON_VERSION" \
            --install-dir "$install_dir" --no-registry --no-bin || rc=$?
    fi
    return "$rc"
}

tuya_install_python() {
    local install_dir rc=0 saved_py_mirror=""
    install_dir=$(tuya_python_install_dir)
    saved_py_mirror="${UV_PYTHON_INSTALL_MIRROR:-}"

    # If the user pinned their own mirror, honor it and don't manage fallback.
    if [ -n "$saved_py_mirror" ]; then
        tuya_debug "[TuyaOpen] Python mirror URL: $saved_py_mirror"
        tuya_run_python_install "$install_dir" 'custom mirror' && return 0
        tuya_python_install_error "$install_dir"
        return 1
    fi

    # In mainland China, try the CN mirror first, then fall back to the default
    # (GitHub) source if it fails — uv itself does not fall back automatically.
    if [ "${_tuya_use_cn_download:-0}" -eq 1 ] && [ -n "${TUYA_PYTHON_INSTALL_MIRROR_CN:-}" ]; then
        UV_PYTHON_INSTALL_MIRROR="$TUYA_PYTHON_INSTALL_MIRROR_CN"
        export UV_PYTHON_INSTALL_MIRROR
        tuya_debug "[TuyaOpen] Python mirror URL: $TUYA_PYTHON_INSTALL_MIRROR_CN"
        tuya_run_python_install "$install_dir" 'npmmirror (CN mirror)'
        rc=$?
        unset UV_PYTHON_INSTALL_MIRROR
        if [ "$rc" -eq 0 ]; then
            return 0
        fi
        tuya_info "[TuyaOpen] CN Python mirror failed (exit ${rc}); falling back to default source (GitHub)..."
    fi

    # Default source (GitHub) — first choice overseas, fallback in CN.
    tuya_run_python_install "$install_dir" 'GitHub (default)' && return 0
    tuya_python_install_error "$install_dir"
    return 1
}

tuya_setup_python() {
    tuya_stage python
    local install_dir python_exe=""
    install_dir=$(tuya_python_install_dir)
    python_exe=$(tuya_find_managed_python "$install_dir")

    if tuya_test_python_exe "$python_exe"; then
        tuya_debug "[TuyaOpen] Python $TUYA_PYTHON_VERSION: $python_exe"
    else
        if [ -n "$python_exe" ] && ! tuya_test_python_exe "$python_exe"; then
            tuya_debug '[TuyaOpen] Existing Python install is invalid; reinstalling.'
            rm -rf "$install_dir" 2>/dev/null || {
                tuya_error Python 'Cannot remove invalid Python install.' "$install_dir" \
                    'Close processes using .tools/python' 'Delete folder manually, then re-run.'
                return 1
            }
            python_exe=""
        fi
        if [ -z "$python_exe" ]; then
            tuya_install_python || return 1
            python_exe=$(tuya_find_managed_python "$install_dir")
        fi
        if ! tuya_test_python_exe "$python_exe"; then
            tuya_error Python 'Python installation incomplete.' \
                "Expected Python $TUYA_PYTHON_VERSION under $install_dir" \
                'Re-run: . ./export.sh'
            return 1
        fi
        tuya_debug "[TuyaOpen] Python $TUYA_PYTHON_VERSION ready: $python_exe"
    fi
    _tuya_managed_python="$python_exe"
}

# ---------------------------------------------------------------------------
# Project .venv (uv sync)
# ---------------------------------------------------------------------------
# Decide the PyPI source for `uv sync`.  Explicit TUYAOPEN_PYPI_MIRROR wins;
# otherwise mainland China auto-uses the Aliyun mirror.  Both plans install
# strictly from uv.lock (--frozen); 'mirror' only changes the index URL.
tuya_uv_sync_plan() {
    case "${TUYAOPEN_PYPI_MIRROR:-}" in
        1)
            echo 'mirror'
            ;;
        0)
            echo 'default'
            ;;
        *)
            if [ "${_tuya_use_cn_download:-0}" -eq 1 ]; then
                echo 'mirror'
            else
                echo 'default'
            fi
            ;;
    esac
}

tuya_lock_pkg_count() {
    local lock="$OPEN_SDK_ROOT/uv.lock" count=0
    [ -f "$lock" ] || { echo 1; return 0; }
    count=$(grep -c '^\[\[package\]\]' "$lock" 2>/dev/null || echo 0)
    [ "$count" -lt 1 ] && count=1
    echo "$count"
}

tuya_sync_deps_ide() {
    local plan="$1" pkg_count="$2" saved_index="" saved_url="" rc=0
    _tuya_sync_current=0
    _tuya_sync_last_name=''
    _tuya_prog_last_text=''
    _tuya_prog_last_at=0
    _tuya_prog_last_pct=-1
    _tuya_sync_pkg_total=$pkg_count
    case "$plan" in
        'mirror')
            saved_index="${UV_DEFAULT_INDEX:-}"
            saved_url="${UV_INDEX_URL:-}"
            UV_DEFAULT_INDEX="$TUYA_ALIYUN_PYPI_INDEX"
            UV_INDEX_URL="$TUYA_ALIYUN_PYPI_INDEX"
            export UV_DEFAULT_INDEX UV_INDEX_URL
            tuya_uv_run_stream tuya_on_uv_sync_line sync --frozen || rc=$?
            if [ -z "$saved_index" ]; then unset UV_DEFAULT_INDEX; else UV_DEFAULT_INDEX="$saved_index"; export UV_DEFAULT_INDEX; fi
            if [ -z "$saved_url" ]; then unset UV_INDEX_URL; else UV_INDEX_URL="$saved_url"; export UV_INDEX_URL; fi
            ;;
        *)
            tuya_uv_run_stream tuya_on_uv_sync_line sync --frozen || rc=$?
            ;;
    esac
    if [ "$rc" -eq 0 ] && [ "$_tuya_sync_current" -lt "$pkg_count" ]; then
        _tuya_sync_current=$pkg_count
        tuya_parse_uv_sync_line "+ done" "$pkg_count"
    fi
    return "$rc"
}

# Report a dependency-sync failure, explaining the real cause (network vs. other)
# from the captured uv output when available.  Emitted here (not in the caller)
# so it runs in the same shell that captured the uv diagnostics.
tuya_sync_deps_error() {
    local cause='uv sync failed.'
    if [ -n "$_tuya_uv_diag" ]; then
        if tuya_uv_diag_is_network; then
            cause='network error while syncing dependencies (check connection/proxy; see uv output below)'
        else
            cause='uv sync failed (see uv output below)'
        fi
    fi
    tuya_error Sync 'Dependency sync failed.' "$cause" \
        'Ensure uv.lock matches pyproject.toml' 'Check network, then re-run: . ./export.sh'
    tuya_uv_print_diag
}

tuya_sync_deps() {
    tuya_stage sync
    local plan saved_index="" saved_url="" rc=0 pkg_count
    tuya_uv_reset_diag
    if command -v pgrep >/dev/null 2>&1 && pgrep -x uv >/dev/null 2>&1; then
        tuya_warn_uv_lock_contention 'Another uv process is already running; dependency sync may pause until it finishes.'
    fi
    plan=$(tuya_uv_sync_plan)
    pkg_count=$(tuya_lock_pkg_count)
    local src='PyPI (default)'
    [ "$plan" = 'mirror' ] && src='Aliyun PyPI mirror (CN)'
    if tuya_is_ide_host; then
        tuya_info "[TuyaOpen] Syncing ${pkg_count} Python dependencies from ${src}..."
        tuya_sync_deps_ide "$plan" "$pkg_count" || rc=$?
        [ "$rc" -ne 0 ] && tuya_sync_deps_error
        return "$rc"
    fi
    tuya_info "[TuyaOpen] Syncing ${pkg_count} Python dependencies from ${src}..."
    case "$plan" in
        'mirror')
            saved_index="${UV_DEFAULT_INDEX:-}"
            saved_url="${UV_INDEX_URL:-}"
            UV_DEFAULT_INDEX="$TUYA_ALIYUN_PYPI_INDEX"
            UV_INDEX_URL="$TUYA_ALIYUN_PYPI_INDEX"
            export UV_DEFAULT_INDEX UV_INDEX_URL
            tuya_uv sync --frozen || rc=$?
            if [ -z "$saved_index" ]; then unset UV_DEFAULT_INDEX; else UV_DEFAULT_INDEX="$saved_index"; export UV_DEFAULT_INDEX; fi
            if [ -z "$saved_url" ]; then unset UV_INDEX_URL; else UV_INDEX_URL="$saved_url"; export UV_INDEX_URL; fi
            ;;
        *)
            tuya_uv sync --frozen || rc=$?
            ;;
    esac
    [ "$rc" -ne 0 ] && tuya_sync_deps_error
    return "$rc"
}

tuya_file_mtime_epoch() {
    # File mtime as epoch seconds: GNU stat (Linux), then BSD stat (macOS).
    stat -c %Y "$1" 2>/dev/null || stat -f %m "$1" 2>/dev/null
}

tuya_is_uv_venv() {
    local venv_path="$1"
    local marker="$venv_path/$TUYA_VENV_MARKER"
    [ -f "$marker" ] && [ -x "$venv_path/bin/python" ]
}

tuya_migrate_legacy_venv() {
    local venv_path="$OPEN_SDK_ROOT/.venv"
    if [ -f "$venv_path" ]; then
        tuya_debug '[TuyaOpen] Removing invalid .venv (not a directory)...'
        rm -rf "$venv_path" 2>/dev/null || {
            tuya_error Venv 'Cannot remove .venv.' 'Path is a file or locked.' \
                'Delete .venv manually' 'Re-run: . ./export.sh'
            return 1
        }
        return 0
    fi
    [ -d "$venv_path" ] || return 0
    if tuya_is_uv_venv "$venv_path"; then
        return 0
    fi
    tuya_info '[TuyaOpen] Detected legacy Python venv (.venv). Migrating to uv-managed environment...'
    tuya_info '           Old .venv removed. A new environment will be created.'
    rm -rf "$venv_path" 2>/dev/null || {
        tuya_error Venv 'Cannot remove .venv.' 'Directory may be in use.' \
            'Close IDE/terminals using .venv' 'Delete folder manually' 'Re-run: . ./export.sh'
        return 1
    }
}

tuya_setup_venv() {
    tuya_stage venv
    local managed_python="${_tuya_managed_python:-}" venv_path="$OPEN_SDK_ROOT/.venv" rc=0
    local venv_py="$venv_path/bin/python" marker="$venv_path/$TUYA_VENV_MARKER"
    local created_venv=0 need_sync=1 lock_mtime='' marker_mtime=''
    tuya_migrate_legacy_venv || return 1

    if ! tuya_is_uv_venv "$venv_path" || [ ! -x "$venv_py" ]; then
        tuya_info '[TuyaOpen] Creating .venv...'
        (
            cd "$OPEN_SDK_ROOT" || exit 1
            tuya_uv venv "$venv_path" --python "$managed_python"
        ) || {
            tuya_error Venv 'Failed to create .venv.' 'uv venv exited non-zero' \
                "Run: \"$OPEN_SDK_UV\" venv .venv --python \"$managed_python\"" \
                'Re-run: . ./export.sh'
            return 1
        }
        printf 'managed-by=export.sh\npython=%s\n' "$TUYA_PYTHON_VERSION" > "$marker" || {
            tuya_error Venv 'Cannot write venv marker.' "$marker" \
                'Check .venv permissions' 'Re-run: . ./export.sh'
            return 1
        }
        created_venv=1
        tuya_debug '[TuyaOpen] .venv created.'
    fi

    # Warm start: skip sync when uv.lock has not changed since the last
    # successful sync (the marker mtime is refreshed after each sync). This
    # keeps re-sourcing fast and avoids re-acquiring the venv lock every time.
    # TUYAOPEN_EXPORT_VERBOSE forces a full sync (self-repair escape hatch).
    if [ "$created_venv" -eq 0 ] && [ -z "${TUYAOPEN_EXPORT_VERBOSE:-}" ]; then
        lock_mtime=$(tuya_file_mtime_epoch "$OPEN_SDK_ROOT/uv.lock") || lock_mtime=''
        marker_mtime=$(tuya_file_mtime_epoch "$marker") || marker_mtime=''
        if [ -n "$lock_mtime" ] && [ -n "$marker_mtime" ] && [ "$lock_mtime" -le "$marker_mtime" ]; then
            need_sync=0
        fi
    fi

    if [ "$need_sync" -eq 1 ]; then
        (
            cd "$OPEN_SDK_ROOT" || exit 1
            tuya_sync_deps
        ) || rc=$?
        if [ "$rc" -ne 0 ]; then
            # tuya_sync_deps already reported the specific cause (incl. uv output).
            return 1
        fi
        tuya_debug '[TuyaOpen] Dependencies synced.'
        # Record the sync time so an unchanged uv.lock can skip sync next source.
        touch "$marker" 2>/dev/null || true
    else
        tuya_stage sync
        tuya_info '[TuyaOpen] Python dependencies up to date (uv.lock unchanged); skipping sync.'
    fi

    if [ ! -x "$venv_py" ]; then
        tuya_error Sync '.venv Python missing after sync.' "$venv_py" \
            'Remove .venv and re-run: . ./export.sh'
        return 1
    fi
    _tuya_venv_py="$venv_py"
}

# ---------------------------------------------------------------------------
# Session helpers
# ---------------------------------------------------------------------------
tuya_is_env_active() {
    if [ "${TUYAOPEN_ENV_ACTIVE:-}" != '1' ]; then
        return 1
    fi
    if [ "${OPEN_SDK_ROOT:-}" != "$1" ]; then
        return 1
    fi
    [ -x "$1/.venv/bin/python" ]
}

tuya_guard_active() {
    # Verbose mode forces full re-initialization even when already active.
    [ -n "${TUYAOPEN_EXPORT_VERBOSE:-}" ] && return 1
    tuya_is_env_active "$OPEN_SDK_ROOT" || return 1
    tuya_info '[TuyaOpen] Environment is already active.'
    tuya_info "To re-activate: deactivate && . ./export.sh"
    return 0
}

tuya_platform_banner() {
    local root="$1" uv_ver=""
    uv_ver=$("$OPEN_SDK_UV" --version 2>/dev/null | awk '{print $2}')
    [ -z "$uv_ver" ] && uv_ver="$_tuya_uv_ver"
    tuya_info "OPEN_SDK_ROOT = $root"
    tuya_info "Host: $(uname -s) $(uname -m) | uv $uv_ver | Python $TUYA_PYTHON_VERSION"
}

tuya_clear_pythonhome() {
    # An inherited PYTHONHOME (conda or another Python distribution active in
    # the launching shell) breaks startup of every python this script and the
    # venv run. Clear it like a standard venv activate does; deactivate
    # restores it.
    if [ -n "${PYTHONHOME:-}" ]; then
        _OLD_TUYA_PYTHONHOME="$PYTHONHOME"
        unset PYTHONHOME
        tuya_debug '[TuyaOpen] Cleared inherited PYTHONHOME (deactivate restores it).'
    fi
}

tuya_set_env() {
    local venv_py="${_tuya_venv_py:-}"
    local venv_path="$OPEN_SDK_ROOT/.venv"
    local bin_dir="$venv_path/bin"
    VIRTUAL_ENV="$venv_path"
    OPEN_SDK_PYTHON="$venv_py"
    OPEN_SDK_PIP="$bin_dir/pip"
    OPEN_SDK_ROOT="$OPEN_SDK_ROOT"
    TUYAOPEN_ENV_ACTIVE=1
    export VIRTUAL_ENV OPEN_SDK_PYTHON OPEN_SDK_PIP OPEN_SDK_ROOT TUYAOPEN_ENV_ACTIVE
    tuya_path_add "$bin_dir"
    tuya_path_add "$OPEN_SDK_ROOT"
}

tuya_reset_cache() {
    local cache="$OPEN_SDK_ROOT/.cache"
    tuya_ensure_dir "$cache" || return 0
    rm -f "$cache/.env.json" "$cache/.dont_prompt_update_platform" 2>/dev/null || true
}

tuya_install_prompt() {
    # Starship and other dynamic prompt frameworks manage the prompt themselves
    # via precmd hooks and detect VIRTUAL_ENV automatically — skip manual prefix.
    tuya_has_cmd starship && return 0
    [ -n "${STARSHIP_SHELL:-}" ] && return 0
    [ -n "${POWERLEVEL9K_MODE:-}" ] && return 0
    if [ -n "${BASH_VERSION:-}" ]; then
        if [ -z "${_OLD_TUYA_PS1:-}" ] && [ -n "${PS1:-}" ]; then
            case "$PS1" in
                *"${TUYA_PROMPT_PREFIX}"*) ;;
                *) _OLD_TUYA_PS1="$PS1" ;;
            esac
        fi
        if [ -n "${_OLD_TUYA_PS1:-}" ] || [ -n "${PS1:-}" ]; then
            PS1="${TUYA_PROMPT_PREFIX}${_OLD_TUYA_PS1:-$PS1}"
        fi
    elif [ -n "${ZSH_VERSION:-}" ]; then
        if [ -z "${_OLD_TUYA_PROMPT:-}" ] && [ -n "${PROMPT:-}" ]; then
            case "$PROMPT" in
                *"${TUYA_PROMPT_PREFIX}"*) ;;
                *) _OLD_TUYA_PROMPT="$PROMPT" ;;
            esac
        fi
        if [ -n "${_OLD_TUYA_PROMPT:-}" ] || [ -n "${PROMPT:-}" ]; then
            PROMPT="${TUYA_PROMPT_PREFIX}${_OLD_TUYA_PROMPT:-$PROMPT}"
        fi
    fi
}

tuya_install_completion() {
    if [ -n "${BASH_VERSION:-}" ]; then
        eval "$(_TOS_PY_COMPLETE=bash_source "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" 2>/dev/null)" || true
    elif [ -n "${ZSH_VERSION:-}" ]; then
        eval "$(_TOS_PY_COMPLETE=zsh_source "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" 2>/dev/null)" || true
    fi
}

tuya_teardown() {
    local silent=0 sdk_root="${OPEN_SDK_ROOT:-}" venv_bin="" uv_dir="" uv_ver=""
    if [ "${1:-}" = '--silent' ]; then
        silent=1
    fi
    if [ -n "$sdk_root" ]; then
        venv_bin="$sdk_root/.venv/bin"
        if [ -f "$sdk_root/uv-manifest.env" ]; then
            uv_ver=$(grep -E '^UV_VERSION=' "$sdk_root/uv-manifest.env" 2>/dev/null | head -n1 | cut -d= -f2-)
        fi
        uv_ver="${uv_ver:-$TUYA_UV_VERSION}"
        uv_dir="$sdk_root/.tools/uv/$uv_ver"
        tuya_path_remove "$sdk_root"
        tuya_path_remove "$venv_bin"
        tuya_path_remove "$uv_dir"
    fi
    unset VIRTUAL_ENV OPEN_SDK_ROOT OPEN_SDK_PYTHON OPEN_SDK_PIP OPEN_SDK_UV OPEN_SDK_MAKE_BIN OPEN_SDK_MAKE TUYAOPEN_ENV_ACTIVE
    if [ -n "${_OLD_TUYA_PYTHONHOME:-}" ]; then
        PYTHONHOME="$_OLD_TUYA_PYTHONHOME"
        export PYTHONHOME
        unset _OLD_TUYA_PYTHONHOME
    fi
    if [ -n "${BASH_VERSION:-}" ] && [ -n "${_OLD_TUYA_PS1:-}" ]; then
        PS1="$_OLD_TUYA_PS1"
        unset _OLD_TUYA_PS1
    elif [ -n "${ZSH_VERSION:-}" ] && [ -n "${_OLD_TUYA_PROMPT:-}" ]; then
        PROMPT="$_OLD_TUYA_PROMPT"
        unset _OLD_TUYA_PROMPT
    fi
    unset -f deactivate 2>/dev/null || true
    unset -f exit 2>/dev/null || true
    if [ "$silent" -eq 0 ]; then
        tuya_info 'TuyaOpen environment deactivated. Re-enter: . ./export.sh'
    fi
}

deactivate() {
    tuya_teardown
}

exit() {
    if [ -n "${OPEN_SDK_ROOT:-}" ]; then
        echo 'Exiting TuyaOpen environment...'
        tuya_teardown --silent
        echo 'TuyaOpen environment deactivated.'
    fi
    command exit "$@"
}

tuya_invoke_hello() {
    if [ -n "${TUYAOPEN_EXPORT_VERBOSE:-}" ]; then
        "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" hello --no-version
    else
        "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" hello --no-version 2>/dev/null
    fi
}

# ---------------------------------------------------------------------------
# Finalize
# ---------------------------------------------------------------------------
tuya_finalize() {
    local prepare_rc=0
    tuya_stage prepare
    "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" prepare || prepare_rc=$?
    if [ "$prepare_rc" -ne 0 ]; then
        tuya_info '[TuyaOpen] Warning: tos.py prepare failed. Retry: tos.py prepare'
    fi
    tuya_install_completion
    tuya_install_prompt
    tuya_reset_cache
    tuya_invoke_hello
    tuya_stage ready
    tuya_info '[TuyaOpen] Ready - tos.py available. Exit: deactivate'
}

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------
if [ "${TUYAOPEN_EXPORT_SKIP_MAIN:-}" = '1' ]; then
    return 0 2>/dev/null || exit 0
fi

if [ -n "${BASH_SOURCE[0]:-}" ] && [ "${BASH_SOURCE[0]}" = "$0" ]; then
    tuya_info '[TuyaOpen] Tip: dot-source this script: . ./export.sh'
fi

tuya_guard_active   && { tuya_cleanup; return 0; }
tuya_clear_pythonhome
tuya_write_cold_start_hint "$(tuya_export_cold_start_kind)"
tuya_detect_region
tuya_check_git      || { tuya_cleanup; return 1; }
tuya_setup_uv       || { tuya_cleanup; return 1; }
tuya_setup_python   || { tuya_cleanup; return 1; }
tuya_setup_venv     || { tuya_cleanup; return 1; }
tuya_set_env
tuya_finalize
tuya_cleanup
