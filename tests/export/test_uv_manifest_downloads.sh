#!/usr/bin/env bash
# Verify uv-manifest.env: download each artifact from CN / Astral / GitHub mirrors
# and check file size + SHA256 against manifest metadata.
#
# Usage:
#   bash tests/export/test_uv_manifest_downloads.sh
#   bash tests/export/test_uv_manifest_downloads.sh --source cn
#   bash tests/export/test_uv_manifest_downloads.sh --triple x86_64-unknown-linux-gnu
#   bash tests/export/test_uv_manifest_downloads.sh --keep   # retain files under .cache/
#
# Requires: curl, sha256sum (or shasum -a 256 on macOS).
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MANIFEST="${ROOT}/uv-manifest.env"
CACHE_ROOT="${ROOT}/.cache/uv-manifest-download-test"
TMP_DIR=""

SOURCE_FILTER='all'
TRIPLE_FILTER=''
KEEP=0
PASS=0
FAIL=0
SKIP=0

usage() {
    cat <<'EOF'
Usage: bash tests/export/test_uv_manifest_downloads.sh [options]

Options:
  --source <cn|astral|github|all>   Mirrors to test (default: all)
  --triple <name>                   Only test one triple, e.g. x86_64-unknown-linux-gnu
  --keep                            Keep downloaded files under .cache/uv-manifest-download-test/
  -h, --help                        Show this help

Downloads every artifact listed in uv-manifest.env (SIZE + SHA256) from:
  - CN:    UV_<TRIPLE>_DOWNLOAD_CN
  - Astral UV_DOWNLOAD_SOURCE_ASTRAL/<version>/<artifact>
  - GitHub UV_DOWNLOAD_SOURCE_GITHUB/<version>/<artifact>
EOF
}

log_pass() { PASS=$((PASS + 1)); printf '  PASS  %s\n' "$1"; }
log_fail() { FAIL=$((FAIL + 1)); printf '  FAIL  %s\n' "$1"; [ -n "${2:-}" ] && printf '        %s\n' "$2"; }
log_skip() { SKIP=$((SKIP + 1)); printf '  SKIP  %s\n' "$1"; [ -n "${2:-}" ] && printf '        %s\n' "$2"; }

format_download_elapsed() {
    local start="$1" end='' elapsed=''
    end=$(date +%s.%N 2>/dev/null || date +%s)
    elapsed=$(awk -v s="$start" -v e="$end" 'BEGIN {
        diff = e - s
        if (diff < 0) diff = 0
        if (diff >= 60) {
            mins = int(diff / 60)
            secs = diff - mins * 60
            printf "%dm%.1fs", mins, secs
        } else {
            printf "%.1fs", diff
        }
    }')
    printf '%s' "$elapsed"
}

section() { printf '\n== %s ==\n' "$1"; }

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source)
            SOURCE_FILTER="${2:-}"
            shift 2
            ;;
        --triple)
            TRIPLE_FILTER="${2:-}"
            shift 2
            ;;
        --keep)
            KEEP=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$SOURCE_FILTER" in
    cn|astral|github|all) ;;
    *)
        printf 'Invalid --source: %s\n' "$SOURCE_FILTER" >&2
        exit 2
        ;;
esac

if ! command -v curl >/dev/null 2>&1; then
    printf 'curl is required.\n' >&2
    exit 1
fi

sha256_file() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum -b "$file" | awk '{print $1}'
        return 0
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
        return 0
    fi
    return 1
}

if ! sha256_file /dev/null >/dev/null 2>&1; then
    printf 'sha256sum or shasum is required.\n' >&2
    exit 1
fi

manifest_key_to_triple() {
    local lower cpu rest
    lower=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$lower" in
        x86_64_*) cpu='x86_64'; rest="${lower#x86_64_}" ;;
        aarch64_*) cpu='aarch64'; rest="${lower#aarch64_}" ;;
        i686_*) cpu='i686'; rest="${lower#i686_}" ;;
        riscv64gc_*) cpu='riscv64gc'; rest="${lower#riscv64gc_}" ;;
        *)
            return 1
            ;;
    esac
    rest=$(echo "$rest" | tr '_' '-')
    printf '%s-%s' "$cpu" "$rest"
}

manifest_key_to_artifact() {
    local key="$1" ext triple
    triple=$(manifest_key_to_triple "$key")
    if [[ "$key" == *PC_WINDOWS_MSVC ]]; then
        ext='zip'
    else
        ext='tar.gz'
    fi
    printf 'uv-%s.%s' "$triple" "$ext"
}

source_enabled() {
    case "$SOURCE_FILTER" in
        all) return 0 ;;
        "$1") return 0 ;;
        *) return 1 ;;
    esac
}

