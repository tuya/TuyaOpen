#!/usr/bin/env bash
#
# Usage: . ./export.sh
#
# Set TUYAOPEN_EXPORT_VERBOSE=1 before sourcing for full diagnostic output.
# Set TUYAOPEN_EXPORT_SKIP_MAIN=1 to load functions only (tests).
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
TUYA_UV_BASE_URL='https://releases.astral.sh/github/uv/releases/download'
TUYA_PYTHON_VERSION='3.12.13'
TUYA_VENV_MARKER='.tuyaopen-uv'
TUYA_UV_DOWNLOAD_ATTEMPTS=2
TUYA_ALIYUN_PYPI_INDEX='https://mirrors.aliyun.com/pypi/simple/'
TUYA_PROMPT_PREFIX='(TuyaOpen) '

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
    unset _tuya_uv_ver _tuya_uv_triple _tuya_uv_artifact _tuya_uv_url_astral _tuya_uv_url_github
    unset _tuya_uv_dl_size _tuya_uv_dl_sha256 _tuya_uv_tools_dir
    unset _tuya_uv_archive _tuya_uv_exe
    unset _tuya_managed_python _tuya_venv_py
    unset -f tuya_debug tuya_error \
             tuya_is_sdk_root tuya_print_version tuya_has_cmd \
             tuya_ensure_dir tuya_path_add \
             tuya_triple_manifest_key tuya_load_uv_manifest \
             tuya_get_uv_artifact_check tuya_get_release_urls \
             tuya_get_arch tuya_select_uv_artifact \
             tuya_check_glibc tuya_download_file tuya_verify_sha256 \
             tuya_test_uv_exe tuya_new_uv_context \
             tuya_resolve_uv tuya_download_uv \
             tuya_extract_uv tuya_install_uv tuya_setup_uv \
             tuya_python_install_dir tuya_find_managed_python \
             tuya_test_python_exe tuya_install_python \
             tuya_setup_python tuya_uv_sync_plan tuya_uv \
             tuya_lock_pkg_count tuya_sync_deps \
             tuya_is_uv_venv tuya_migrate_legacy_venv \
             tuya_setup_venv tuya_is_env_active \
             tuya_set_env tuya_reset_cache \
             tuya_install_prompt tuya_install_completion \
             tuya_register_helpers tuya_invoke_hello \
             tuya_platform_banner tuya_guard_active tuya_finalize \
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

