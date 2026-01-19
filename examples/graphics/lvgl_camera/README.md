# LVGL2Camera 示例

[English](#english-version) | [中文](#中文版本)

---

## 中文版本

### 项目简介

lvgl2Camera 是一个基于 Tuya T5AI 开发板的图形界面与摄像头预览切换示例项目。该项目展示了如何在嵌入式设备上同时使用 LVGL 图形库和摄像头模块，并通过按键在两种显示模式之间切换。

该实例目前仅在 [T5AI Board 开发板](https://tuyaopen.ai/zh/docs/hardware-specific/t5-ai-board/overview-t5-ai-board) 的 3.5" LCD 屏幕和摄像头模块上测试通过。

### 主要特性

- ✨ **LVGL 图形界面**：显示 "Hello World" 计数器的简单 UI 界面
- 📷 **摄像头预览**：实时摄像头画面预览（480x480 @ 20fps）
- 🔄 **显示模式切换**：通过按键在 LVGL 界面和摄像头预览之间切换
- ⚡ **硬件加速**：使用 DMA2D 硬件加速进行 YUV422 到 RGB565 格式转换
- 🎯 **双缓冲机制**：采用双帧缓冲技术，确保显示流畅无撕裂
- 💾 **灵活内存管理**：支持 PSRAM 和 SRAM 两种内存分配方式

### 硬件要求

- **开发板**：Tuya T5AI Board
- **显示屏**：3.5" LCD 屏幕（配置型号：35565LCD）
- **摄像头**：兼容的摄像头模块
- **按键**：至少一个功能按键用于模式切换

### 技术规格

| 参数 | 值 |
|------|-----|
| 摄像头分辨率 | 480x480 |
| 帧率 | 20 FPS |
| 图像格式 | YUV422（摄像头输出）→ RGB565（显示输出） |
| JPEG 编码 | 支持（质量范围：10-25） |
| 内存管理 | 支持 PSRAM/SRAM |

### 项目结构

```
lvgl2Camera/
├── src/
│   ├── tuya_main.c          # 主程序入口
│   ├── app_lvgl.c           # LVGL 图形界面管理
│   ├── app_camera.c         # 摄像头管理和图像处理
│   └── app_button.c         # 按键事件处理
├── include/
│   ├── app_lvgl.h           # LVGL 模块头文件
│   ├── app_camera.h         # 摄像头模块头文件
│   └── app_button.h         # 按键模块头文件
├── config/
│   └── TUYA_T5AI_BOARD_LCD_3.5.config  # 3.5寸LCD配置
├── CMakeLists.txt           # CMake 构建配置
├── app_default.config       # 默认配置文件
└── README.md                # 项目说明文档
```

### 功能模块说明

#### 1. LVGL 图形界面模块 (`app_lvgl.c`)

- 初始化 LVGL 图形库
- 创建简单的 UI 界面，显示 "Hello World" 和实时更新的计数器
- 提供显示启动/停止控制接口
- 使用独立线程处理 UI 更新，优先级为 `THREAD_PRIO_0`

**主要函数：**
- `app_lvgl_init()` - 初始化 LVGL
- `app_lvgl_deinit()` - 反初始化 LVGL
- `app_lvgl_display_start()` - 启动 LVGL 显示
- `app_lvgl_display_stop()` - 停止 LVGL 显示

#### 2. 摄像头管理模块 (`app_camera.c`)

- 初始化摄像头设备（480x480 @ 20fps）
- 实时捕获摄像头帧数据
- 使用 DMA2D 硬件加速将 YUV422 格式转换为 RGB565
- 采用双缓冲机制避免显示撕裂
- 支持 JPEG 编码输出
- 通过工作队列异步处理图像数据

**主要函数：**
- `app_camera_init()` - 初始化摄像头
- `app_camera_deinit()` - 反初始化摄像头
- `app_camera_display_start()` - 启动摄像头显示
- `app_camera_display_stop()` - 停止摄像头显示

**技术实现：**
- 帧缓冲内存对齐（4字节对齐，主要是由于 dma2d 的需要）
- 双帧缓冲池（`sg_display_frame_buff[0]` 和 `sg_display_frame_buff[1]`）
- 回调机制处理摄像头帧数据
- DMA2D 硬件加速格式转换（转换地址需要4字节对齐）

#### 3. 按键控制模块 (`app_button.c`)

- 管理功能按键
- 处理按键单击事件
- 在 LVGL 模式和摄像头模式之间切换
- 支持防抖动处理（50ms）
- 支持长按检测（3秒起始，1秒间隔）

**主要函数：**
- `app_button_init()` - 初始化按键
- `app_button_deinit()` - 反初始化按键

**显示模式：**
- `APP_DISPLAY_MODE_LVGL` - LVGL 图形界面模式
- `APP_DISPLAY_MODE_CAMERA` - 摄像头预览模式

### 编译和运行

#### 1. 配置项目

项目使用配置文件 `app_default.config`：

```
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_EX_MODULE_35565LCD=y
CONFIG_ENABLE_EX_MODULE_CAMERA=y
CONFIG_ENABLE_LIBLVGL=y
```

#### 2. 构建项目

```bash
# 在项目根目录执行构建命令
cd tuyaopen_dev_ai_base_2_1
./build.sh examples/graphics/lvgl2Camera
```

#### 3. 烧录固件

将生成的固件烧录到 Tuya T5AI 开发板。

#### 4. 运行

- 系统启动后，默认显示 LVGL 界面（显示 "Hello World" 计数器）
- 单击功能按键切换到摄像头预览模式
- 再次单击按键切换回 LVGL 界面模式

### 使用说明

1. **初始状态**：设备启动后进入 LVGL 显示模式，屏幕显示 "Hello World" 和递增的计数器
2. **切换到摄像头**：按下功能按键，切换到摄像头预览模式，屏幕显示实时摄像头画面
3. **切换回 LVGL**：再次按下功能按键，返回 LVGL 界面模式
4. **内存监控**：主线程每3秒打印一次内存使用情况（SRAM/PSRAM）

### 内存使用

该项目使用双帧缓冲和摄像头缓冲区，内存需求如下：

- **摄像头帧缓冲**：`480 × 480 × 2 = 460.8 KB` (YUV422)
- **显示帧缓冲1**：`480 × 480 × 2 = 460.8 KB` (RGB565)
- **显示帧缓冲2**：`480 × 480 × 2 = 460.8 KB` (RGB565)
- **总计**：约 **1.35 MB**

如果开启 PSRAM (`ENABLE_EXT_RAM`)，上述缓冲区将分配在 PSRAM 中，否则使用 SRAM。

### 注意事项

1. 确保开发板已正确连接 LCD 屏幕和摄像头模块
2. 摄像头模块需要支持 YUV422 格式输出
3. 建议使用 PSRAM 配置以获得更好的内存管理
4. DMA2D 转换超时时间为 3 秒

---

## English Version

### Project Overview

lvgl2Camera is a demonstration project based on the Tuya T5AI development board that showcases switching between a graphical interface (LVGL) and camera preview. This project demonstrates how to simultaneously use the LVGL graphics library and camera module on embedded devices, with button-controlled switching between display modes.

This example has currently been tested only on the [T5AI Board](https://tuyaopen.ai/docs/hardware-specific/t5-ai-board/overview-t5-ai-board) with a 3.5" LCD screen and camera module.

### Key Features

- ✨ **LVGL Graphics Interface**: Simple UI displaying "Hello World" with a counter
- 📷 **Camera Preview**: Real-time camera preview (480x480 @ 20fps)
- 🔄 **Display Mode Switching**: Toggle between LVGL and camera preview using a button
- ⚡ **Hardware Acceleration**: DMA2D hardware acceleration for YUV422 to RGB565 conversion
- 🎯 **Double Buffering**: Uses double frame buffering for smooth, tear-free display
- 💾 **Flexible Memory Management**: Supports both PSRAM and SRAM memory allocation

### Hardware Requirements

- **Development Board**: Tuya T5AI Board
- **Display**: 3.5" LCD Screen (Model: 35565LCD)
- **Camera**: Compatible camera module
- **Button**: At least one function button for mode switching

### Technical Specifications

| Parameter | Value |
|-----------|-------|
| Camera Resolution | 480x480 |
| Frame Rate | 20 FPS |
| Image Format | YUV422 (camera) → RGB565 (display) |
| JPEG Encoding | Supported (quality range: 10-25) |
| Memory Management | PSRAM/SRAM supported |

### Project Structure

```
lvgl2Camera/
├── src/
│   ├── tuya_main.c          # Main program entry
│   ├── app_lvgl.c           # LVGL graphics management
│   ├── app_camera.c         # Camera management and image processing
│   └── app_button.c         # Button event handling
├── include/
│   ├── app_lvgl.h           # LVGL module header
│   ├── app_camera.h         # Camera module header
│   └── app_button.h         # Button module header
├── config/
│   └── TUYA_T5AI_BOARD_LCD_3.5.config  # 3.5" LCD configuration
├── CMakeLists.txt           # CMake build configuration
├── app_default.config       # Default configuration file
└── README.md                # Project documentation
```

### Module Description

#### 1. LVGL Graphics Module (`app_lvgl.c`)

- Initializes the LVGL graphics library
- Creates a simple UI displaying "Hello World" with a real-time counter
- Provides display start/stop control interface
- Uses a separate thread for UI updates with priority `THREAD_PRIO_0`

**Main Functions:**
- `app_lvgl_init()` - Initialize LVGL
- `app_lvgl_deinit()` - Deinitialize LVGL
- `app_lvgl_display_start()` - Start LVGL display
- `app_lvgl_display_stop()` - Stop LVGL display

#### 2. Camera Management Module (`app_camera.c`)

- Initializes camera device (480x480 @ 20fps)
- Real-time camera frame capture
- DMA2D hardware acceleration for YUV422 to RGB565 conversion
- Double buffering to prevent display tearing
- JPEG encoding support
- Asynchronous image processing via work queue

**Main Functions:**
- `app_camera_init()` - Initialize camera
- `app_camera_deinit()` - Deinitialize camera
- `app_camera_display_start()` - Start camera display
- `app_camera_display_stop()` - Stop camera display

**Technical Implementation:**
- Frame buffer memory alignment (4-byte aligned, primarily required by DMA2D)
- Double frame buffer pool (`sg_display_frame_buff[0]` and `sg_display_frame_buff[1]`)
- Callback mechanism for camera frame processing
- DMA2D hardware-accelerated format conversion (conversion addresses require 4-byte alignment)

#### 3. Button Control Module (`app_button.c`)

- Manages function button
- Handles button single-click events
- Switches between LVGL and camera modes
- Debouncing support (50ms)
- Long-press detection (3-second start, 1-second interval)

**Main Functions:**
- `app_button_init()` - Initialize button
- `app_button_deinit()` - Deinitialize button

**Display Modes:**
- `APP_DISPLAY_MODE_LVGL` - LVGL graphics mode
- `APP_DISPLAY_MODE_CAMERA` - Camera preview mode

### Build and Run

#### 1. Configure Project

Project uses configuration file `app_default.config`:

```
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_EX_MODULE_35565LCD=y
CONFIG_ENABLE_EX_MODULE_CAMERA=y
CONFIG_ENABLE_LIBLVGL=y
```

#### 2. Build Project

```bash
# Execute build command in project root directory
cd tuyaopen_dev_ai_base_2_1
./build.sh examples/graphics/lvgl2Camera
```

#### 3. Flash Firmware

Flash the generated firmware to the Tuya T5AI development board.

#### 4. Run

- After system startup, LVGL interface is displayed by default (showing "Hello World" counter)
- Click function button to switch to camera preview mode
- Click button again to switch back to LVGL interface mode

### Usage Instructions

1. **Initial State**: Device boots into LVGL display mode, screen shows "Hello World" with incrementing counter
2. **Switch to Camera**: Press function button to switch to camera preview mode, screen displays real-time camera feed
3. **Switch to LVGL**: Press function button again to return to LVGL interface mode
4. **Memory Monitoring**: Main thread prints memory usage every 3 seconds (SRAM/PSRAM)

### Memory Usage

This project uses double frame buffering and camera buffers, with the following memory requirements:

- **Camera Frame Buffer**: `480 × 480 × 2 = 460.8 KB` (YUV422)
- **Display Frame Buffer 1**: `480 × 480 × 2 = 460.8 KB` (RGB565)
- **Display Frame Buffer 2**: `480 × 480 × 2 = 460.8 KB` (RGB565)
- **Total**: Approximately **1.35 MB**

If PSRAM is enabled (`ENABLE_EXT_RAM`), these buffers are allocated in PSRAM; otherwise, SRAM is used.

### Important Notes

1. Ensure the development board is properly connected to the LCD screen and camera module
2. Camera module must support YUV422 format output
3. PSRAM configuration is recommended for better memory management
4. DMA2D conversion timeout is 3 seconds
