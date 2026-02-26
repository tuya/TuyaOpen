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
- Installs Python dependencies from `requirements.txt`
- Exports `OPEN_SDK_ROOT`, `OPEN_SDK_PYTHON`, and `OPEN_SDK_PIP`
- Makes `tos.py` available in the current shell

### Build workflow

Standard flow:
1. `tos.py check` to verify required tools and submodules
2. `cd examples/<category>/<project>`
3. `tos.py build`
4. Use binaries from `<project>/dist/` (LINUX target produces native ELF)

### Non-interactive configuration guidance

- `tos.py config choice` and `tos.py config menu` are interactive TTY flows. Avoid them in non-interactive cloud runs.
- Prefer editing `app_default.config` directly for deterministic builds.
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

### Artifacts and output locations

- Build intermediates: `<project>/.build/`
- Final outputs: `<project>/dist/`
- Platform SDK cache: `platform/LINUX/`
