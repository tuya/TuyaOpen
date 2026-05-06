# LVGL PC Simulator

Run any TuyaOpen App's LVGL UI on a desktop—no hardware required. UI source files are compiled directly into a native binary and rendered in an SDL2 window.

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

### Linux

```bash
# Ubuntu / Debian
sudo apt install cmake gcc libsdl2-dev

# Fedora / RHEL
sudo dnf install cmake gcc SDL2-devel

# Arch Linux
sudo pacman -S cmake gcc sdl2
```

### macOS

Install [Homebrew](https://brew.sh) if not already installed, then:

```bash
brew install cmake sdl2
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
# Window title / app identifier (also used to derive the SIM_APP_<name> macro)
set(SIM_APP_NAME  "tuya_t5_pocket_ai")

# Screen resolution — match the target hardware
set(SIM_SCREEN_W  384)
set(SIM_SCREEN_H  168)

# UI entry function — called by the simulator's main()
set(SIM_ENTRY_FUNC  "ui_init")

# UI source directories or individual .c file paths — compiled recursively for directories
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/display"
)

# Header search paths (no SDK paths needed; simulator stubs cover them)
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/expand/inc"
)
```

After editing `sim_config.cmake`, re-run `cmake -B .build` — an incremental build is not sufficient.

### Predefined app configurations

The file ships with four ready-to-use configurations (uncomment the one you want):

| App name | Resolution | Entry function | Notes |
|----------|-----------|---------------|-------|
| `tuya_t5_pocket_ai` | 384×168 | `ui_init` | Tuya T5 pocket AI device (default active) |
| `your_chat_bot_wechat` | 480×320 | `ui_init` | Chat-bot — WeChat-style UI |
| `your_chat_bot_chatbot` | 480×320 | `ui_init` | Chat-bot — chatbot-style UI |

---

## tuya_kconfig.h — Per-app Feature Macros

`include/tuya_kconfig.h` is a simulator stub for the Kconfig-generated header that normally lives in the firmware build. It enables the feature macros that each app's UI code depends on.

CMake converts `SIM_APP_NAME` to a C identifier and injects it as a compile-time macro:

```cmake
string(MAKE_C_IDENTIFIER "${SIM_APP_NAME}" _sim_app_id)
# e.g. "your_chat_bot_wechat" → SIM_APP_your_chat_bot_wechat
target_compile_definitions(... SIM_APP_${_sim_app_id})
```

`tuya_kconfig.h` can then selectively enable macros per app:

```c
// Always enable common font sizes used across UIs
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

```

Add new entries here whenever a new app requires feature macros that aren't defined elsewhere.

---

## Macro Reference

The simulator uses a set of compile-time macros to separate PC and hardware environments.

### Core macros

| Macro | When defined | Purpose |
|-------|-------------|---------|
| `LVGL_PC_SIMULATOR` | Injected by CMake during simulator build | Master switch — identifies the PC simulator environment |
| `ENABLE_LVGL_HARDWARE` | Defined by `screen_manager.h` on hardware | Identifies the real hardware environment (pocket project only) |
| `LV_CONF_INCLUDE_SIMPLE` | Injected by CMake | Tells LVGL to locate config via `lv_conf.h` |
| `LV_LVGL_H_INCLUDE_SIMPLE` | Injected by CMake | Tells LVGL internals to `#include "lvgl.h"` |
| `OPERATING_SYSTEM=100` | Injected by CMake | `SYSTEM_LINUX` — activates Linux branch in `tuya_cloud_types.h` |
| `SIM_APP_<name>` | Injected by CMake | Per-app identifier for `tuya_kconfig.h` conditional macros |

### lv_conf.h options affected by LVGL_PC_SIMULATOR

| Option | Hardware value | Simulator value | Notes |
|--------|---------------|-----------------|-------|
| `LV_USE_STDLIB_MALLOC` | `LV_STDLIB_CUSTOM` | `LV_STDLIB_CLIB` | Embedded uses tlsf; PC uses system malloc |
| `LV_USE_SDL` | `0` | `1` | SDL2 display/input driver |
| `LV_USE_PNG` | `1` (Tuya custom) | `0` | Hardware PNG decoder (requires PSRAM, unavailable on PC) |
| `LV_USE_LODEPNG` | `0` | `1` | Pure-software PNG decoder used on PC |

### Writing hardware guards in UI source files

**Standard pattern — `#ifndef LVGL_PC_SIMULATOR`:**

```c
// Hardware-only headers
#ifndef LVGL_PC_SIMULATOR
#include "tal_api.h"
#include "lv_vendor.h"
#endif

// Hardware operation with simulator stub
#ifndef LVGL_PC_SIMULATOR
    camera_hw_start();
#else
    printf("[SIM] camera stub\n");
#endif
```

**Function-level stub pattern:**

```c
OPERATE_RET ai_ui_register_intfs(void)
{
#ifndef LVGL_PC_SIMULATOR
    AI_UI_INTFS_T intfs = {0};
    intfs.disp_emotion    = __ui_set_emotion;
    intfs.disp_wifi_state = __ui_set_network;
    return ai_ui_register(&intfs);
#else
    return OPRT_OK;   // simulator: no-op
#endif
}
```

---

## TAL/TKL Interfaces & PR_xxx Logging

The simulator compiles the full `platform/LINUX` TKL adapter and `src/tal_system` TAL layer. UI code can use these interfaces directly:

- **`PR_ERR` / `PR_WARN` / `PR_NOTICE` / `PR_INFO` / `PR_DEBUG` / `PR_TRACE`** — TAL log macros, output to stdout
- **`tal_mutex_*` / `tal_semaphore_*`** — thread synchronization backed by Linux pthread
- **`tal_thread_*`** — thread management
- **`tal_system_*`** — system information
- **`tal_log_*`** — logging system, initialized at `TAL_LOG_LEVEL_DEBUG` in `main()`

---

## Keyboard Controls

SDL keyboard events are mapped to LVGL key codes