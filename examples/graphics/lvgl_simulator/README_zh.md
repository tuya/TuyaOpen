# LVGL PC 模拟器

在 Linux 桌面上运行 TuyaOpen App 的 LVGL UI，无需烧录硬件。UI 源码直接编译为原生可执行文件，通过 SDL2 窗口渲染 LVGL 界面。

模拟器同时编译了 `platform/LINUX` 的 TKL 适配层和 `src/tal_system` 的 TAL 层，使得 UI 代码可以直接调用 `tal_*` / `tkl_*` 接口，并使用 `PR_xxx` 宏输出日志。

---

## 目录结构

```
lvgl_simulator/
├── CMakeLists.txt        # 构建脚本（通常无需修改）
├── sim_config.cmake      # App 配置文件（修改此文件切换 App）
├── sim_main.c.in         # main.c 模板（cmake 时自动生成到 .build/）
├── include/              # Stub 头文件（tuya_kconfig.h 等模拟器专用桩）
├── dist/                 # 编译产物：可执行文件 lvgl_sim
└── .build/               # CMake 中间产物（可安全删除）
```

---

## 依赖安装

```bash
# Ubuntu / Debian
sudo apt install cmake gcc libsdl2-dev

# Fedora / RHEL
sudo dnf install cmake gcc SDL2-devel
```

---

## 编译与运行

### 首次编译（或修改了 sim_config.cmake / CMakeLists.txt 后）

```bash
cd examples/graphics/lvgl_simulator

cmake -B .build -DCMAKE_BUILD_TYPE=Debug
cmake --build .build -j$(nproc)

./dist/lvgl_sim
```

### 仅修改了 UI 源码后（增量编译）

```bash
cmake --build .build -j$(nproc)
./dist/lvgl_sim
```

### Release 构建

```bash
cmake -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build -j$(nproc)
```

### 清除编译产物

```bash
# 只清除可执行文件
rm -f dist/lvgl_sim

# 清除所有中间产物（完全重新编译）
rm -rf .build dist
```

---

## 修改配置文件

`sim_config.cmake` 是切换 App 的唯一入口，所有变量说明如下：

```cmake
# 窗口标题（任意字符串，仅用于显示）
set(SIM_APP_NAME  "tuya_t5_pocket_ai")

# 屏幕分辨率（与目标硬件保持一致）
set(SIM_SCREEN_W  384)
set(SIM_SCREEN_H  168)

# UI 入口函数名（模拟器 main() 会调用此函数）
set(SIM_ENTRY_FUNC  "screens_init")

# UI 源码目录（递归搜索所有 .c 文件参与编译）
# 可以列多个目录
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/display"
)

# UI 头文件搜索路径（不含 SDK 路径，模拟器 stub 已覆盖）
# 可以列多个目录
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/expand/inc"
)
```

修改 `sim_config.cmake` 后，需要重新运行 `cmake -B .build` 才能生效（增量编译不够）。

---

## 宏定义说明

模拟器通过一组编译宏区分 PC 模拟环境与真实硬件环境。

### 核心宏

| 宏 | 定义时机 | 作用 |
|----|---------|------|
| `LVGL_PC_SIMULATOR` | 模拟器编译时由 CMake 注入 | 总开关，标识当前为 PC 模拟器环境 |
| `ENABLE_LVGL_HARDWARE` | 硬件编译时由 `screen_manager.h` 定义 | 标识当前为真实硬件环境 |
| `LV_CONF_INCLUDE_SIMPLE` | 模拟器编译时注入 | 让 LVGL 以 `lv_conf.h` 方式查找配置头 |
| `LV_LVGL_H_INCLUDE_SIMPLE` | 模拟器编译时注入 | 让 LVGL 内部以 `"lvgl.h"` 方式 include |
| `OPERATING_SYSTEM=100` | 模拟器编译时注入 | `SYSTEM_LINUX`，激活 `tuya_cloud_types.h` 的 Linux 分支 |

`screen_manager.h` 中的定义关系：

