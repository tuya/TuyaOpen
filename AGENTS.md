# AGENTS.md

## Cursor Cloud specific instructions

### Overview

TuyaOpen is a cross-platform IoT SDK (C/C++) for smart hardware. It supports Tuya T-series MCUs, ESP32, Raspberry Pi, and Linux/Ubuntu. On Cursor Cloud, the LINUX target can be compiled and run natively on the host.

### Communication and behavior

- Always respond in Simplified Chinese unless explicitly requested otherwise.
- Prefer concise progress updates while running commands.
- Do not use interactive workflows unless the task explicitly requires them.

### Environment setup

Initialize the environment from repository root:

```bash
cd /workspace && . ./export.sh
```

What this does:
- Creates or reuses `.venv/`
- Syncs Python dependencies via `uv sync` (`pyproject.toml` + `uv.lock`)
- Runs `tos.py prepare` to install SDK host tools (on Windows, GNU Make goes under `.tools/make/<version>/`; downloads are cached under `.tools/archives/`)
- Exports `OPEN_SDK_ROOT`, `OPEN_SDK_PYTHON`, and `OPEN_SDK_PIP` on all platforms; on Windows, also exports `OPEN_SDK_MAKE_BIN` and `OPEN_SDK_MAKE` after `tos.py prepare` installs GNU Make
- Makes `tos.py` available in the current shell

On Windows, after `export.ps1` / `export.bat`, you can also run `tos.py prepare` manually to retry host-tool setup.

### Export progress protocol (IDE)

When `TUYAOPEN_EXPORT_IDE=1` (set by TuyaOpen IDE during non-interactive init), export scripts emit line-based progress on stderr for the IDE UI. Manual `. ./export.sh` / `. .\export.ps1` without this flag keeps the original interactive progress bars and behavior.

| Line pattern | Stage | Example |
|--------------|-------|---------|
| `[TuyaOpen] Stage: <id>` | stage switch | `[TuyaOpen] Stage: sync` |
| `[TuyaOpen] Downloading <artifact>: X / Y MB` | uv download | `[TuyaOpen] Downloading uv-x86_64.tar.gz: 12.3 / 25.6 MB` |
| `[TuyaOpen] Installing Python <ver>: ...: X / Y MB (N%)` | python install | `[TuyaOpen] Installing Python 3.12.13: cpython-...: 12.5 / 25.6 MB (48%)` |
| `[TuyaOpen] Syncing dependencies [###---] N/M (P%) - pkg` | uv sync | `[TuyaOpen] Syncing dependencies [########------] 8/28 (28%) - pydantic` |

IDE parser: `tuyaopen_ide/src/extension.ts` → `parseSdkEnvStageLine`.

Export tests: `bash tests/export/run_all.sh`

### Build workflow

Standard flow:
1. `tos.py check` to verify required tools and submodules
2. `cd examples/<category>/<project>`
3. `tos.py build`
4. Use binaries from `<project>/dist/` (LINUX target produces native ELF)

### Non-interactive configuration guidance

- `tos.py config menu` is an interactive TTY flow. Avoid it in non-interactive cloud runs; use `tos.py config set` instead.
- Read and write individual symbols without a TTY (the `CONFIG_` prefix is optional everywhere):

```bash
tos.py config get CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN   # print one value
tos.py config get -a CONFIG_ENABLE_WIFI                       # type, prompt, deps
tos.py config list -p MBEDTLS                                 # filtered dump
tos.py config list -j                                         # JSON, for scripts
tos.py config set CONFIG_ENABLE_LIBLVGL=y CONFIG_XX=8192      # dependency-aware
tos.py config set -u CONFIG_ENABLE_LIBLVGL                    # revert to default
tos.py config diff T5AI                                       # vs a saved config
tos.py config save -n my_board -f                             # no prompt
```

- `tos.py config choice -c NAME.config` / `-l` are already non-interactive and remain the right way to switch to a whole saved config.
- Prefer `config set` over hand-editing `app_default.config`. Hand-editing bypasses kconfiglib, so `choice` symbols are not made mutually exclusive, derived symbols (`CONFIG_PLATFORM_CHOICE`, `CONFIG_CHIP_CHOICE`) are not updated, and `.build/cache/using.cmake` plus `.build/include/tuya_kconfig.h` stay stale on a warm build tree (`tools/kconfiglib/CMakeLists.txt` only generates them when absent). `config set` handles all of that.
- `config set` exits non-zero and writes nothing when any assignment fails, so a batch is all-or-nothing.
- To avoid prompt blocks from platform commit checks, create:

```bash
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
```

### Lint and formatting

- Single file or directory checks:
  - `python tools/check_format.py --debug --files <file>`
  - `python tools/check_format.py --debug --dir <dir>`
- PR-style checks:
  - `python tools/check_format.py --base <branch>`

### System dependencies

Expected packages (see `Dockerfile`):
`build-essential`, `libsystemd-dev`, `locales`, `libc6-i386`, `libusb-1.0-0`, `libusb-1.0-0-dev`, `python3`, `python3-pip`, `python3-venv`, `clang-format`

### Validation expectations

- For source changes, run the smallest relevant build or check for the touched area.
- For formatting-only updates, run `tools/check_format.py` against changed files.
- For docs-only updates, verify file content, command correctness, and path validity.
- For `tools/cli_command/` changes, run the Python unit tests from the repo root (`-t .` is required so the `tools.cli_command.*` imports resolve):

```bash
python -m unittest discover -s tools/cli_command/tests -t . -p "test_*.py"
```

- The `tos.py config` end-to-end tests load the real Kconfig tree and are opt-in (no compilation involved):

```bash
TUYAOPEN_E2E=1 python -m unittest tools.cli_command.tests.test_config_e2e
```

### Artifacts and output locations

- Build intermediates: `<project>/.build/`
- Final outputs: `<project>/dist/`
- Platform SDK cache: `platform/LINUX/`

