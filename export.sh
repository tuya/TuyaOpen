#!/usr/bin/env bash
#
# Usage: . ./export.sh
#
# Set TUYAOPEN_EXPORT_VERBOSE=1 before sourcing for full diagnostic output.
#
# This script must be *sourced* (not executed). It:
#   * locates the TuyaOpen project root,
#   * creates/activates a Python venv in <root>/.venv,
#   * installs requirements.txt,
#   * exports OPEN_SDK_ROOT / OPEN_SDK_PYTHON / OPEN_SDK_PIP,
#   * adds the project root to PATH so `tos.py` is runnable,
#   * installs a friendly `exit` override that tears the env down cleanly.

# ---------------------------------------------------------------------------
# Locate this script (works under bash, zsh, and plain POSIX sh)
# ---------------------------------------------------------------------------
if [ -n "$BASH_VERSION" ]; then
    __tuya_script_dir=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
elif [ -n "$ZSH_VERSION" ]; then
    __tuya_script_dir=$(realpath "$(dirname "${(%):-%x}")")
else
    __tuya_script_dir=$(realpath "$(dirname "$0")")
fi
__tuya_pwd_dir="$(pwd)"

# ---------------------------------------------------------------------------
# Logging helpers
#   __tuya_info  : always printed
#   __tuya_debug : printed only when TUYAOPEN_EXPORT_VERBOSE is set
# ---------------------------------------------------------------------------
__tuya_info()  { echo "$@"; }
__tuya_debug() { [ -n "$TUYAOPEN_EXPORT_VERBOSE" ] && echo "$@"; return 0; }

# Unset every helper/temporary we introduce (used on both error and success).
__tuya_export_cleanup() {
    unset __tuya_script_dir __tuya_pwd_dir
    unset -f __tuya_info __tuya_debug __tuya_is_sdk_root \
             __tuya_print_version __tuya_find_python \
             __tuya_export_cleanup 2>/dev/null || true
}

# ---------------------------------------------------------------------------
# Project root detection
# Prefer $PWD when it looks like a TuyaOpen checkout (so a copy of export.sh
# sourced from another tree still picks the right root), then fall back to
# the directory of this script.
# ---------------------------------------------------------------------------
__tuya_is_sdk_root() {
    [ -f "$1/export.sh" ] && [ -f "$1/requirements.txt" ] && [ -f "$1/tos.py" ]
}

if __tuya_is_sdk_root "$__tuya_pwd_dir"; then
    OPEN_SDK_ROOT="$__tuya_pwd_dir"
elif __tuya_is_sdk_root "$__tuya_script_dir"; then
    OPEN_SDK_ROOT="$__tuya_script_dir"
else
    echo "Error: Unable to locate TuyaOpen project root (export.sh + requirements.txt)."
    __tuya_export_cleanup
    return 1
fi