cleanup() {
    if [ "$KEEP" -eq 0 ] && [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

if [ "$KEEP" -eq 1 ]; then
    mkdir -p "$CACHE_ROOT"
    TMP_DIR="$CACHE_ROOT"
else
    TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/tuya_uv_manifest_test.XXXXXX")
fi

if [ ! -f "$MANIFEST" ]; then
    printf 'Missing manifest: %s\n' "$MANIFEST" >&2
    exit 1
fi

UV_VERSION=''
UV_ASTRAL=''
UV_GITHUB=''
declare -A ART_SIZE=()
declare -A ART_SHA=()
declare -A ART_CN_URL=()
declare -a ART_KEYS=()

while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        ''|\#*) continue ;;
        UV_VERSION=*) UV_VERSION="${line#UV_VERSION=}" ;;
        UV_DOWNLOAD_SOURCE_ASTRAL=*) UV_ASTRAL="${line#UV_DOWNLOAD_SOURCE_ASTRAL=}" ;;
        UV_DOWNLOAD_SOURCE_GITHUB=*) UV_GITHUB="${line#UV_DOWNLOAD_SOURCE_GITHUB=}" ;;
        UV_*_DOWNLOAD_CN=*)
            key="${line%%_DOWNLOAD_CN=*}"
            key="${key#UV_}"
            ART_CN_URL[$key]="${line#*=}"
            ;;
        UV_*_SHA256=*)
            key="${line%%_SHA256=*}"
            key="${key#UV_}"
            ART_SHA[$key]="${line#*=}"
            ;;
        UV_*_SIZE=*)
            key="${line%%_SIZE=*}"
            key="${key#UV_}"
            ART_SIZE[$key]="${line#*=}"
            ART_KEYS+=("$key")
            ;;
    esac
done < "$MANIFEST"

if [ -z "$UV_VERSION" ] || [ -z "$UV_ASTRAL" ] || [ -z "$UV_GITHUB" ]; then
    printf 'uv-manifest.env missing UV_VERSION or download source URLs.\n' >&2
    exit 1
fi

if [ "${#ART_KEYS[@]}" -eq 0 ]; then
    printf 'No UV_*_SIZE entries found in uv-manifest.env.\n' >&2
    exit 1
fi

verify_artifact_source() {
    local triple="$1" source="$2" url="$3" expected_size="$4" expected_sha="$5"
    local artifact dest tmp rc actual_size actual_sha token label dl_start dl_elapsed
    artifact=$(manifest_key_to_artifact "$triple")
    dest="${TMP_DIR}/${triple}.${source}.${artifact##*.}"
    tmp="${dest}.part"
    label="${triple} @ ${source}"

    rm -f "$tmp" "$dest" 2>/dev/null || true
    dl_start=$(date +%s.%N 2>/dev/null || date +%s)
    token="${UV_GITHUB_TOKEN:-}"
    if [ -n "$token" ]; then
        curl -fL -s --header "Authorization: Bearer $token" "$url" -o "$tmp"
    else
        curl -fL -s "$url" -o "$tmp"
    fi
    rc=$?
    dl_elapsed=$(format_download_elapsed "$dl_start")
    if [ "$rc" -ne 0 ] || [ ! -f "$tmp" ]; then
        log_fail "$label" "download failed in ${dl_elapsed} (curl exit $rc)"
        rm -f "$tmp" 2>/dev/null || true
        return 1
    fi
    mv -f "$tmp" "$dest"

    actual_size=$(wc -c <"$dest" | awk '{print $1}')
    if [ "$actual_size" != "$expected_size" ]; then
        log_fail "$label" "size mismatch in ${dl_elapsed}: got $actual_size, want $expected_size"
        [ "$KEEP" -eq 0 ] && rm -f "$dest" 2>/dev/null || true
        return 1
    fi

    actual_sha=$(sha256_file "$dest")
    if [ "$actual_sha" != "$expected_sha" ]; then
        log_fail "$label" "sha256 mismatch in ${dl_elapsed}: got $actual_sha"
        [ "$KEEP" -eq 0 ] && rm -f "$dest" 2>/dev/null || true
        return 1
    fi

    log_pass "$label (${actual_size} bytes, sha256 ok, download ${dl_elapsed})"
    [ "$KEEP" -eq 0 ] && rm -f "$dest" 2>/dev/null || true
    return 0
}

section "uv-manifest mirror verification (uv ${UV_VERSION})"
printf 'Manifest: %s\n' "$MANIFEST"
printf 'Cache:    %s\n' "$TMP_DIR"
printf 'Filter:   source=%s' "$SOURCE_FILTER"
[ -n "$TRIPLE_FILTER" ] && printf ' triple=%s' "$TRIPLE_FILTER"
printf '\n'

for key in "${ART_KEYS[@]}"; do
    triple=$(manifest_key_to_triple "$key")
    if [ -n "$TRIPLE_FILTER" ] && [ "$triple" != "$TRIPLE_FILTER" ]; then
        continue
    fi

    expected_size="${ART_SIZE[$key]:-}"
    expected_sha="${ART_SHA[$key]:-}"
    if [ -z "$expected_size" ] || [ -z "$expected_sha" ]; then
        log_skip "$triple" 'missing SIZE or SHA256 in manifest'
        continue
    fi

    artifact=$(manifest_key_to_artifact "$key")
    section "$triple ($artifact)"

    if source_enabled 'cn'; then
        cn_url="${ART_CN_URL[$key]:-}"
        if [ -z "$cn_url" ]; then
            log_skip "$triple @ cn" 'no UV_*_DOWNLOAD_CN in manifest'
        else
            verify_artifact_source "$key" 'cn' "$cn_url" "$expected_size" "$expected_sha" || true
        fi
    fi

    if source_enabled 'astral'; then
        astral_url="${UV_ASTRAL%/}/${UV_VERSION}/${artifact}"
        verify_artifact_source "$key" 'astral' "$astral_url" "$expected_size" "$expected_sha" || true
    fi

    if source_enabled 'github'; then
        github_url="${UV_GITHUB%/}/${UV_VERSION}/${artifact}"
        verify_artifact_source "$key" 'github' "$github_url" "$expected_size" "$expected_sha" || true
    fi
done

section 'Summary'
printf 'Pass: %s  Fail: %s  Skip: %s\n' "$PASS" "$FAIL" "$SKIP"
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
