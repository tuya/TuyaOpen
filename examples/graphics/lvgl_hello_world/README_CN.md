# LVGL Hello World 示例

## 概述

本示例演示了一个简单的 LVGL（轻量级多功能图形库）应用程序，在屏幕上显示一个按钮。当点击按钮时，会弹出一个显示 "hello world" 的消息提示框。

## 功能特性

- **按钮 UI**：在白色背景上居中显示一个标签为 "Button" 的按钮
- **消息提示框**：点击按钮时弹出模态消息提示框
- **清晰的架构**：UI 代码与主应用程序代码分离

## 项目结构

```
lvgl_hello_world/
├── src/
│   ├── tuya_main.c          # 主应用程序入口
│   └── ui_hello_world.c     # UI 实现（按钮和消息框）
├── include/
│   └── ui_hello_world.h     # UI 接口头文件
├── CMakeLists.txt           # 构建配置
├── app_default.config       # 应用程序配置
└── README.md                # 本文件
```

## 配置

示例需要在 `app_default.config` 中配置以下选项：

- `CONFIG_ENABLE_LIBLVGL=y` - 启用 LVGL 库
- `CONFIG_LVGL_ENABLE_TP=y` - 启用触摸屏支持
- `CONFIG_LV_USE_BUTTON=y` - 启用按钮组件
- `CONFIG_LV_USE_MSGBOX=y` - 启用消息框组件

## 使用方法

1. **编译项目**：
   ```bash
   # 配置并编译
   ./build.sh lvgl_hello_world
   ```

2. **烧录并运行**：
   - 将固件烧录到设备
   - 设备将显示一个标签为 "Click Me" 的按钮

3. **交互**：
   - 点击屏幕上的按钮
   - 将弹出一个显示 "hello world" 的消息提示框
   - 点击关闭按钮（X）可以关闭消息提示框

## 代码结构

### UI 模块 (`ui_hello_world.c`)

UI 模块处理所有视觉组件：

- **`ui_hello_world_init()`**：初始化 UI，创建按钮
- **`button_click_event_cb()`**：处理按钮点击事件
- **`show_hello_world_msgbox()`**：创建并显示消息提示框
- **`msgbox_close_event_cb()`**：处理消息框关闭按钮事件

### 主模块 (`tuya_main.c`)

主模块处理应用程序初始化：

- 初始化日志
- 注册硬件
- 初始化 LVGL vendor
- 调用 UI 初始化
- 启动 LVGL 任务

## 自定义

您可以通过修改以下内容来自定义示例：

- **按钮文本**：在 `ui_hello_world_init()` 中修改 `"Click Me"`
- **消息框标题**：在 `show_hello_world_msgbox()` 中修改 `"Hello"`
- **消息框文本**：在 `show_hello_world_msgbox()` 中修改 `"hello world"`
- **按钮大小**：修改 `lv_obj_set_size()` 参数
- **颜色**：添加 `lv_obj_set_style_bg_color()` 调用

## 要求

- TuyaOpen SDK
- LVGL 库（v9）
- 带触摸支持的显示硬件
- 配置了 LCD 模块的开发板

## 注意事项

- 消息框是模态的（会阻止与其他 UI 元素的交互）
- 一次只能显示一个消息框
- 示例使用 LVGL v9 API（`lv_button_create`、`lv_msgbox_create` 等）