# ---------------------------------------------------------------------------
# Git version string: exact tag, else <tag>-<8-char-sha>, plus -dirty suffix.
# ---------------------------------------------------------------------------
__tuya_print_version() {
    local root="$1" ver="" tag="" short="" dirty=""
    if ! command -v git >/dev/null 2>&1; then
        echo "TuyaOpen version: (git not found)"
        return 0
    fi
    if [ ! -e "$root/.git" ]; then
        echo "TuyaOpen version: (not a git checkout)"
        return 0
    fi
    # Tolerate failures under `set -e`: a shallow clone with no reachable tags
    # makes `git describe` exit 128, which would otherwise abort the caller
    # (e.g. CI running `bash -e`).
    local status_out=""
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
# Verify required project files (silent on success; only report what's missing)
# ---------------------------------------------------------------------------
__tuya_missing=""
for __f in export.sh requirements.txt tos.py; do
    if [ ! -f "$OPEN_SDK_ROOT/$__f" ]; then
        __tuya_missing="$__tuya_missing $__f"
    fi
done
unset __f
if [ -n "$__tuya_missing" ]; then
    echo "Error: Missing required file(s) in $OPEN_SDK_ROOT:$__tuya_missing"
    unset __tuya_missing
    __tuya_export_cleanup
    return 1
fi
unset __tuya_missing

# ---------------------------------------------------------------------------
# Locate a usable Python: 3.9 - 3.13 are officially supported (3.11 recommended).
# Any Python 3.x is accepted with a warning (e.g. 3.14) instead of blocking.
# ---------------------------------------------------------------------------
__TUYA_PY_MIN="3.9"
__TUYA_PY_MAX="3.13"
__TUYA_PY_REC="3.11"

__tuya_find_python_supported() {
    local cmd
    for cmd in python3.11 python3.12 python3.10 python3.13 python3.9 python3 python; do
        command -v "$cmd" >/dev/null 2>&1 || continue
        if "$cmd" -c '
import sys
major, minor = sys.version_info[:2]
sys.exit(0 if (major, minor) >= (3, 9) and (major, minor) <= (3, 13) else 1)
' 2>/dev/null; then
            echo "$cmd"
            return 0
        fi
    done
    return 1
}

__tuya_find_python_any() {
    local cmd
    for cmd in python3 python; do
        command -v "$cmd" >/dev/null 2>&1 || continue
        if "$cmd" -c 'import sys; sys.exit(0 if sys.version_info[0] >= 3 else 1)' 2>/dev/null; then
            echo "$cmd"
            return 0
        fi
    done
    return 1
}

PYTHON_CMD=$(__tuya_find_python_supported)
if [ -z "$PYTHON_CMD" ]; then
    PYTHON_CMD=$(__tuya_find_python_any)
fi
if [ -z "$PYTHON_CMD" ]; then
    echo "Error: No usable Python interpreter found!"
    echo "       Please install Python ${__TUYA_PY_MIN} - ${__TUYA_PY_MAX} (${__TUYA_PY_REC} recommended; all in range work)."
    unset __TUYA_PY_MIN __TUYA_PY_MAX __TUYA_PY_REC
    __tuya_export_cleanup
    return 1
fi
__tuya_py_major=$("$PYTHON_CMD" -c 'import sys; print(sys.version_info[0])' 2>/dev/null)
__tuya_py_minor=$("$PYTHON_CMD" -c 'import sys; print(sys.version_info[1])' 2>/dev/null)
__tuya_py_in_range=0
if [ "$__tuya_py_major" = "3" ] && [ "${__tuya_py_minor:-0}" -ge 9 ] 2>/dev/null && [ "${__tuya_py_minor:-0}" -le 13 ] 2>/dev/null; then
    __tuya_py_in_range=1
fi

# Detect re-source: this project's venv is already active.
__tuya_is_resource=0
if [ -n "$VIRTUAL_ENV" ] && [ "$VIRTUAL_ENV" = "$OPEN_SDK_ROOT/.venv" ]; then
    __tuya_is_resource=1
fi

# ---------------------------------------------------------------------------
# Summary banner
#   Re-source: show only the "already active" note, then the greeting banner.
#   First source / verbose: full summary as before.
# ---------------------------------------------------------------------------
if [ "$__tuya_is_resource" = "1" ] && [ -z "$TUYAOPEN_EXPORT_VERBOSE" ]; then
    __tuya_info "[TuyaOpen] Note: Virtual environment is already active ($VIRTUAL_ENV); refreshing environment variables."
else
    __tuya_info  "OPEN_SDK_ROOT = $OPEN_SDK_ROOT"
    __tuya_debug "Current root  = $__tuya_pwd_dir"
    __tuya_debug "Script path   = $__tuya_script_dir"
    __tuya_info  "Host: $(uname -s) $(uname -m) | $PYTHON_CMD $("$PYTHON_CMD" -c 'import sys; print(".".join(map(str, sys.version_info[:3])))' 2>/dev/null)"

    if [ "$__tuya_is_resource" = "1" ]; then
        __tuya_debug "Virtual environment already active: $VIRTUAL_ENV (re-sourcing export.sh)"
    fi

    __tuya_print_version "$OPEN_SDK_ROOT"

    if [ "$__tuya_py_in_range" != "1" ]; then
        __tuya_info "[TuyaOpen] Warning: Python ${__tuya_py_major}.${__tuya_py_minor} is outside the tested range ${__TUYA_PY_MIN} - ${__TUYA_PY_MAX}; continuing anyway (${__TUYA_PY_REC} recommended)."
    elif [ "$__tuya_py_minor" != "11" ]; then
        __tuya_info "[TuyaOpen] Note: Python ${__TUYA_PY_REC} is recommended; ${__TUYA_PY_MIN}-${__TUYA_PY_MAX} are supported (detected 3.${__tuya_py_minor})."
    fi
fi
unset __tuya_py_major __tuya_py_minor __tuya_py_in_range __TUYA_PY_MIN __TUYA_PY_MAX __TUYA_PY_REC

# ---------------------------------------------------------------------------
# Work inside project root so relative paths resolve
# ---------------------------------------------------------------------------
cd "$OPEN_SDK_ROOT" || { __tuya_export_cleanup; return 1; }

# ---------------------------------------------------------------------------
# Create / reuse virtualenv
# ---------------------------------------------------------------------------
if [ ! -d "$OPEN_SDK_ROOT/.venv" ]; then
    echo "Creating virtual environment..."
    if ! "$PYTHON_CMD" -m venv "$OPEN_SDK_ROOT/.venv"; then
        echo "Error: Failed to create virtual environment!"
        echo "Please check your Python installation and try again."
        __tuya_export_cleanup
        return 1
    fi
    echo "Virtual environment created successfully."
else
    __tuya_debug "Virtual environment already exists."
fi

if [ ! -f "$OPEN_SDK_ROOT/.venv/bin/activate" ]; then
    echo "Error: Virtual environment activation script not found at $OPEN_SDK_ROOT/.venv/bin/activate"
    __tuya_export_cleanup
    return 1
fi

# ---------------------------------------------------------------------------
# Custom `exit` that cleanly leaves the TuyaOpen environment before quitting.
# Always falls through to the real `exit` so the shell actually terminates and
# the user-supplied exit code is preserved.
#
# IMPORTANT: do NOT `export -f exit`. Exporting this function would leak it
# into every child bash process (CI scripts, build scripts, `bash -c ...`),
# where `exit N` would hit our wrapper instead of the builtin, silently
# dropping the exit code and breaking error propagation.
# ---------------------------------------------------------------------------
exit() {
    if [ -n "$OPEN_SDK_ROOT" ]; then
        echo "Exiting TuyaOpen environment..."
        if type deactivate >/dev/null 2>&1; then
            deactivate
        fi
        unset OPEN_SDK_PYTHON OPEN_SDK_PIP OPEN_SDK_ROOT
        unset -f exit 2>/dev/null || true
        echo "TuyaOpen environment deactivated."
    fi
    command exit "$@"
}

# ---------------------------------------------------------------------------
# Activate venv and export variables
# ---------------------------------------------------------------------------
__tuya_debug "Activating virtual environment from $OPEN_SDK_ROOT/.venv/bin/activate"
# shellcheck disable=SC1091
. "$OPEN_SDK_ROOT/.venv/bin/activate"

# Append project root to PATH only if not already present (idempotent).
case ":$PATH:" in
    *":$OPEN_SDK_ROOT:"*) ;;
    *) PATH="$PATH:$OPEN_SDK_ROOT" ;;
