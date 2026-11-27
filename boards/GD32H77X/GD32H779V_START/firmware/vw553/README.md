# GD32VW553 wireless firmware

本目录保存 GD32H779V_START + 嘉立创 GD32VW553 的配套预编译固件。固件按传输方式分开，
使 H77 平台在没有 `platform/GD32` 源码仓库时，仍可使用已验证版本完成 VW553
烧录。

```text
firmware/vw553/
├── uart/
│   ├── vw553_at_gateway_uart_v0.5.2.bin
│   ├── manifest.json
│   └── SHA256SUMS
└── spi/
```

Desk Buddy 新板请使用 [Desk Buddy TY1 固件](../../../GD32H779I_DESK_BUDDY/firmware/vw553/README.md)。

## 当前稳定版本

| 项目 | 值 |
| --- | --- |
| 应用 | `vw553_at_gateway` |
| 版本 | `0.5.2` |
| 传输方式 | UART AT gateway |
| 芯片 | `GD32VW553HMQ6` |
| 开发板 | `LCKFB_GD32VW553` |
| 镜像 | `uart/vw553_at_gateway_uart_v0.5.2.bin` |
| 大小 | `1044372` bytes |
| SHA256 | `4860ad84a94b444910fb1e1429f0f630bbaa80ebfbbb36de87fb9931ed8a8b7e` |

该镜像用于 H779 主控的 UART AT gateway，包含 MBL 和应用，烧录起点为
`0x08000000`。UART 和 SPI 镜像必须按目录分开，不能互相替换。

本固件只固定 VW553 侧的 UART 外设、引脚、波特率和功能配置。H77 侧使用哪个
UART 或 GPIO 由其平台配置决定，不由 VW553 镜像控制，也不在本包中描述。

## VW553 引脚

### 应用态

| 用途 | 外设 | VW553 引脚 | 方向 | 参数 | 说明 |
| --- | --- | --- | --- | --- | --- |
| AT 数据链路 | UART2 TX | `PA6`，AF10 | VW553 TX -> 对端 RX | 115200 8N1 | 只传输 AT 命令、响应和二进制帧 |
| AT 数据链路 | UART2 RX | `PA7`，AF8 | 对端 TX -> VW553 RX | 115200 8N1 | 不得混入日志文本 |
| 调试日志 | UART1 TX | `PB15`，AF7 | VW553 TX -> CH340 RX | 115200 8N1 | 板载 Type-C/CH340 日志口 |
| 调试日志 | UART1 RX | `PA8`，AF3 | CH340 TX -> VW553 RX | 115200 8N1 | 板载 Type-C/CH340 日志口 |
| 复位输入 | `RESET/NRST` | 板级复位信号 | 输入 | 低有效 | 可由板载 RST 或外部控制；硬件复位仅用于失联恢复 |
| 启动模式 | `BOOT0` | 板载 BOOT0 按键 | 输入 | 高电平进入 ROM ISP | 正常运行时保持低电平，不与其它启动脚连接 |
| 地 | `GND` | 板级 GND | — | — | 必须与 AT 对端共地 |

### ROM ISP

板载 Type-C 通过 CH340 连接 `PB15`/`PA8`。应用态这两个物理引脚由 UART1
输出日志；进入 ROM ISP 后由 BootROM 使用 USART0 路径复用同一组引脚：

```text
PC/CH340 RX  <- VW553 PB15
PC/CH340 TX  -> VW553 PA8
```

ROM ISP 初始握手为 57600 8E1；loader 接管后使用 115200 8N1，下载阶段当前工具
默认使用 2 Mbaud。进入 ISP 时按住 BOOT0、短按 RESET，并在握手完成前保持
BOOT0 有效。

H77 侧接线不在本固件包内描述，见
[GD32H779V-START 与 VW553 接线](../../../../../platform/GD32H77X/docs/gd32h779v_start_vw553_wiring.md)。
烧录 VW553 时必须断开外部对 UART2、RESET 和 BOOT0 的控制，避免下载口与外部
控制器同时驱动同一信号。

## 烧录

不需要修改 VW553 固件时，推荐使用 GigaDevice 官方独立烧录工具
**GD32 Mass Production Tool**，不需要拉取 `platform/GD32` 源码仓库。

已验证工具版本：

```text
GD32MassProductionTool v1.0.5g
SHA256 33559c67b294e7867867ba1c6063d2a64c8bc0cd0922df9bb0bbd879d0633faa
```

基本设置：

1. Chip 选择 `GD32VW553`。
2. 选择 VW553 板载 Type-C/CH340 对应的串口。
3. Download baudrate 选择 `2000000`。
4. Erase 选择 `Page Erase`，不要选择 `Chip Erase`。
5. Firmware 选择本目录的 `vw553_at_gateway_uart_v0.5.2.bin`。
6. 按住 BOOT0、短按 RESET，进入 ROM ISP 后开始下载。
7. 下载完成后释放 BOOT0 并再次复位。

烧录前必须确认所选串口对应 VW553 的 Type-C/CH340，而不是其它串口或调试
探针。

### 需要修改 VW553 固件时

H77 仓库中的 bin 是可复现的稳定交付件，不是 VW553 源码。需要修改时：

1. 拉取 `platform/GD32` 平台仓库。
2. 修改 `platform/GD32/gd32_os/MSDK/vw553_at_gateway`。
3. 通过该仓库的 canonical 构建命令重新编译 `image-all.bin`。
4. 在目标板完成 UART/SPI 对应功能验收。
5. 直接烧录开发者自己构建的 `image-all.bin`，不需要回写 H77 仓库。

本目录中的稳定 bin 由 H77 平台维护者按发布节奏更新；普通开发者修改固件时，
不需要也不应覆盖这里的稳定交付件。

### Linux 自动化备用方式

如果开发环境无法运行官方 Windows GUI/CMD，可使用 GD32 平台仓库中的 Python
ISP 工具；该方式需要 `platform/GD32` 仓库：

```bash
python3 platform/GD32/platform_flash_bridge.py \
  --device GD32VW553H \
  --port /dev/ttyUSB0 \
  --baud 2000000 \
  --binfile boards/GD32H77X/GD32H779V_START/firmware/vw553/uart/vw553_at_gateway_uart_v0.5.2.bin
```

## 更新规则

- UART 固件只放 `uart/`，SPI 固件只放 `spi/`。
- 版本写入文件名和 `manifest.json`，不再创建额外的版本目录。
- 更新固件时必须同时更新大小、SHA256 和 `SHA256SUMS`。
- 不得直接复制下一次构建的 `scripts/images/image-all.bin` 覆盖稳定文件；先在
  对应硬件组合完成验收，再作为新版本加入。
