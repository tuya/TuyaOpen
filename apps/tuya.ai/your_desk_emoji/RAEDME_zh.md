[English](./README.md) | 简体中文

# your_desk_emoji

[your_desk_emoji](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_desk_emoji) 是基于 tuya.ai 的智能桌面表情机器人。结合AI对话、手势识别、舵机控制、表情显示和天气信息，打造一个互动的桌面伙伴。

## 支持功能

1. **AI 智能对话**
   - 多种对话模式：按键对话、语音唤醒、自由对话
   - 支持不同唤醒词
   - LCD屏幕实时聊天内容显示

2. **手势识别**
   - 基于PAJ7620传感器
   - 支持方向手势（上、下、左、右、前、后）
   - 手势触发舵机动作和表情变化

3. **舵机电机控制**
   - 双舵机控制（垂直和水平运动）
   - 平滑运动动画
   - 基于手势和对话的互动动作

4. **表情显示系统**
   - 多种表情动画
   - 眨眼动画增强互动效果
   - 基于用户交互的实时表情切换

5. **天气时钟显示**
   - 从涂鸦云获取实时天气信息
   - 时间同步和显示
   - LCD显示日期和天气信息

6. **显示界面**
   - 多种UI风格：微信式、聊天机器人式、OLED
   - 实时聊天内容显示
   - 天气和时间信息显示

7. **网络配置**
   - 蓝牙网络配置
   - WiFi连接管理
   - 网络状态监控

## 依赖硬件能力

1. **音频系统**
   - 音频采集能力
   - 音频播放能力
   - 喇叭控制

2. **传感器接口**
   - I2C接口用于PAJ7620手势传感器
   - GPIO用于手势传感器中断

3. **电机控制**
   - PWM接口用于舵机电机控制
   - 双舵机支持（头部和身体运动）

4. **显示接口**
   - LCD显示用于表情动画和信息
   - 支持不同屏幕尺寸和类型

5. **用户界面**
   - 按键用于对话控制
   - LED指示灯用于状态显示

## 已支持硬件

| 型号 | 配置文件 | 说明 | 重置方式 |
| --- | --- | --- | ----- |
| TUYA T5AI Core | TUYA_T5AI_CORE.config | T5AI Core开发板，配备13565LCD显示屏 | 重启(按 RST 按钮) 3 次重置 |
| T5AI Mini (ST7735S) | app_default.config | T5AI Mini开发板，配备ST7735S彩色LCD显示屏 | 重启(按 RST 按钮) 3 次重置 |
| T5AI Mini (13565LCD) | T5AI_MINI_LCD.config | T5AI Mini开发板，配备13565LCD显示屏 | 重启(按 RST 按钮) 3 次重置 |


## 编译

1. **选择配置**：根据您的硬件选择合适的配置文件：
   - TUYA T5AI Core + 13565LCD：使用 `TUYA_T5AI_CORE.config`
   - T5AI Mini + ST7735S LCD：使用 `app_default.config`
   - T5AI Mini + 13565LCD：使用 `T5AI_MINI_LCD.config`

2. **应用配置**：运行 `tos.py config choice` 命令，选择当前运行的开发板。

