# LVGL PC 模拟器

在桌面上运行 TuyaOpen App 的 LVGL UI，无需烧录硬件。UI 源码直接编译为原生可执行文件，通过 SDL2 窗口渲染 LVGL 界面。

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

### Linux

```bash
# Ubuntu / Debian
sudo apt install build-essential cmake libsdl2-dev

# Fedora / RHEL
sudo dnf install @development-tools cmake SDL2-devel

# Arch Linux
sudo pacman -S base-devel cmake sdl2
```

### macOS

如未安装 [Homebrew](https://brew.sh)，先安装，然后：

```bash
brew install sdl2 cmake make
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
# 窗口标题 / App 标识符（同时用于派生 SIM_APP_<name> 宏）
set(SIM_APP_NAME  "tuya_t5_pocket_ai")

# 屏幕分辨率（与目标硬件保持一致）
set(SIM_SCREEN_W  384)
set(SIM_SCREEN_H  168)

# UI 入口函数名（模拟器 main() 会调用此函数）
set(SIM_ENTRY_FUNC  "ui_init")

# UI 源码目录或单个 .c 文件路径；目录会递归搜索所有 .c 文件
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/display"
)

# UI 头文件搜索路径（不含 SDK 路径，模拟器 stub 已覆盖）
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/your_app/src/expand/inc"
)
```

修改 `sim_config.cmake` 后，需要重新运行 `cmake -B .build` 才能生效（增量编译不够）。

### 预置 App 配置

文件中已内置四套配置（取消注释对应段落即可激活）：

| App 名称 | 分辨率 | 入口函数 | 说明 |
|---------|--------|---------|------|
| `tuya_t5_pocket_ai` | 384×168 | `ui_init` | Tuya T5 口袋 AI 设备（当前默认激活） |
| `your_chat_bot_wechat` | 480×320 | `ui_init` | 聊天机器人 — 微信风格 UI |
| `your_chat_bot_chatbot` | 480×320 | `ui_init` | 聊天机器人 — chatbot 风格 UI |

---

## tuya_kconfig.h — 按 App 开启功能宏

`include/tuya_kconfig.h` 是 Kconfig 生成头文件的模拟器桩，用于开启各 App UI 代码所依赖的功能宏。

CMake 将 `SIM_APP_NAME` 转换为 C 标识符并注入为编译宏：

```cmake
string(MAKE_C_IDENTIFIER "${SIM_APP_NAME}" _sim_app_id)
# 例："your_chat_bot_wechat" → SIM_APP_your_chat_bot_wechat
target_compile_definitions(... SIM_APP_${_sim_app_id})
```

`tuya_kconfig.h` 即可按 App 有条件地开启宏：

```c
// 各 UI 通用字体大小，无条件开启
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

```

新增 App 时，若 UI 代码依赖额外的功能宏，在此处添加对应条件定义。

---

## 宏定义说明

模拟器通过一组编译宏区分 PC 模拟环境与真实硬件环境。

### 核心宏

| 宏 | 定义时机 | 作用 |
|----|---------|------|
| `LVGL_PC_SIMULATOR` | 模拟器编译时由 CMake 注入 | 总开关，标识当前为 PC 模拟器环境 |
| `ENABLE_LVGL_HARDWARE` | 硬件编译时由 `screen_manager.h` 定义 | 标识当前为真实硬件环境（仅 pocket 项目使用） |
| `LV_CONF_INCLUDE_SIMPLE` | 模拟器编译时注入 | 让 LVGL 以 `lv_conf.h` 方式查找配置头 |
| `LV_LVGL_H_INCLUDE_SIMPLE` | 模拟器编译时注入 | 让 LVGL 内部以 `"lvgl.h"` 方式 include |
| `OPERATING_SYSTEM=100` | 模拟器编译时注入 | `SYSTEM_LINUX`，激活 `tuya_cloud_types.h` 的 Linux 分支 |
| `SIM_APP_<name>` | 由 CMake 注入 | 每个 App 的唯一标识，供 `tuya_kconfig.h` 条件开启功能宏 |

### lv_conf.h 中受 LVGL_PC_SIMULATOR 影响的配置

| 配置项 | 硬件值 | 模拟器值 | 说明 |
|--------|--------|---------|------|
| `LV_USE_STDLIB_MALLOC` | `LV_STDLIB_CUSTOM` | `LV_STDLIB_CLIB` | 内存分配器：嵌入式用 tlsf，PC 用系统 malloc |
| `LV_USE_SDL` | `0` | `1` | SDL2 显示/输入驱动 |
| `LV_USE_PNG` | `1`（Tuya 定制） | `0` | 硬件 PNG 解码器（依赖 PSRAM，PC 不可用） |
| `LV_USE_LODEPNG` | `0` | `1` | 纯软件 PNG 解码器（PC 模拟器使用） |

### 在 UI 源码中使用宏

**标准写法 — 使用 `#ifndef LVGL_PC_SIMULATOR`：**

```c
// 硬件专属头文件
#ifndef LVGL_PC_SIMULATOR
#include "tal_api.h"
#include "lv_vendor.h"
#endif

// 硬件操作分支，模拟器提供默认实现
#ifndef LVGL_PC_SIMULATOR
    camera_hw_start();
#else
    printf("[SIM] camera stub\n");
#endif
```

**函数级整体屏蔽写法：**

```c
OPERATE_RET ai_ui_register_intfs(void)
{
#ifndef LVGL_PC_SIMULATOR
    AI_UI_INTFS_T intfs = {0};
    intfs.disp_emotion    = __ui_set_emotion;
    intfs.disp_wifi_state = __ui_set_network;
    return ai_ui_register(&intfs);
#else
    return OPRT_OK;   // 模拟器：空实现
#endif
}
```

---

## TAL/TKL 接口与 PR_xxx 日志

模拟器编译了完整的 `platform/LINUX` TKL 适配层和 `src/tal_system` TAL 层，UI 代码可以直接使用：

- **`PR_ERR` / `PR_WARN` / `PR_NOTICE` / `PR_INFO` / `PR_DEBUG` / `PR_TRACE`** — TAL 日志宏，输出到 stdout
- **`tal_mutex_*` / `tal_semaphore_*`** — 线程同步，底层调用 Linux pthread
- **`tal_thread_*`** — 线程管理
- **`tal_system_*`** — 系统信息
- **`tal_log_*`** — 日志系统，已在 `main()` 中初始化为 `TAL_LOG_LEVEL_DEBUG`

---

## 键盘控制

SDL 键盘事件映射到 LVGL 按键
