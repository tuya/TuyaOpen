# LVGL PC Simulator

Run any TuyaOpen App's LVGL UI on a Linux desktop—no hardware required. UI source files are compiled directly into a native binary and rendered in an SDL2 window.

The simulator also compiles the `platform/LINUX` TKL adapter layer and the `src/tal_system` TAL layer, so UI code can call `tal_*` / `tkl_*` interfaces natively and use `PR_xxx` macros for logging.

---

## Directory Layout

```
lvgl_simulator/
├── CMakeLists.txt        # Build script (rarely needs editing)
├── sim_config.cmake      # App config — the only file you need to edit to switch apps
├── sim_main.c.in         # main.c template (generated into .build/ by cmake)
├── include/              # Stub headers (tuya_kconfig.h and other simulator-only stubs)
├── dist/                 # Build output: lvgl_sim executable
└── .build/               # CMake intermediate files (safe to delete)
```

---

## Prerequisites

```bash
# Ubuntu / Debian
sudo apt install cmake gcc libsdl2-dev

# Fedora / RHEL
sudo dnf install cmake gcc SDL2-devel
```

---

## Build & Run

### First build (or after changing sim_config.cmake / CMakeLists.txt)

```bash
cd examples/graphics/lvgl_simulator

cmake -B .build -DCMAKE_BUILD_TYPE=Debug
cmake --build .build -j$(nproc)

./dist/lvgl_sim
```

### Incremental build (UI source files changed only)

```bash
cmake --build .build -j$(nproc)
./dist/lvgl_sim
```

### Release build

```bash
cmake -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build -j$(nproc)
```

### Clean

```bash
# Remove executable only
rm -f dist/lvgl_sim

# Full clean — forces a complete rebuild next time
rm -rf .build dist
```

---

## Configuring sim_config.cmake

`sim_config.cmake` is the single file you edit to switch between apps. All variables:

```cmake
# Window title (any string, display-only)
set(SIM_APP_NAME  "tuya_t5_pocket_ai")

# Screen resolution — match the target hardware
set(SIM_SCREEN_W  384)
set(SIM_SCREEN_H  168)

# UI entry function — called by the simulator's main()
set(SIM_ENTRY_FUNC  "screens_init")

# UI source directories — all .c files are compiled recursively
# Multiple directories can be listed
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/display"
)

# Header search paths (no SDK paths needed; simulator stubs cover them)
# Multiple directories can be listed
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/expand/inc"
)
```

After editing `sim_config.cmake`, re-run `cmake -B .build` — an incremental build is not sufficient.

---

## Macro Reference

The simulator uses a set of compile-time macros to separate PC and hardware environments.

### Core macros

| Macro | When defined | Purpose |
|-------|-------------|---------|
| `LVGL_PC_SIMULATOR` | Injected by CMake during simulator build | Master switch — identifies the PC simulator environment |
| `ENABLE_LVGL_HARDWARE` | Defined by `screen_manager.h` on hardware | Identifies the real hardware environment |
| `LV_CONF_INCLUDE_SIMPLE` | Injected by CMake | Tells LVGL to locate config via `lv_conf.h` |
| `LV_LVGL_H_INCLUDE_SIMPLE` | Injected by CMake | Tells LVGL internals to `#include "lvgl.h"` |
| `OPERATING_SYSTEM=100` | Injected by CMake | `SYSTEM_LINUX` — activates Linux branch in `tuya_cloud_types.h` |

The relationship defined in `screen_manager.h`:

```c
// ENABLE_LVGL_HARDWARE is only defined outside the simulator
#ifndef LVGL_PC_SIMULATOR
#define ENABLE_LVGL_HARDWARE
#endif
```

### lv_conf.h options affected by LVGL_PC_SIMULATOR

| Option | Hardware value | Simulator value | Notes |
|--------|---------------|-----------------|-------|
| `LV_USE_STDLIB_MALLOC` | `LV_STDLIB_CUSTOM` | `LV_STDLIB_CLIB` | Embedded uses tlsf; PC uses system malloc |
| `LV_USE_SDL` | `0` | `1` | SDL2 display/input driver |
| `LV_USE_PNG` | `1` (Tuya custom) | `0` | Hardware PNG decoder (requires PSRAM, unavailable on PC) |
| `LV_USE_LODEPNG` | `0` | `1` | Pure-software PNG decoder used on PC |

### Writing hardware guards in UI source files

**Recommended — use `ENABLE_LVGL_HARDWARE`:**

```c
// Hardware-only headers
#ifdef ENABLE_LVGL_HARDWARE
#include "lv_vendor.h"
#include "tkl_output.h"
#include "tdl_camera_manage.h"
#endif

// Hardware operation with simulator stub
#ifdef ENABLE_LVGL_HARDWARE
    camera_hw_start();
#else
    printf("[SIM] camera stub\n");
#endif
```

**Function-level stub pattern:**

```c
OPERATE_RET ai_ui_chat_register(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    AI_UI_INTFS_T intfs = {0};
    intfs.disp_emotion    = __ui_set_emotion;
    intfs.disp_wifi_state = __ui_set_network;
    return ai_ui_register(&intfs);
#else
    return OPRT_OK;   // simulator: no-op
#endif
}
```

**Note:** `#ifdef ENABLE_LVGL_HARDWARE` and `#ifndef LVGL_PC_SIMULATOR` are equivalent; prefer the former for consistency across the codebase.

---

## TAL/TKL Interfaces & PR_xxx Logging

The simulator compiles the full `platform/LINUX` TKL adapter and `src/tal_system` TAL layer. UI code can use these interfaces directly:

- **`PR_ERR` / `PR_WARN` / `PR_NOTICE` / `PR_INFO` / `PR_DEBUG` / `PR_TRACE`** — TAL log macros, output to stdout
- **`tal_mutex_*` / `tal_semaphore_*`** — thread synchronization backed by Linux pthread
- **`tal_thread_*`** — thread management
- **`tal_system_*`** — system information
- **`tal_log_*`** — logging system, initialized at `TAL_LOG_LEVEL_DEBUG` in `main()`

Example log output:
```
[E][file.c:42] some error message
[D][file.c:100] debug info
```

**Compiled TKL modules** (Linux-native): `tkl_memory`, `tkl_mutex`, `tkl_semaphore`, `tkl_queue`, `tkl_thread`, `tkl_system`, `tkl_output`, `tkl_fs`

**Compiled TAL modules**: `tal_log`, `tal_sleep`, `tal_sw_timer`, `tal_system`, `tal_thread`, `tal_time_service`, `tal_workqueue`, `tal_workq_service`

> **Not compiled**: `tal_api` (requires network/UART/KV subsystems, not needed by the UI layer), `tal_event`, `tal_fs` (both depend on `tal_api.h`).

---

## Keyboard Controls

SDL keyboard events are mapped to LVGL key codes:

| Key | LVGL event |
|-----|-----------|
| ↑ / ↓ / ← / → | `LV_KEY_UP` / `LV_KEY_DOWN` / `LV_KEY_LEFT` / `LV_KEY_RIGHT` |
| Enter | `LV_KEY_ENTER` |
| Esc | `LV_KEY_ESC` |
| Tab | `LV_KEY_NEXT` |
