# GUI应用开发指南

## 项目功能简介

`your_chat_bot_multi_app` 是一个基于 Tuya IoT 平台的智能聊天机器人多应用系统，集成了丰富的GUI界面和功能模块。
![](./src/display/image/home_page.jpg)
![](./src/display/image/draw_page.jpg)
![](./src/display/image/weather_page.jpg)
### 核心功能

1. **智能聊天机器人**
   - AI语音对话交互
   - 多种对话模式（按键、唤醒词等）
   - 实时聊天内容显示

2. **多应用界面系统**
   - 主页（HomePage）：应用入口、时间显示、WiFi状态
   - 设置页（SettingPage）：系统设置、亮度、音量调节
   - 天气页（WeatherPage）：实时天气信息显示
   - 日历页（CalendarPage）：日历查看
   - 游戏应用：
     - 2048游戏（Game2048Page）
     - 记忆翻牌游戏（GameMemoryPage）
     - 恐龙跳跃游戏（GameDinoPage）
     - 木鱼敲击（GameMuyuPage）
   - 工具应用：
     - 计算器（CalculatorPage）
     - 绘画板（DrawPage）
   - 摄像头页（CameraPage）：摄像头功能

3. **页面管理系统**
   - 基于栈的页面导航
   - 支持手势滑动返回
   - 页面生命周期管理（init/deinit）

4. **UI框架**
   - 基于 LVGL v9 图形库
   - 支持触摸交互
   - 丰富的UI组件和动画效果

---

## 如何新增自定义页面

本指南将详细介绍如何在 `gui_app/pages` 目录下创建新的页面。

### 步骤1：创建页面目录和文件

在 `src/display/gui_app/pages/` 目录下创建新的页面目录，例如 `ui_MyPage`：

```bash
mkdir -p src/display/gui_app/pages/ui_MyPage
```

创建以下两个文件：

#### 1.1 创建头文件 `ui_MyPage.h`

```c
#ifndef _UI_MYPAGE_H
#define _UI_MYPAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../ui.h"

void ui_MyPage_init(void);
void ui_MyPage_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
```

#### 1.2 创建源文件 `ui_MyPage.c`

```c
#include "ui_MyPage.h"
#include "tal_log.h"
#include "lvgl.h"

///////////////////// VARIABLES ////////////////////

lv_obj_t * ui_MyPage;

///////////////////// FUNCTIONS ////////////////////

// 手势事件处理：支持左右滑动返回上一页
static void ui_event_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || 
           lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

///////////////////// SCREEN init ////////////////////

void ui_MyPage_init(void)
{
    // 创建页面对象
    ui_MyPage = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_MyPage, LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置页面背景色
    lv_obj_set_style_bg_color(ui_MyPage, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MyPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 创建UI元素（示例：创建一个标签）
    lv_obj_t *label = lv_label_create(ui_MyPage);
    lv_label_set_text(label, "这是我的自定义页面");
    lv_obj_center(label);
    
    // 添加手势事件（可选，用于返回上一页）
    lv_obj_add_event_cb(ui_MyPage, ui_event_Gesture, LV_EVENT_GESTURE, NULL);
    
    // 关键：加载页面到屏幕
    lv_scr_load_anim(ui_MyPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_MyPage_deinit(void)
{
    // 清理资源（如果有动态分配的内存，在这里释放）
    // 例如：tal_free(...)
}
```

### 步骤2：在 `ui.c` 中注册新页面

编辑 `src/display/gui_app/ui.c` 文件：

#### 2.1 添加头文件包含

在文件顶部添加：

```c
#include "./pages/ui_MyPage/ui_MyPage.h"
```

#### 2.2 在 `ui_apps` 数组中注册页面

找到 `ui_apps` 数组定义，添加新页面：

```c
ui_app_data_t ui_apps[] = 
{
    // ... 其他页面 ...
    {
        .name = "MyPage",                    // 页面名称（用于导航）
        .init = ui_MyPage_init,              // 初始化函数
        .deinit = ui_MyPage_deinit,          // 销毁函数
        .page_obj = NULL
    },
};
```

**注意**：确保 `_APP_NUMS` 宏的值与 `ui_apps` 数组的实际元素数量一致。

### 步骤3：在 CMakeLists.txt 中添加源文件