esac
export PATH
export OPEN_SDK_PYTHON="$OPEN_SDK_ROOT/.venv/bin/python"
export OPEN_SDK_PIP="$OPEN_SDK_ROOT/.venv/bin/pip"
export OPEN_SDK_ROOT

# Intentionally do NOT export the `exit` function (see note above its
# definition). Child bash processes must keep the builtin `exit` semantics.

if [ -z "$VIRTUAL_ENV" ]; then
    echo "Error: Failed to activate virtual environment"
    __tuya_export_cleanup
    return 1
fi
__tuya_debug "Virtual environment activated successfully: $VIRTUAL_ENV"

# ---------------------------------------------------------------------------
# Install dependencies
# ---------------------------------------------------------------------------
__tuya_pip_rc=0
if [ -n "$TUYAOPEN_EXPORT_VERBOSE" ]; then
    pip install -r "$OPEN_SDK_ROOT/requirements.txt" || __tuya_pip_rc=$?
else
    pip install -q -r "$OPEN_SDK_ROOT/requirements.txt" || __tuya_pip_rc=$?
fi
if [ "$__tuya_pip_rc" -ne 0 ]; then
    echo "[TuyaOpen] Warning: Some dependencies may not have been installed correctly."
fi
unset __tuya_pip_rc

# ---------------------------------------------------------------------------
# Clean stale cache files
# ---------------------------------------------------------------------------
CACHE_PATH="$OPEN_SDK_ROOT/.cache"
mkdir -p "$CACHE_PATH"
rm -f "$CACHE_PATH/.env.json" "$CACHE_PATH/.dont_prompt_update_platform"

# ---------------------------------------------------------------------------
# tos.py shell completion (bash only; zsh does not implement `complete`).
# Swallow errors so completion-generation failures never break sourcing.
# ---------------------------------------------------------------------------
if [ -n "$BASH_VERSION" ]; then
    eval "$(_TOS_PY_COMPLETE=bash_source tos.py 2>/dev/null)" || true
fi

# ---------------------------------------------------------------------------
# Greeting banner — ASCII/version from `tos.py hello`; "ready" + exit hint
# from this script so shells can show accurate wording (see export.ps1).
# On re-source (and non-verbose) the tos.py "Running tos.py ..." INFO log on
# stderr would be noise, so we drop stderr in that case.
# ---------------------------------------------------------------------------
if [ "$__tuya_is_resource" = "1" ] && [ -z "$TUYAOPEN_EXPORT_VERBOSE" ]; then
    tos.py hello --no-version 2>/dev/null
else
    tos.py hello --no-version
fi
echo 'tos.py Tool and TuyaOpen SDK is now ready.'
echo 'Exit environment: `exit` or `deactivate`.'
unset __tuya_is_resource

__tuya_export_cleanup