3. **自定义设置**：如需修改配置，请先运行 `tos.py config menu` 命令，查看当前配置。` 命令修改配置。

4. **编译工程**：运行 `tos.py build` 命令，编译工程。

## 配置说明

### 默认配置
- 随意对话模式，未开启 AEC，不支持打断
- 唤醒词：
  - T5AI 版本：你好涂鸦
  - ESP32 版本：你好小智

### 通用配置

#### 对话模式选择

- **长按对话模式**
  - `ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL`：按住按键后说话，一句话说完后松开按键

- **按键对话模式**
  - `ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE`：按一下按键，设备会进入/退出聆听状态，支持VAD检测

- **唤醒对话模式**
  - `ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL`：需要说出唤醒词才能唤醒设备，进行单轮对话

- **随意对话模式**
  - `ENABLE_CHAT_MODE_ASR_WAKEUP_FREE`：需要说出唤醒词才能唤醒设备，支持连续对话（30秒超时）

#### 唤醒词选择
该配置只会在对话模式选择**唤醒对话**和**随意对话**两种模式下才会出现。

| 宏 | 类型 | 说明 |
| --- | --- | --- |
| `ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA` | 布尔 | 唤醒词是 "你好涂鸦" |
| `ENABLE_WAKEUP_KEYWORD_NIHAO_XIAOZHI` | 布尔 | 唤醒词是 "你好小智" |
| `ENABLE_WAKEUP_KEYWORD_XIAOZHI_TONGXUE` | 布尔 | 唤醒词是 "小智同学" |
| `ENABLE_WAKEUP_KEYWORD_XIAOZHI_GUANJIA` | 布尔 | 唤醒词是 "小智管家" |

#### 音频配置

- **是否支持 AEC**
  - `ENABLE_AEC`：根据板子的硬件是否有回声消除功能来配置

- **喇叭使能引脚**
  - `SPEAKER_EN_PIN`：该引脚控制喇叭是否使能，范围：0-64

- **对话按键引脚**
  - `CHAT_BUTTON_PIN`：控制对话的按键引脚，范围：0-64

- **指示灯引脚**
  - `CHAT_INDICATE_LED_PIN`：控制指示灯引脚，该指示灯主要用来显示对话状态，范围：0-64

#### 显示配置

- **使能显示**
  - `ENABLE_CHAT_DISPLAY`：使能显示功能，如果板子有带屏幕，可将该功能打开

- **选择显示 UI 风格**
  - `ENABLE_GUI_WECHAT`：类似微信聊天界面式风格
  - `ENABLE_GUI_CHATBOT`：聊天盒子式风格
  - `ENABLE_GUI_OLED`：滑动字幕，适合 OLED 小屏

- **使能文本流式显示**
  - `ENABLE_GUI_STREAM_AI_TEXT`：AI 回复的文本可进行流式的显示

- **选择OLED 屏类型**（当选择OLED UI风格时出现）
  - `OLED_SSD1306_128X32`：OLED 屏幕的尺寸大小为128*32
  - `OLED_SSD1306_128X64`：OLED 屏幕的尺寸大小为128*64

## 项目结构

```
src/
├── app_chat_bot.c          # AI对话管理
├── app_gesture.c           # PAJ7620手势识别
├── app_servo.c             # 舵机电机控制
├── app_weather.c           # 天气服务和数据
├── app_system_info.c       # 系统信息显示
├── tuya_main.c             # 主应用程序入口
├── display/                # 显示系统
│   ├── app_display.c       # 显示管理
│   ├── ui/                 # UI组件
│   │   ├── ui_emoji.c      # 表情显示
│   │   ├── ui_weather_clock.c # 天气时钟UI
│   │   ├── ui_chatbot.c    # 聊天机器人UI
│   │   ├── ui_wechat.c     # 微信风格UI
│   │   └── ui_oled.c       # OLED显示UI
│   └── font/               # 字体资源
└── media/                  # 媒体资源
```

## 使用方法

1. **初始设置**：配置开发板并编译项目
2. **网络配置**：使用蓝牙或按键重置进行WiFi设置
3. **交互使用**：使用语音唤醒或按键按压开始对话
4. **手势控制**：向不同方向挥手触发动作
5. **天气显示**：在显示屏上查看实时天气和时间信息

## 开发说明

- 项目支持多种对话模式，可根据硬件能力进行配置
- 手势识别需要PAJ7620传感器通过I2C连接
- 舵机控制支持双轴运动，用于头部和身体
- 天气信息每30分钟从涂鸦云获取一次
- 显示系统支持多种UI风格，适配不同屏幕类型