编辑 `src/display/CMakeLists.txt` 文件，在 `PAGES_SRCS` 列表中添加新页面的源文件：

```cmake
set(PAGES_SRCS
    # ... 其他页面 ...
    ${APP_MODULE_PATH}/gui_app/pages/ui_MyPage/ui_MyPage.c
)
```

**重要提示**：如果页面有额外的源文件（如 `app_MyPage.c`），也需要添加到列表中：

```cmake
set(PAGES_SRCS
    # ... 其他页面 ...
    ${APP_MODULE_PATH}/gui_app/pages/ui_MyPage/ui_MyPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_MyPage/app_MyPage.c  # 如果有额外源文件
)
```

### 步骤4：在主页添加入口按钮（可选）

如果希望从主页进入新页面，需要编辑 `ui_HomePage.c`：

#### 4.1 创建按钮

在 `ui_HomePage_init()` 函数中添加按钮创建代码：

```c
lv_obj_t * ui_MyPageBtn = lv_btn_create(ui_AppContainer);
// 设置按钮样式、位置等
lv_obj_align(ui_MyPageBtn, LV_ALIGN_CENTER, 0, 0);
lv_obj_set_size(ui_MyPageBtn, 80, 80);

// 添加按钮标签
lv_obj_t * btn_label = lv_label_create(ui_MyPageBtn);
lv_label_set_text(btn_label, "我的页面");
lv_obj_center(btn_label);

// 添加点击事件
lv_obj_add_event_cb(ui_MyPageBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "MyPage");
```

**注意**：`"MyPage"` 必须与 `ui.c` 中注册的页面名称一致。

---

## 页面开发最佳实践

### 1. 页面结构规范

每个页面应包含以下部分：

```c
///////////////////// VARIABLES ////////////////////
// 全局变量定义

///////////////////// ANIMATIONS ////////////////////
// 动画相关代码（可选）

///////////////////// FUNCTIONS ////////////////////
// 辅助函数、事件处理函数

///////////////////// SCREEN init ////////////////////
// 页面初始化函数

/////////////////// SCREEN deinit ////////////////////
// 页面销毁函数
```

### 2. 必须实现的函数

- **`ui_XXXPage_init(void)`**：页面初始化
  - 创建页面对象
  - 创建UI元素
  - 绑定事件处理
  - **必须调用** `lv_scr_load_anim()` 加载页面

- **`ui_XXXPage_deinit(void)`**：页面销毁
  - 释放动态分配的内存
  - 清理资源

### 3. 手势返回功能

建议为每个页面添加手势返回功能：

```c
static void ui_event_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || 
           lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

// 在 init 函数中绑定：
lv_obj_add_event_cb(ui_MyPage, ui_event_Gesture, LV_EVENT_GESTURE, NULL);
```

### 4. 内存管理

- 使用静态变量存储页面对象指针
- 如果使用动态内存分配（如 `tkl_system_psram_malloc`），必须在 `deinit` 中释放
- 大缓冲区建议使用动态分配，减少静态内存占用

### 5. 字体使用

如果需要使用自定义字体：

```c
// 在文件顶部声明字体
LV_FONT_DECLARE(font_puhui_18_2);

// 在创建标签后设置字体
lv_obj_set_style_text_font(label, &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
```

### 6. 页面导航

从其他页面跳转到新页面：

```c
// 使用页面管理器打开指定页面
lv_lib_pm_OpenPage(&page_manager, "MyPage");
```

返回上一页：

```c
// 使用页面管理器返回上一页
lv_lib_pm_OpenPrePage(&page_manager);
```

---

## 完整示例：创建一个简单的"关于"页面

### 文件结构

```
src/display/gui_app/pages/ui_AboutPage/
├── ui_AboutPage.h
└── ui_AboutPage.c
```

### ui_AboutPage.h

```c
#ifndef _UI_ABOUTPAGE_H
#define _UI_ABOUTPAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../ui.h"

void ui_AboutPage_init(void);
void ui_AboutPage_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
```

### ui_AboutPage.c

