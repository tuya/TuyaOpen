[English](./README.md) | 简体中文

# your_chat_bot_led_ctl
 [your_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot) 是基于 tuya.ai 开源的大模型智能聊天机器人。通过麦克风采集语音，语音识别，实现对话、互动、调侃，还能通过屏幕看到实时聊天内容。

**本版本新增了 LED PWM 控制功能**，可通过云端或本地触摸按键控制 LED 灯光。

**注意：在 TUYA AI V1.0 和 V2.0 之间切换时，需要先在 APP 上删除设备并清除数据后再使用。**

## 支持功能

1. AI 智能对话
2. 按键唤醒/语音唤醒, 回合制对话，支持语音打断（需硬件支持）
3. 表情显示
4. 支持 LCD 显示实时聊天内容、支持 APP 端实时查看聊天内容
5. 蓝牙配网快捷连接路由器
6. APP 端实时切换 AI 智能体角色
7. **LED PWM 控制** - 通过云端或本地触摸按键控制 LED 开关和亮度

![](../../../docs/images/apps/your_chat_bot.png)

## LED PWM 控制功能

### 云端控制
- **开关控制（DP ID: 20）**：通过涂鸦 IoT 云端控制 LED 开关
- **亮度控制（DP ID: 22）**：通过涂鸦 IoT 云端调节 LED 亮度（0-100%）

### 本地触摸按键控制
- **短按**：切换 LED 开关状态
- **长按（400ms 以上）**：逐级增加亮度，达到最大值后自动回绕

### 技术规格
- **PWM 通道**：PWM5（P9 引脚）
- **PWM 频率**：100Hz（周期：10ms）
- **占空比范围**：0-10000（对应 0% - 100%）
- **占空比步进**：每次调整 100
- **触摸按键引脚**：GPIO29
- **PWM 极性**：负极性

## 依赖硬件能力
1. 音频采集
2. 音频播放
3. **支持 PWM 的 GPIO 引脚（P9/PWM5）**用于 LED 控制
4. **GPIO 引脚（GPIO29）**用于触摸按键控制（可选，用于本地控制）