```c
// 非模拟器环境下才定义硬件宏
#ifndef LVGL_PC_SIMULATOR
#define ENABLE_LVGL_HARDWARE
#endif
```

### lv_conf.h 中受 LVGL_PC_SIMULATOR 影响的配置

| 配置项 | 硬件值 | 模拟器值 | 说明 |
|--------|--------|---------|------|
| `LV_USE_STDLIB_MALLOC` | `LV_STDLIB_CUSTOM` | `LV_STDLIB_CLIB` | 内存分配器：嵌入式用 tlsf，PC 用系统 malloc |
| `LV_USE_SDL` | `0` | `1` | SDL2 显示/输入驱动 |
| `LV_USE_PNG` | `1`（Tuya 定制） | `0` | 硬件 PNG 解码器（依赖 PSRAM，PC 不可用） |
| `LV_USE_LODEPNG` | `0` | `1` | 纯软件 PNG 解码器（PC 模拟器使用） |

### 在 UI 源码中使用宏

**推荐写法：使用 `ENABLE_LVGL_HARDWARE`**

```c
// 硬件专属头文件
#ifdef ENABLE_LVGL_HARDWARE
#include "lv_vendor.h"
#include "tkl_output.h"
#include "tdl_camera_manage.h"
#endif

// 硬件操作分支
#ifdef ENABLE_LVGL_HARDWARE
    camera_hw_start();
#else
    printf("[SIM] camera stub\n");
#endif
```

**函数级整体屏蔽写法：**

```c
OPERATE_RET ai_ui_chat_register(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    AI_UI_INTFS_T intfs = {0};
    intfs.disp_emotion    = __ui_set_emotion;
    intfs.disp_wifi_state = __ui_set_network;
    return ai_ui_register(&intfs);
#else
    return OPRT_OK;   // 模拟器：空实现
#endif
}
```

**注意：** `#ifndef LVGL_PC_SIMULATOR` 与 `#ifdef ENABLE_LVGL_HARDWARE` 等价，但推荐统一使用后者，保持代码一致性。

---

## TAL/TKL 接口与 PR_xxx 日志

模拟器编译了完整的 `platform/LINUX` TKL 适配层和 `src/tal_system` TAL 层，UI 代码可以直接使用：

- **`PR_ERR` / `PR_WARN` / `PR_NOTICE` / `PR_INFO` / `PR_DEBUG` / `PR_TRACE`** — TAL 日志宏，输出到 stdout
- **`tal_mutex_*` / `tal_semaphore_*`** — 线程同步，底层调用 Linux pthread
- **`tal_thread_*`** — 线程管理
- **`tal_system_*`** — 系统信息
- **`tal_log_*`** — 日志系统，已在 `main()` 中初始化为 `TAL_LOG_LEVEL_DEBUG`

日志格式示例：
```
[E][file.c:42] some error message
[D][file.c:100] debug info
```

已编译的 TKL 模块（Linux 原生实现）：`tkl_memory`, `tkl_mutex`, `tkl_semaphore`, `tkl_queue`, `tkl_thread`, `tkl_system`, `tkl_output`, `tkl_fs`

已编译的 TAL 模块：`tal_log`, `tal_sleep`, `tal_sw_timer`, `tal_system`, `tal_thread`, `tal_time_service`, `tal_workqueue`, `tal_workq_service`

> **未编译**：`tal_api`（依赖网络/UART/KV 子系统，UI 层无需）、`tal_event`、`tal_fs`（依赖 `tal_api.h`）。

---

## 键盘控制

SDL 键盘事件映射到 LVGL 按键：

| 按键 | LVGL 事件 |
|------|-----------|
| ↑ / ↓ / ← / → | `LV_KEY_UP` / `LV_KEY_DOWN` / `LV_KEY_LEFT` / `LV_KEY_RIGHT` |
| Enter | `LV_KEY_ENTER` |
| Esc | `LV_KEY_ESC` |
| Tab | `LV_KEY_NEXT` |