```c
#include "ui_AboutPage.h"
#include "tal_log.h"
#include "lvgl.h"

LV_FONT_DECLARE(font_puhui_18_2);

///////////////////// VARIABLES ////////////////////

lv_obj_t * ui_AboutPage;

///////////////////// FUNCTIONS ////////////////////

static void ui_event_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || 
           lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

///////////////////// SCREEN init ////////////////////

void ui_AboutPage_init(void)
{
    ui_AboutPage = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_AboutPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_AboutPage, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AboutPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 创建标题
    lv_obj_t * title = lv_label_create(ui_AboutPage);
    lv_label_set_text(title, "关于");
    lv_obj_set_style_text_font(title, &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 创建内容
    lv_obj_t * content = lv_label_create(ui_AboutPage);
    lv_label_set_text(content, "Tuya IoT 智能设备\n版本: 1.0.0\n基于 LVGL v9");
    lv_obj_set_style_text_font(content, &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    
    // 添加手势事件
    lv_obj_add_event_cb(ui_AboutPage, ui_event_Gesture, LV_EVENT_GESTURE, NULL);
    
    // 加载页面
    lv_scr_load_anim(ui_AboutPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_AboutPage_deinit(void)
{
    // 无需特殊清理
}
```

### 在 ui.c 中注册

```c
#include "./pages/ui_AboutPage/ui_AboutPage.h"

// ...

ui_app_data_t ui_apps[] = 
{
    // ... 其他页面 ...
    {
        .name = "AboutPage",
        .init = ui_AboutPage_init,
        .deinit = ui_AboutPage_deinit,
        .page_obj = NULL
    },
};
```

### 在 CMakeLists.txt 中添加

```cmake
set(PAGES_SRCS
    # ... 其他页面 ...
    ${APP_MODULE_PATH}/gui_app/pages/ui_AboutPage/ui_AboutPage.c
)
```

---

## 常见问题

### Q1: 页面不显示怎么办？

**A:** 确保在 `init` 函数中调用了 `lv_scr_load_anim()`：

```c
lv_scr_load_anim(ui_MyPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
```

### Q2: 编译报错 "undefined reference to xxx"

**A:** 检查以下几点：
1. 源文件是否添加到 `CMakeLists.txt` 的 `PAGES_SRCS` 中
2. 函数声明和定义是否匹配
3. 头文件是否被正确包含

### Q3: 如何从其他页面跳转到新页面？

**A:** 使用页面管理器：

```c
lv_lib_pm_OpenPage(&page_manager, "MyPage");
```

### Q4: 如何添加按钮点击事件？

**A:** 

```c
static void ui_event_btn_clicked(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        // 处理点击事件
        PR_DEBUG("Button clicked!");
    }
}

// 在创建按钮后绑定事件
lv_obj_add_event_cb(btn, ui_event_btn_clicked, LV_EVENT_CLICKED, NULL);
```

### Q5: 如何使用系统参数？

**A:** 系统参数存储在全局变量 `ui_system_para` 中：

```c
// 读取亮度
uint16_t brightness = ui_system_para.brightness;

// 读取音量
uint16_t sound = ui_system_para.sound;

// 读取位置信息
char *city = ui_system_para.location.city;
```

### Q6: 如何获取天气信息？

**A:** 使用天气服务API（需要先实现 `get_weather_info` 函数）：

```c
#include "app_WeatherPage.h"

WeatherInfo_t weather_info;
if(get_weather_info(&weather_info) == 0)
{
    // 使用天气信息
    PR_DEBUG("Weather: %s, Temp: %s°C", 
             weather_info.weather, 
             weather_info.temperature);
}
```

---

## 参考资源

- **LVGL 官方文档**: https://docs.lvgl.io/
- **Tuya IoT 开发文档**: https://developer.tuya.com/
- **现有页面示例**:
  - 简单页面：`ui_CameraPage`
  - 复杂页面：`ui_HomePage`
  - 游戏页面：`ui_Game2048Page`
  - 工具页面：`ui_CalculatorPage`
  - 模板页面：`ui_template`

---

## 注意事项

1. **命名规范**：页面目录和文件使用 `ui_XXXPage` 格式
2. **内存优化**：大缓冲区使用动态分配（`tkl_system_psram_malloc`）
3. **错误处理**：添加必要的空指针检查和错误处理
4. **代码风格**：遵循现有代码风格，保持一致性
5. **测试**：新增页面后务必测试编译和运行

---

**祝开发愉快！** 🚀

