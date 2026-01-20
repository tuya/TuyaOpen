# 4.26 英寸墨水屏时钟（Tuya IoT）

本示例在墨水屏上实现一个时钟界面，并集成 Tuya IoT 能力：

- Tuya APP 配网（BLE/AP）
- 时间同步（Tuya 云端时间戳；可选 NTP 兜底）
- DP 控制：
  - `time_mode`（`24h` / `12h`）
  - `night_mode`（映射为两档主题：黑底白字 / 白底黑字）
- 板载按键一键重新进入配网（清除 WiFi 信息 + 重置 IoT 状态机）

## 硬件说明

### 屏幕连线（示例）

默认引脚在 `lib/Config/DEV_Config.h` / `lib/Config/DEV_Config.c` 中定义，常见 SPI 墨水屏连线如下（具体以你的硬件为准）：

|信号|默认|
|---|---|
|SCLK|`TUYA_GPIO_NUM_2`|
|MOSI|`TUYA_GPIO_NUM_4`|
|CS|`TUYA_GPIO_NUM_3`|
|DC|`TUYA_GPIO_NUM_7`|
|RST|`TUYA_GPIO_NUM_8`|
|BUSY|`TUYA_GPIO_NUM_6`|
|PWR|`TUYA_GPIO_NUM_28`|

如需修改引脚，可在 `lib/Config/DEV_Config.h` 中调整（或通过编译宏覆盖）。

### 按键（重新配网）

默认按键引脚为 `TUYA_GPIO_NUM_12`（低电平有效）。在 `examples/tuya_config.h` 中可配置：

- `EPD_CLOCK_NETCFG_KEY_ENABLE`
- `EPD_CLOCK_NETCFG_KEY_PIN`
- `EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL`
- `EPD_CLOCK_NETCFG_KEY_LONGPRESS_MS`

按下按键后会执行：

1. 清除保存的 WiFi 信息（KV：`netinfo`）
2. 调用 `tuya_iot_reset()`，让设备重新进入配网流程

## Tuya 平台配置

编辑 `examples/tuya_config.h`：

- `TUYA_PRODUCT_ID`：Tuya IoT 平台创建产品得到的 PID
- `TUYA_OPENSDK_UUID` / `TUYA_OPENSDK_AUTHKEY`：仅在 OTP/KV 授权读取失败时作为兜底使用
- `EPD_CLOCK_TZ_SECONDS`：时区偏移秒数（默认 `UTC+8`）

说明：日志里出现 `OTP license read failed` 不一定是错误。如果你的模组没有预烧录授权信息，Demo 会使用 `examples/tuya_config.h` 里的 UUID/AUTHKEY 兜底。

## 编译 / 烧录 / 监控

在本目录下执行：

```powershell
python ..\..\..\tos.py build
python ..\..\..\tos.py flash -p COM3
python ..\..\..\tos.py monitor
```

提示：

- 有些开发板会有两个串口（一个用于下载，一个用于日志）。如果日志乱码，请在监控工具里换另一个 COM 口。
- 如果 Windows 报串口设备异常（例如错误 31 “设备没有发挥作用”），请拔插开发板并检查 USB 转串口驱动（设备管理器）。

## 配网使用

1. 上电运行。
2. 打开 Tuya Smart / Smart Life APP 添加设备（BLE/AP）。
3. 如果需要重新配网，按下按键即可清除并重新进入配网流程。

## DP：时间制 / 夜间模式（主题）

本示例在 `TUYA_EVENT_DP_RECEIVE_OBJ` 事件中解析 DP，并更新本地设置。

### DPID 约定

代码默认使用以下 DPID：

- `time_mode` DPID = `18`
- `night_mode` DPID = `28`

如果你在 Tuya 平台配置的 DPID 不同，请修改 `examples/EPD_clock.c` 中的 DPID 宏。

### `time_mode` 映射

- `24h` -> enum `0`
- `12h` -> enum `1`

### `night_mode` 映射（两档主题）

你的产品 Schema 可能把 `night_mode` 定义为 `mode_1`..`mode_5` 等多档。本 Demo 仅映射两档主题：

- `mode_1`（enum `0`）-> **黑底白字**
- 其它值 -> **白底黑字**

### 持久化

设置会写入 KV，重启后仍保留：

- `clock.time_mode`
- `clock.theme`

## 建议先看的文件

- `examples/EPD_clock.c`：Tuya IoT 初始化、DP 处理、按键处理、主循环
- `examples/clock_ui.c`：主题与 12/24 小时显示
- `examples/tuya_config.h`：产品/授权配置、按键配置