## 已支持硬件
|  型号  | config | 说明 | 重置方式 |
| --- | --- | --- | ----- |
| TUYA T5AI_Board 开发板 | TUYA_T5AI_BOARD_LCD_3.5.config | [https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | 重启(按 RST 按钮) 3 次重置 |
| TUYA T5AI_EVB 开发板 | TUYA_T5AI_EVB.config | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | 重启(按 RST 按钮) 3 次重置 |
| moji T5AI 版 | T5AI_MOJI_1.28.config |  | 重启(按 RST 按钮) 3 次重置 |
| 正点原子 ESP32S3BOX | DNESP32S3_BOX.config | [https://www.alientek.com/Product_Details/118.html](https://www.alientek.com/Product_Details/118.html) | 重启(按 RST 按钮) 3 次重置 |
| ESP32S3 面包板 | ESP32S3_BREAD_COMPACT_WIFI.config |  | 重启(按 RST 按钮) 3 次重置 |
| waveshare ESP32S3 1.8 英寸触摸 AMOLED 开发板 | WAVESHARE_ESP32S3_TOUCH_AMOLED_1_8.config | [https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm) | 重启(按 RST 按钮) 3 次重置 |
| ESP32S3 星智 0.96 OLED 开发板 | XINGZHI_Cube_0_96OLED_WIFI.config | [https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html](https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html) | 重启(按 RST 按钮) 3 次重置 |


## 编译
1. 运行 `tos config_choice` 命令， 选择当前运行的开发板。
2. 如需修改配置，请先运行 `tos menuconfig` 命令修改配置。
3. 运行 `tos build` 命令，编译工程。

## 配置说明

### 默认配置
- 随意对话模式，未开启 AEC，不支持打断
- 唤醒词：
  - T5AI 版本： 你好涂鸦
  - ESP32 版本：你好小智

### 通用配置

- **选择对话模式**

  - 长按对话模式

    | 宏                                     | 类型 | 说明                                   |
    | -------------------------------------- | ---- | -------------------------------------- |
    | ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL | 布尔 | 按住按键后说话，一句话说完后松开按键。 |

  - 按键对话模式

    | 宏                                 | 类型 | 说明                                                         |
    | ---------------------------------- | ---- | ------------------------------------------------------------ |
    | ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE | 布尔 | 按一下按键，设备会进入/退出聆听状态。如果在聆听状态，会开启 vad 检测，此时可以进行对话。 |

  - 唤醒对话模式

    | 宏                                 | 类型 | 说明                                                         |
    | ---------------------------------- | ---- | ------------------------------------------------------------ |
    | ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL | 布尔 | 需要说出唤醒词才能唤醒设备，设备唤醒后会进入聆听状态，此时可以进行对话。每次唤醒只能进行一轮对话。如果想继续对话，需要再次用唤醒词唤醒。 |

  - 随意对话模式

    | 宏                               | 类型 | 说明                                                         |
    | -------------------------------- | ---- | ------------------------------------------------------------ |
    | ENABLE_CHAT_MODE_ASR_WAKEUP_FREE | 布尔 | 需要说出唤醒词才能唤醒设备，设备唤醒后会进入聆听状态，此时可以进行随意对话。如果 30S 没有检测到声音，则需要再次唤醒。 |

- **选择唤醒词**

  该配置只会在对话模式选择**唤醒对话**和**随意对话**两种模式下才会出现。

  | 宏                                    | 类型 | 说明                |
  | ------------------------------------- | ---- | ------------------- |
  | ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA      | 布尔 | 唤醒词是 “你好涂鸦” |
  | ENABLE_WAKEUP_KEYWORD_NIHAO_XIAOZHI   | 布尔 | 唤醒词是 “你好小智” |
  | ENABLE_WAKEUP_KEYWORD_XIAOZHI_TONGXUE | 布尔 | 唤醒词是 “小智同学” |
  | ENABLE_WAKEUP_KEYWORD_XIAOZHI_GUANJIA | 布尔 | 唤醒词是 “小智管家” |

- **是否支持 AEC**

  | 宏         | 类型 | 说明                                                         |
  | ---------- | ---- | ------------------------------------------------------------ |
  | ENABLE_AEC | 布尔 | 这个是根据板子的硬件是否有回声消除功能来配置。<br />如果板子支持回声消除，则把该配置打开。**如果板子不支持回声消除，则需要关闭该功能，否则会影响唤醒对话功能**。<br />该配置没打开，则不支持语音打断的功能。 |

- **喇叭使能引脚**

  | 宏             | 类型 | 说明                                 |
  | -------------- | ---- | ------------------------------------ |
  | SPEAKER_EN_PIN | 数值 | 该引脚控制喇叭是否使能，范围：0-64。 |

- **对话按键引脚**

  | 宏              | 类型 | 说明                             |
  | --------------- | ---- | -------------------------------- |
  | CHAT_BUTTON_PIN | 数值 | 控制对话的按键引脚，范围：0-64。 |

- **指示灯引脚**

  | 宏                    | 类型 | 说明                                                       |
  | --------------------- | ---- | ---------------------------------------------------------- |
  | CHAT_INDICATE_LED_PIN | 数值 | 控制指示灯引脚，该指示灯主要用来显示对话状态，范围：0-64。 |

- **使能显示**

  | 宏                  | 类型 | 说明                                             |
  | ------------------- | ---- | ------------------------------------------------ |
  | ENABLE_CHAT_DISPLAY | 布尔 | 使能显示功能，如果板子有带屏幕，可将该功能打开。 |

### 显示配置

显示使能被打开后，以下配置才会出现。

- **选择显示 UI 风格**

  | 宏                 | 类型 | 说明                     |
  | ------------------ | ---- | ------------------------ |
  | ENABLE_GUI_WECHAT  | 布尔 | 类似微信聊天界面式风格   |
  | ENABLE_GUI_CHATBOT | 布尔 | 聊天盒子式风格           |
  | ENABLE_GUI_OLED    | 布尔 | 滑动字幕，适合 oled 小屏 |

- **使能文本流式显示**

  | 宏                        | 类型 | 说明                                                      |
  | ------------------------- | ---- | --------------------------------------------------------- |
  | ENABLE_GUI_STREAM_AI_TEXT | 布尔 | AI 回复的文本可进行流式的显示，而不是一下子出现文本内容。 |

- **选择OLED 屏类型**

  该配置只有在选择 OLED UI 风格时才会出现。

  | 宏                  | 类型 | 说明                        |
  | ------------------- | ---- | --------------------------- |
  | OLED_SSD1306_128X32 | 布尔 | oled 屏幕的尺寸大小为128*32 |
  | OLED_SSD1306_128X64 | 布尔 | oled 屏幕的尺寸大小为128*64 |

### LED PWM 配置

可以在 `src/pwm_led_ctrl.c` 中修改 LED 控制参数：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `LED_PWM_VERTICAL` | `TUYA_PWM_NUM_5` | 用于 LED 控制的 PWM 通道 |
| `LED_PWM_FREQ` | `100` | PWM 频率（Hz） |
| `TOUCHE_KEY_PIN` | `TUYA_GPIO_NUM_29` | 触摸按键 GPIO 引脚 |
| `LONG_KEY_TIME` | `400` | 长按检测时间（毫秒） |
| `PWM_DUTY_STEP` | `100` | 亮度调整步进值 |

### 数据点 ID

LED 控制使用以下数据点 ID（定义在 `include/pwm_led_ctrl.h` 中）：

- **开关控制**：DP ID 20（BOOL 类型）
- **亮度控制**：DP ID 22（VALUE 类型，范围：0-100）

### 使用方法

1. **云端控制**：使用涂鸦智能 APP 或云端 API 发送命令：
   - 开关：向 DP ID 20 发送布尔值（true = 开，false = 关）
   - 亮度：向 DP ID 22 发送整数值（0-100）

2. **本地触摸按键控制**：
   - 短按触摸按键（GPIO29）切换 LED 开关状态
   - 长按（>400ms）逐级增加亮度

3. **初始化**：在设备初始化时调用 `app_led_contral_task()` 函数启动 LED PWM 控制和触摸按键监控。

## 文件结构

```
your_chat_bot_led_ctl/
├── src/
│   ├── pwm_led_ctrl.c      # LED PWM 控制实现
│   └── tuya_main.c         # 主应用程序，包含 DP 处理函数
├── include/
│   └── pwm_led_ctrl.h      # LED 控制 API 定义
└── README.md               # 本文档
```

## API 参考

### `int app_set_led_onoff(bool led_state)`
设置 LED 开关状态。

**参数：**
- `led_state`：`true` 表示打开，`false` 表示关闭

**返回值：** 操作结果代码

### `int app_set_led_brightness(INT_T value)`
设置 LED 亮度。

**参数：**
- `value`：亮度值（0-100）

**返回值：** 操作结果代码

### `OPERATE_RET app_led_contral_task(VOID)`
初始化 LED PWM 控制和触摸按键监控。此函数应在设备初始化时调用。

**返回值：** 成功时返回 `OPRT_OK`

## 故障排除

1. **LED 无响应**：检查 PWM 通道和 GPIO 引脚是否已为您的硬件正确配置
2. **触摸按键不工作**：确认 GPIO29 可用且未被其他外设占用
3. **亮度控制问题**：确保 PWM 频率和占空比值在硬件限制范围内

## 许可证

请查看仓库根目录中的 LICENSE 文件。