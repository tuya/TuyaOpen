# LVGL PC Simulator Component Design

**Date:** 2026-05-06  
**Branch:** lvgl_pc_simulator  
**Scope:** Fix current implementation to match dev.md requirements

---

## Background

A previous session implemented the LVGL PC simulator as `src/liblvgl/simulator/` component. The implementation is mostly correct but has two problems identified in the updated dev.md (2026-05-06):

1. `lvgl_sim.c` depends on `tal_log.h` / `tkl_output.h` — violates the "no strong platform binding" requirement
2. `app_default.config` was incorrectly modified during development testing — simulator is enabled and embedded board selection was removed

---

## Requirements

- `src/liblvgl/simulator/` is a pure LVGL + SDL2 bootstrap — no TAL/TKL dependencies
- UI design code only uses LVGL (`#ifdef ENABLE_LVGL_HARDWARE` guards handle hardware code)
- Default config remains the embedded T5AI baseline; simulator activated manually via `tos.py config menu`
- No changes to `tuya_main.c`, no new config files, no changes to `tos.py`

---

## Architecture

The component structure remains unchanged. Only `lvgl_sim.c` internals change.

```
src/liblvgl/simulator/
├── Kconfig              — unchanged (depends on ENABLE_LIBLVGL && PLATFORM_LINUX)
├── CMakeLists.txt       — unchanged
├── include/lvgl_sim.h   — unchanged
└── src/lvgl_sim.c       — remove tal_log_init / tkl_log_output
```

### Dependency chain after fix

```
lvgl_sim.c → LVGL v9 + SDL2 drivers only
```

No TAL, no TKL, no embedded platform headers.

### UI guard mechanism (unchanged)

`screen_manager.h` defines the gate:

```c
#ifndef LVGL_PC_SIMULATOR
#define ENABLE_LVGL_HARDWARE
#endif
```

All hardware-dependent includes in display/ui/*.c are wrapped in `#ifdef ENABLE_LVGL_HARDWARE`. This is already correct in the codebase.

---

## Changes

### Change 1 — `src/liblvgl/simulator/src/lvgl_sim.c`

**Remove:**
```c
#include "tal_log.h"
#include "tkl_output.h"
// and the call:
tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
```

**Result:** `lvgl_sim_start()` begins directly with `lv_init()` then SDL window/input setup.

### Change 2 — `apps/tuya_t5_pocket/tuya_t5_pocket_ai/app_default.config`

Restore via:
```bash
git checkout HEAD -- apps/tuya_t5_pocket/tuya_t5_pocket_ai/app_default.config
```

Restores the embedded T5AI baseline:
- `CONFIG_BOARD_CHOICE_T5AI=y`
- `CONFIG_BOARD_CHOICE_TUYA_T5AI_POCKET=y`
- `CONFIG_LVGL_PC_SIMULATOR` not set (defaults to `n`)

---

## Files Unchanged

| File | Reason |
|------|--------|
| `src/liblvgl/simulator/Kconfig` | `depends on PLATFORM_LINUX` is correct for manual workflow |
| `src/liblvgl/simulator/CMakeLists.txt` | Conditional compile logic correct |
| `src/liblvgl/simulator/include/lvgl_sim.h` | Interface unchanged |
| `src/liblvgl/Kconfig` | `rsource "simulator/Kconfig"` correct |
| `src/liblvgl/CMakeLists.txt` | `add_subdirectory(simulator)` correct |
| `apps/.../CMakeLists.txt` | if/else conditional compile correct |
| `platform/LINUX/.../tuyaopen_adapter.cmake` | SDL2 link correct |
| All `display/ui/*.c` | `#ifdef ENABLE_LVGL_HARDWARE` guards in place |
| `rfid_scan_screen.c/.h` | Already wrapped in `#ifdef ENABLE_LVGL_HARDWARE` |
| `main_screen.c` | rfid_scan_screen load already commented out |

---

## Usage Workflow

### Simulator development
```bash
tos.py config menu   # Select LINUX/Ubuntu board → enable LVGL PC Simulator → save
tos.py build         # Compiles SDL executable
```

### Embedded development
```bash
tos.py config menu   # Select T5AI board → LVGL PC Simulator off → save
tos.py build         # Compiles embedded firmware
```

---

## Out of Scope

- `tuya_main.c` — not modified
- `tos.py` / `cli_build.py` — not modified (manual board selection workflow retained)
- No new config files
- No auto platform selection
