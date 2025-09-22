[English](./README.md) | 简体中文

# your_serial_chat_bot
[your_serial_chat_bot](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_serial_chat_bot) 是基于 tuya.ai 开源的大模型智能聊天机器人。通过串口从外部设备（如PC）接收文本对话，将内容上报云端进行AI处理，并通过串口将AI响应返回给外部设备。**仅需要最小核心板就可以实现AI对话！！！**

## 支持功能

1. 通过UART进行AI智能对话
2. 基于文本的对话模式，无需语音处理
3. 通过串口实时传输聊天内容
4. 蓝牙配网快捷连接路由器
5. APP端实时切换AI智能体角色

## 依赖硬件能力
1. UART通信接口
2. 网络连接能力（Wi-Fi或以太网）

## 已支持硬件
| 型号 | 说明 | 重置方式 |
| --- | --- | --- |
| TUYA T5AI_Board 开发板 | [https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj](https://developer.tuya.com/cn/docs/iot-device-dev/T5-E1-IPEX-development-board?id=Ke9xehig1cabj) | 重启(按 RST 按钮) 3 次重置 |
| TUYA T5AI_EVB 开发板 | [https://oshwhub.com/flyingcys/t5ai_evb](https://oshwhub.com/flyingcys/t5ai_evb) | 重启(按 RST 按钮) 3 次重置 |
| ATK T5AI Mini Board |  | 重启(按 RST 按钮) 3 次重置 |
| DNESP32S3_BOX | [https://www.alientek.com/Product_Details/118.html](https://www.alientek.com/Product_Details/118.html) | 重启(按 RST 按钮) 3 次重置 |
| ESP32S3_BREAD_COMPACT_WIFI |  | 重启(按 RST 按钮) 3 次重置 |
| WAVESHARE_ESP32S3_Touch_AMOLED_1.8 | [https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm) | 重启(按 RST 按钮) 3 次重置 |
| XINGZHI_ESP32S3_CUDE_0.96_OLED_WIFI | [https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html](https://www.nologo.tech/product/esp32/esp32s3/esp32s3ai/esp32s3ai.html) | 重启(按 RST 按钮) 3 次重置 |

## 工作原理
1. 外部设备（如PC）通过UART将对话文本发送给嵌入式设备
2. 嵌入式设备接收文本并上报给涂鸦云AI服务
3. 涂鸦云AI服务处理请求并返回响应
4. 嵌入式设备接收AI响应并通过UART发送回外部设备

## UART通信协议
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：无
- 数据格式：UTF-8编码文本

## 编译
1. 运行 `tos config_choice` 命令，选择当前运行的开发板。
2. 如需修改配置，请先运行 `tos menuconfig` 命令修改配置。
3. 运行 `tos build` 命令，编译工程。

## 配置说明

### 默认配置
- 使用UART0与外部设备通信
- 对话模式为随意对话模式
- 未开启AEC

### 通用配置

- **选择对话模式**

  - 随意对话模式

    | 宏 | 类型 | 说明 |
    | --- | --- | --- |
    | ENABLE_CHAT_MODE_ASR_WAKEUP_FREE | 布尔 | 设备唤醒后可进行随意对话。如果30秒没有检测到声音，则需要再次唤醒。 |

- **UART配置**

  | 宏 | 类型 | 说明 |
  | --- | --- | --- |
  | USER_TEXT_UART | 数值 | 用于通信的UART端口，默认为TUYA_UART_NUM_0 |

### 连接示意图
```
+-----------------+      UART     +----------------------+      网络      +--------------+
|   外部设备      | <------------> | your_serial_chat_bot | <-------------> | 涂鸦云AI服务 |
+-----------------+   (文本数据)   +----------------------+  (Wi-Fi/以太网)  +--------------+
```

## 使用说明
1. 通过UART将开发板与PC连接
2. 打开串口终端（如PuTTY、SecureCRT），设置115200 波特率
3. 向设备发送文本消息
4. 接收设备返回的AI响应
