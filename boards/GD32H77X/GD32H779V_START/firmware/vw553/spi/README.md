# GD32VW553 SPI gateway 候选固件

这是供 GD32H779V-START 使用的 SPI transport 候选固件。镜像已交叉编译，协议与
transport 回归已通过；双板基础 SPI AT 与 `AT+GMR` smoke 已通过。当前状态为
**hardware-smoke-validated**，验收范围仅限基础 AT/GMR。

## 镜像信息

| Item | Value |
|---|---|
| File | `vw553_at_gateway_spi_v0.1.0.bin` |
| Chip | GD32VW553HMQ6 |
| Board | LCKFB GD32VW553 |
| Size | 1,046,932 bytes |
| SHA256 | `5a641fe3dc23004fc32b09bcf5aea5c5024af211d32f93cc9b4088bf1229e572` |
| Build transport | SPI |
| Test status | Basic AT/GMR hardware smoke passed |
| Flash start | `0x08000000` |

这是包含 MBL 和应用的完整镜像，烧录起点为 `0x08000000`。烧录前执行
`sha256sum -c SHA256SUMS` 校验。SPI 镜像与 `../uart/` 下的稳定 UART 镜像分开，
不可互换。

## 构建

From `platform/GD32/gd32_os`:

```sh
VW553_GATEWAY_TRANSPORT=SPI ./cmake_build.sh vw553_at_gateway clean
VW553_GATEWAY_TRANSPORT=SPI ./cmake_build.sh vw553_at_gateway
```

默认传输仍是 UART；可显式设置 `VW553_GATEWAY_TRANSPORT=UART`，也可省略该变量。
SPI 镜像使用本工程自定义 framed SPI v1 字节流传输，保留现有 UART AT 命令、
TCP/IP、BLE、`+IPD` 和二进制数据语义。它没有启用原厂 `CONFIG_ATCMD_SPI`，与
原厂 AT over SPI 示例的填充及长度前缀协议不兼容。

## SPI 接线与当前板级限制

接线方案见 [GD32H779V-START 与 GD32VW553 接线说明](../../../../../../platform/GD32H77X/docs/gd32h779v_start_vw553_wiring.md)：

| Signal | H779V-START | VW553 |
|---|---|---|
| SCK | PE2, AF5 | PA2 |
| MISO | PE5, AF5 | PA1 |
| MOSI | PE6, AF5 | PA0 |
| REQUEST / software NSS | PE4 GPIO | PA3 input |
| READY | PE3 GPIO/EXTI | PA4 output |
| Ground | GND | GND |

仓库中的 LCKFB_GD32VW553 引脚图标注：PA0 同时接用户 KEY，板载 LED 位于 PC13；PA1 为 SPI MISO。SPI 使用期间不要按 KEY。

### 已完成的硬件 smoke

H779 USART5 日志中，`AT` 返回 `OK`，`AT+GMR` 返回版本
`v1.0.3g-04f35186ae4a8741`（build `2026/11/12-11:15:50`）。探针结果为
`SPI_AT_PROBE_RESULT PASS at=1 version=1`；SPI 诊断统计 `timeouts=0`、
`invalid=0`、`valid=69`。

首次冷启动尝试未通过，原因尚未查明；观察到 VW553 正常启动后再复位 H779，探针
通过。因此不能据此声称冷启动稳定。二进制 payload/`+IPD`、Socket 与 BLE 业务、
吞吐、长时间稳定性，以及冷启动稳定性均未验证；候选固件不代表这些业务已完成
实机验收。

每帧固定 256 字节，包含 16 字节头和最多 240 字节 payload。SPI 使用 Mode 0、
MSB-first、8-bit；header 和 payload 分别用 CRC-16/CCITT-FALSE 校验。两个方向各自
使用 stop-and-wait 序号与累计 ACK。VW553 PA4 READY 在收发 DMA 都准备好后响应每次
PE4 REQUEST。完整字段与恢复行为见 [SPI transport 协议说明](../../../../../../platform/GD32/gd32_os/MSDK/vw553_at_gateway/docs/spi-protocol.md)。

SPI 模式下 `AT+CIPMODE=1` 和 BLE passthrough 命令会返回错误，因为它们依赖 UART
idle 检测。普通 AT 命令模式和定长二进制发送保留支持。烧录步骤见 [VW553 ISP
烧录说明](../../../../../../platform/GD32H77X/docs/vw553-isp-flashing.md)。
