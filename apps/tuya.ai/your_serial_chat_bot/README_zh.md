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
1. 运行 `tos.py config choice` 命令，选择当前运行的开发板。
2. 如需修改配置，请先运行 `tos.py config menu` 命令修改配置。
3. 运行 `tos.py build` 命令，编译工程。

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

## GD32H779I Desk Buddy 新板（2026-11-16 原理图）

在应用目录执行 `tos.py config choice -c GD32H779I_DESK_BUDDY.config`，再执行
`tos.py build`。硬件路由保存在板级 `platform_config/gd32h77x.config`。
该配置保留产品 PID，关闭显示、双摄、触摸、编码器和运动传感器，先验证串口 AI。

日志与 AI 聊天共用 USART0：PF4 TX、PF5 RX，对应 CH9114 通道 1，115200 8N1。
发送 UTF-8 文本，以 CR 或 LF 结束；AI 回复与日志会在同一串口出现。
TY1 使用硬件 SPI1：PC3 SCK、PC2 MISO、PC1 MOSI、PC0 CS，PH10 控制 TY1 PU。
采用 DMA/IRQ 传输，运行 256 字节 CS_ONLY v2 AT 协议。
固件启动早期拉高 PC13，保持板上电源。

配套 VW553 固件使用 CS_ONLY v2，无 READY 引脚；PA2 SCK、PA5 MISO、PA4 MOSI、
PA3 CS。H779 控制台使用 CH9114 `if02`，VW553 ROM ISP 和控制线使用 `if06`。
`ttyACM*` 每次插拔都可能变化，换板或换集线器后 by-id 中的 USB 序列号也会变化。
应动态枚举 `/dev/serial/by-id/usb-wch.cn_USB_Quad_Serial_*-if02` 和 `*-if06`；
`if02` / `if06` 只表示 CH9114 的接口角色，不是板卡身份。多板同时接入时必须按当前
USB 序列号或物理端口选择目标设备，并用 ISP 握手/芯片 ID 复核，不能写死 `ttyACM*`
或某个序列号。
可先烧录 `switch_demo` 的 Desk Buddy 配置，在 H77 上通过现有 `tal_cli` 命令
`auth <UUID> <KEY> 0` 将授权写入 KV，再烧录本工程。烧录须保留 KV 分区。
聊天工程不启动 CLI 接收任务，避免两者争用串口；无需将密钥写进源代码。
Desk Buddy 共用 NRST 时启用 `ENABLE_GD32H77X_AT_SHARED_RESET`，禁止模块独立硬复位。
构建时需指定 `VW553_GATEWAY_LOG_UART=UART2`、`VW553_GATEWAY_TRANSPORT=SPI`、
`VW553_GATEWAY_SPI_CS_ONLY=ON`、`VW553_GATEWAY_SPI_DESK_BUDDY_PINS=ON`。
DSP 实际连接 H779 UART3 PD1 TX / PD0 RX、PA15 复位；本聊天配置暂不启用 DSP。
TY1 PB15/PA8 仅作备用连接。新板固件编译通过后仍需实板验证供电、串口和联网。

## GD32H779V START + VW553（SPI）

在应用目录中执行：

```bash
tos.py config choice -c GD32H779V_START.config
tos.py build
tos.py flash -p <GD-Link探针ID>
```

板级 `boards/GD32H77X/GD32H779V_START/platform_config/gd32h77x.config` 使用已验证的 SPI1
CS_ONLY 协议：PB13 SCK、PB14 MISO、PB15 MOSI、PC0 CS、PC5 模块复位。
VW553 使用配套的 CS_ONLY 固件，SPI 连线保持此前联网测试的接法。

聊天口为 UART0，日志口独立保留 USART5；接口均为 3.3V TTL、115200 8N1。

| 用途 | START 引脚与排针 | USB 转串口连接 |
| --- | --- | --- |
| 聊天发送 | PB6，JP4-33 | 接 RX |
| 聊天接收 | PB7，JP5-17 | 接 TX |
| 共地 | GND，JP4-5 | 接 GND |
| 日志发送 | PC6，JP5-2 | 接日志适配器 RX |
| 日志接收 | PC7，JP5-1 | 接日志适配器 TX |

START 板 PA9 的 USART0 TX 功能实测不可用；`board_register_hardware()` 将
UART0 重映射到 PB6/PB7。2026-11-16 已通过 CH340 完成 PB6/PB7 外部回环验证。

聊天端发送 UTF-8 文本，以 CR 或 LF 结束；CRLF 视为一条消息。
每条最多 512 字节，超过限制会丢弃整行并提示错误。
AI 会话未就绪时提示 `AI session is not ready`，云端回复从同一聊天口输出。
可用 `tos.py config set USER_CHAT_UART_NUMBER=<编号>` 选择其他 TKL UART；
切换前应检查其路由与日志、SPI 及其他外设是否冲突。

该配置关闭音频、显示和语音交互模式，只做串口文本对话。
AI 服务需要有效设备授权、完成配网绑定，以及对应产品的 AI 基座/智能体服务。
设备联网或普通 MQTT 上线本身不代表 AI 对话已验收。

START 配置将 `AI_CLIENT_STACK_SIZE` 设为 16384 字节，供 AI 客户端执行
TLS 握手；原来的 4096 字节在该平台实测触发任务栈溢出检查。
TCP_NODELAY 与 keepalive 由双端 `AT+CIPTCPOPT` 实现，连接前缓存参数，
连接后写入 VW553 的 lwIP socket 并读回确认；已有连接也可修改或禁用
keepalive。参数设置失败会报错并中止连接，不再跳过“不支持”错误。
需配套 [Desk Buddy 当前 VW553 固件](../../../boards/GD32H77X/GD32H779I_DESK_BUDDY/firmware/vw553/README.md)。

2026-11-16 START 实板验证：AI 服务 TLS 握手、认证、进入 RUNNING 状态和
会话创建均成功。UART0 文本经 CH340 发送后，本地可见 `user:`、情绪和
`ai reply:`，云端 AI 回复已完整回传。AT transport 在 `app_entry()` 创建的
TuyaOpen 应用线程之前初始化；复位路径按“复位 → recover transport →
等待 READY → drain → AT 探测”执行，消除了首轮 READY/AT 事务竞争。
本地 AI monitor 的 5055 端口与其他 TCP 监听可独立创建、关闭；同一端口
重复监听会报错。2026-11-16 双端新固件实测：TCP_NODELAY=1，keepalive
开启且参数读回为 100/5/1；monitor 5055 监听成功，AI TLS、RUNNING 和
会话创建通过。未启用跳过 keepalive 设置错误的兼容逻辑。