tuya_load_uv_manifest() {
    local root="$1"
    local env_file="$root/uv-manifest.env"
    _tuya_uv_ver="$TUYA_UV_VERSION"
    _tuya_uv_url_astral="$TUYA_UV_BASE_URL"
    _tuya_uv_url_github='https://github.com/astral-sh/uv/releases/download'

    if [ -f "$env_file" ]; then
        while IFS= read -r line || [ -n "$line" ]; do
            case "$line" in
                ''|\#*) continue ;;
                UV_VERSION=*) _tuya_uv_ver="${line#UV_VERSION=}" ;;
                UV_DOWNLOAD_SOURCE_ASTRAL=*) _tuya_uv_url_astral="${line#UV_DOWNLOAD_SOURCE_ASTRAL=}" ;;
                UV_DOWNLOAD_SOURCE_GITHUB=*) _tuya_uv_url_github="${line#UV_DOWNLOAD_SOURCE_GITHUB=}" ;;
                UV_*_SHA256=*)
                    local key="${line%%_SHA256=*}"
                    key="${key#UV_}"
                    eval "_tuya_uv_sha256_${key}=\"${line#*=}\""
                    ;;
                UV_*_SIZE=*)
                    local key="${line%%_SIZE=*}"
                    key="${key#UV_}"
                    eval "_tuya_uv_size_${key}=\"${line#*=}\""
                    ;;
            esac
        done < "$env_file"
    fi
    _tuya_uv_ver="${_tuya_uv_ver#"${_tuya_uv_ver%%[![:space:]]*}"}"
    _tuya_uv_ver="${_tuya_uv_ver%"${_tuya_uv_ver##*[![:space:]]}"}"
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
    echo "${_tuya_uv_url_astral}/$version"
    echo "${_tuya_uv_url_github}/$version"
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
tuya_download_file() {
    local url="$1" dest="$2" token="${UV_GITHUB_TOKEN:-}"
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

tuya_download_uv() {
    local base url attempt mirror=0 rc=1
    for base in $(tuya_get_release_urls "$_tuya_uv_ver"); do
        mirror=$((mirror + 1))
        url="${base%/}/$_tuya_uv_artifact"
        if [ "$mirror" -gt 1 ]; then
            tuya_debug "[TuyaOpen] Mirror $mirror: $url"
        else
            tuya_debug "[TuyaOpen] Download: $url"
        fi
        attempt=1
        while [ "$attempt" -le "$TUYA_UV_DOWNLOAD_ATTEMPTS" ]; do
            [ "$attempt" -gt 1 ] && tuya_debug "[TuyaOpen] Retry $attempt/$TUYA_UV_DOWNLOAD_ATTEMPTS..."
            rm -f "$_tuya_uv_archive" 2>/dev/null || true
            if tuya_download_file "$url" "$_tuya_uv_archive"; then
                rc=0
                break 2
            fi
            attempt=$((attempt + 1))
        done
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

tuya_install_python() {
    local install_dir
    install_dir=$(tuya_python_install_dir)
    tuya_info "[TuyaOpen] Installing Python $TUYA_PYTHON_VERSION..."
    tuya_uv --with-progress python install "$TUYA_PYTHON_VERSION" \
        --install-dir "$install_dir" --no-registry --no-bin || {
        tuya_error Python "Python $TUYA_PYTHON_VERSION installation failed." \
            "uv python install exited non-zero" \
            "Run: \"$OPEN_SDK_UV\" python install $TUYA_PYTHON_VERSION --install-dir \"$install_dir\"" \
            'Re-run: . ./export.sh'
        return 1
    }
}

tuya_setup_python() {
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
tuya_uv_sync_plan() {
    case "${TUYAOPEN_PYPI_MIRROR:-}" in
        1)
            echo 'sync mirror'
            ;;
        0)
            echo 'sync --frozen'
            ;;
        *)
            echo 'sync --frozen'
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

tuya_sync_deps() {
    local plan saved_index="" saved_url="" rc=0 pkg_count
    plan=$(tuya_uv_sync_plan)
    pkg_count=$(tuya_lock_pkg_count)
    tuya_info "[TuyaOpen] Syncing ${pkg_count} Python dependencies..."
    case "$plan" in
        'sync mirror')
            tuya_debug "[TuyaOpen] Dependency sync: Aliyun mirror."
            saved_index="${UV_DEFAULT_INDEX:-}"
            saved_url="${UV_INDEX_URL:-}"
            UV_DEFAULT_INDEX="$TUYA_ALIYUN_PYPI_INDEX"
            UV_INDEX_URL="$TUYA_ALIYUN_PYPI_INDEX"
            export UV_DEFAULT_INDEX UV_INDEX_URL
            tuya_uv sync || rc=$?
            if [ -z "$saved_index" ]; then unset UV_DEFAULT_INDEX; else UV_DEFAULT_INDEX="$saved_index"; export UV_DEFAULT_INDEX; fi
            if [ -z "$saved_url" ]; then unset UV_INDEX_URL; else UV_INDEX_URL="$saved_url"; export UV_INDEX_URL; fi
            ;;
        *)
            tuya_debug '[TuyaOpen] Dependency sync: PyPI lock (--frozen).'
            tuya_uv sync --frozen || rc=$?
            ;;
    esac
    return "$rc"
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
    local managed_python="${_tuya_managed_python:-}" venv_path="$OPEN_SDK_ROOT/.venv" marker="" rc=0
    local venv_py="$venv_path/bin/python"
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
        marker="$venv_path/$TUYA_VENV_MARKER"
        printf 'managed-by=export.sh\npython=%s\n' "$TUYA_PYTHON_VERSION" > "$marker" || {
            tuya_error Venv 'Cannot write venv marker.' "$marker" \
                'Check .venv permissions' 'Re-run: . ./export.sh'
            return 1
        }
        tuya_debug '[TuyaOpen] .venv created.'
    fi

    (
        cd "$OPEN_SDK_ROOT" || exit 1
        tuya_sync_deps
    ) || rc=$?
    if [ "$rc" -ne 0 ]; then
        tuya_error Sync 'Dependency sync failed.' 'uv sync --frozen failed.' \
            'Ensure uv.lock matches pyproject.toml' 'Check network, then re-run: . ./export.sh'
        return 1
    fi
    tuya_debug '[TuyaOpen] Dependencies synced.'

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
    "$OPEN_SDK_PYTHON" "$OPEN_SDK_ROOT/tos.py" prepare || prepare_rc=$?
    if [ "$prepare_rc" -ne 0 ]; then
        tuya_info '[TuyaOpen] Warning: tos.py prepare failed. Retry: tos.py prepare'
    fi
    tuya_install_completion
    tuya_install_prompt
    tuya_reset_cache
    tuya_invoke_hello
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
tuya_setup_uv       || { tuya_cleanup; return 1; }
tuya_setup_python   || { tuya_cleanup; return 1; }
tuya_setup_venv     || { tuya_cleanup; return 1; }
tuya_set_env
tuya_finalize
tuya_cleanup
