# LVGL 按钮演示

一个简单的 LVGL 演示项目，在屏幕上创建一个按钮。点击按钮可以在蓝色和白色之间切换屏幕背景颜色。

## 功能特性

- **LVGL 显示**: 在屏幕上显示一个按钮控件
- **交互式按钮**: 点击按钮切换背景颜色
- **颜色切换**: 在蓝色 (`0x0000FF`) 和白色 (`0xFFFFFF`) 背景之间切换
- **简洁界面**: 居中显示的按钮，界面简洁

## 前置要求

1. **显示板**: 支持 LCD 屏幕的开发板（例如：带 LCD 模块的 T5AI）
2. **触摸支持**（可选）：触摸屏用于按钮交互

## 配置

### 配置显示

编辑 `app_default.config`:

```config
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_EX_MODULE_35565LCD=y
CONFIG_ENABLE_LIBLVGL=y
CONFIG_LVGL_ENABLE_TP=y
CONFIG_ENABLE_LVGL_DEMO=y
```

### 配置显示驱动

运行 `tos menuconfig` 进行配置：
- 显示驱动（SPI/RGB/8080）
- 触摸驱动（如果支持触摸屏）
- 显示和触摸的 GPIO 引脚

## 编译和运行

```bash
# 配置项目
tos config_choice  # 选择您的开发板

# 配置显示（如需要）
tos menuconfig    # 配置显示驱动和引脚

# 编译
tos build

# 烧录到设备
```

## 使用方法

1. **上电**设备
2. **按钮显示**在屏幕上（居中，120x50 像素）
3. **点击按钮**切换背景颜色：
   - 第一次点击：背景变为蓝色
   - 第二次点击：背景变为白色
   - 每次点击都会继续切换

## 工作原理

1. **显示初始化**: LVGL 初始化并在屏幕上创建按钮控件
2. **按钮创建**: 创建一个按钮（120x50 像素）并居中显示
3. **事件处理**: 当按钮被点击时：
   - 触发事件回调函数 `button_event_cb`
   - 背景颜色在蓝色和白色之间切换
   - 使用静态布尔变量跟踪状态

## 代码结构

```
lvgl_button_demo/
├── CMakeLists.txt          # 构建配置
├── app_default.config      # 应用配置
└── src/
    └── example_lvgl.c      # 主应用程序（按钮创建和事件处理）
```

## 关键代码片段

### 按钮创建

```c
void lvgl_demo_button()
{
    // 创建按钮
    lv_obj_t * btn = lv_btn_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_center(btn);
    
    // 为按钮添加标签
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    // 注册点击事件回调
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
}
```

### 按钮点击事件处理

```c
void button_event_cb(lv_event_t * event)
{
    static bool is_blue = false;
    lv_obj_t * screen = lv_screen_active();
    
    if (is_blue) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);  // 白色
        is_blue = false;
    } else {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x0000FF), 0);  // 蓝色
        is_blue = true;
    }
}
```

## 支持平台

- **T5AI**: RGB/8080/SPI 接口
- **T3**: SPI 接口

## 支持驱动

### 屏幕
- SPI: ST7789, ILI9341, GC9A01
- RGB: ILI9488

### 触摸（可选）
- I2C: GT911, CST816, GT1511

## 故障排除

### 按钮不显示

- 检查 `app_default.config` 中的显示配置
- 验证 `tos menuconfig` 中是否配置了显示驱动
- 检查 LVGL 初始化日志
- 确保设置了 `CONFIG_ENABLE_LIBLVGL=y`

### 按钮点击无响应

- 如果使用触摸输入，验证触摸屏是否已配置
- 检查 `tos menuconfig` 中的触摸驱动配置
- 如果使用触摸，确保设置了 `CONFIG_LVGL_ENABLE_TP=y`
- 验证触摸的 GPIO 引脚是否正确配置

### 显示问题

- 检查显示接口配置（SPI/RGB/8080）
- 验证显示驱动是否与硬件匹配
- 检查 GPIO 引脚配置
- 查看显示初始化日志

## 技术支持

您可以通过以下方式获得涂鸦技术支持：

- TuyaOS 论坛: https://www.tuyaos.com
- 开发者中心: https://developer.tuya.com
- 帮助中心: https://support.tuya.com/help
- 技术支持工单中心: https://service.console.tuya.com
