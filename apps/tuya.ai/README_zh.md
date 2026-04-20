[English](./README.md) | 简体中文

# tuya.ai

通过 tuya.ai 提供的 AI 能力，连接涂鸦云和 AI 服务，实现 AI 语音交互、音频/视频多模态 AI 交互、智能化控制等功能。

## 快速开始

### 1. 创建产品并获取产品 PID

参考文档 [创建产品](https://tuyaopen.ai/zh/docs/cloud/tuya-cloud/creating-new-product) 在 [涂鸦 IoT 平台](https://iot.tuya.com) 下创建产品，并获取到创建产品的 PID。

然后替换对应工程目录下 `include/tuya_config.h` 文件中 `TUYA_PRODUCT_ID` 宏为实际的 PID。

### 2. 确认 TuyaOpen 授权码

TuyaOpen Framework 包括：
- C 版 TuyaOpen：[https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- Arduino 版 TuyaOpen：[https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- Luanode 版 TuyaOpen：[https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

均采用 TuyaOpen 专用授权码（UUID + AuthKey），使用其他授权码无法正常连接涂鸦云。

可通过以下方式获取 TuyaOpen 专用授权码：

- **方式 1**：购买已烧录 TuyaOpen 授权码的模组。授权码已在出厂时写入模组中，不会丢失。设备启动时通过 `tuya_authorize_read()` 接口读取。

- **方式 2**：通过 [涂鸦生产平台](https://platform.tuya.com/purchase/index?type=6) 购买 **TuyaOpen 授权码**，然后修改对应工程的 `include/tuya_config.h` 文件：

  ```c
  #define TUYA_OPENSDK_UUID    "uuidxxxxxxxxxxxxxxxx"             // 替换为实际的 uuid
  #define TUYA_OPENSDK_AUTHKEY "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" // 替换为实际的 authkey
  ```

- **方式 3**：通过 [淘宝店铺](https://item.taobao.com/item.htm?ft=t&id=911596682625) 购买 **TuyaOpen 授权码**，替换方式同上。

![authorization_code](../../docs/images/zh/authorization_code.png)

代码中的授权读取逻辑：

```c
tuya_iot_license_t license;

if (OPRT_OK != tuya_authorize_read(&license)) {
    license.uuid    = TUYA_OPENSDK_UUID;
    license.authkey = TUYA_OPENSDK_AUTHKEY;
    PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work");
}
```

> 如 `tuya_authorize_read()` 接口返回 OPRT_OK，则表示当前设备已烧录 TuyaOpen 授权码；否则将使用 `tuya_config.h` 中定义的默认值。

### 3. 编译烧录

```bash
# 进入目标工程目录
cd apps/tuya.ai/your_chat_bot

# 选择开发板配置
tos.py config choice

# 编译工程
tos.py build

# 烧录固件
tos.py flash
```

更多详情请参考官方文档：[项目编译](https://tuyaopen.ai/zh/docs/quick-start/project-compilation) | [固件烧录](https://tuyaopen.ai/zh/docs/quick-start/firmware-burning)

### 4. 设备授权

烧录固件后需要将授权码写入设备。两种方式：

- **命令授权**：使用 `tos.py monitor -b 115200` 进入串口，输入 `auth <uuid> <authkey>` 命令写入授权信息。
- **代码预置**：在编译前修改 `include/tuya_config.h` 中的 `TUYA_OPENSDK_UUID` 和 `TUYA_OPENSDK_AUTHKEY`，重新编译烧录。

详细步骤请参考：[设备授权](https://tuyaopen.ai/zh/docs/quick-start/equipment-authorization)

### 5. 设备配网

使用涂鸦智能生活 APP 配网激活设备后，即可进行 AI 语音交互。

详细步骤请参考：[设备手机配网](https://tuyaopen.ai/zh/docs/quick-start/device-network-configuration)

> **注意**：目前仅支持连接 2.4 GHz 频段路由器，5 GHz 频段会导致配网失败。

---

## AI 应用列表

| 应用 | 说明 |
| --- | --- |
| [your_chat_bot](./your_chat_bot/) | AI 智能聊天机器人，支持语音对话、表情显示、LCD 实时聊天 |
| [your_serial_chat_bot](./your_serial_chat_bot/) | 串口文本 AI 聊天机器人，仅需最小核心板即可实现 AI 对话 |
| [your_robot_dog](./your_robot_dog/) | AI 聊天机器狗，支持表情动作和舵机控制 |
| [your_desk_emoji](./your_desk_emoji/) | 智能桌面表情机器人，支持手势识别和天气显示 |
| [your_otto_robot](./your_otto_robot/) | Otto 人形机器人，支持多功能扩展和远程控制 |

### 支持芯片与平台

| 芯片/平台 | 说明 |
| --- | --- |
| [Tuya T5AI](https://tuyaopen.ai/zh/docs/hardware-specific/tuya-t5/t5-ai-board/overview-t5-ai-board) | Wi-Fi + BLE 5.4 双模，ARMv8-M 480MHz，16MB RAM |
| ESP32S3 | 乐鑫 ESP32-S3 系列 |
| LINUX | 树莓派、DshanPi 等 Linux 开发板 |

### 支持开发板列表

| 开发板 | 芯片 | 说明 |
| --- | --- | --- |
| TUYA_T5AI_BOARD | T5AI | [涂鸦 T5AI 3.5 寸 LCD 开发板](https://tuyaopen.ai/zh/docs/hardware-specific/tuya-t5/t5-ai-board/overview-t5-ai-board) |
| TUYA_T5AI_EVB | T5AI | [T5AI EVB 开发板](https://oshwhub.com/flyingcys/t5ai_evb) |
| TUYA_T5AI_CORE | T5AI | T5AI Core 核心板(https://tuyaopen.ai/zh/docs/hardware-specific/tuya-t5/t5-ai-core/overview-t5-ai-core) |
| T5AI_MINI | T5AI | T5AI Mini 开发板 |
| T5AI_MOJI_1.28 | T5AI | Moji 1.28 寸屏开发板 |
| WAVESHARE_T5AI_TOUCH_AMOLED_1.75 | T5AI | 微雪 T5AI 1.75 寸触摸 AMOLED |
| DNESP32S3_BOX | ESP32S3 | [正点原子 ESP32S3 BOX](https://www.alientek.com/Product_Details/118.html) |
| ESP32S3_BREAD_COMPACT_WIFI | ESP32S3 | ESP32S3 面包板 |
| WAVESHARE_ESP32S3_Touch_AMOLED_1.8 | ESP32S3 | [微雪 ESP32S3 1.8 寸触摸 AMOLED](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm) |
| XINGZHI_ESP32S3_Cube_0.96_OLED | ESP32S3 | [星智 0.96 寸 OLED 开发板](https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html) |
| DshanPi_A1 | LINUX | 百问网 DshanPi-A1 |
| Raspberry Pi | LINUX | 树莓派 |

---

## 免费赠送 TuyaOpen 授权码活动

为了让开发者可以自由体验 TuyaOpen Framework，现在只要在 GitHub 上给以下仓库加 star：
- [https://github.com/tuya/TuyaOpen](https://github.com/tuya/TuyaOpen)
- [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)

凭 GitHub 账号和截图，发送邮件至 `chenyisong@tuya.com` 或加入 QQ 群 `796221529` 向群主免费领取一个 TuyaOpen Framework 专用授权码。

限量 500 个，先到先得，送完即止，赶紧扫码加群来领：

![qq_qrcode](../../docs/images/zh/qq_qrcode.png)
