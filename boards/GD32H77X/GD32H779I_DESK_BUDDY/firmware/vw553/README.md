# Desk Buddy TY1 / GD32VW553 固件

当前文件：`vw553_desk_buddy_spi_v2.bin`，为 Desk Buddy 提供无线联网能力的
TY1 / GD32VW553 完整固件，包含引导程序和应用。

- 当前构建版本：`v1.0.3g-04f35186ae4a8741`
- 构建时间：`2026-11-24 22:58:18`
- 镜像状态：已构建并核对 SHA-256；本轮实板回归待执行。

H779 通过硬件 SPI 上的 AT 接口控制无线模块，实现 Wi-Fi 扫描、连接、AP 配网、
DNS 解析及 TCP/UDP 数据通信；同时提供 BLE 配网支持。
联网业务由 H779 发起，TY1 / GD32VW553 负责无线连接和网络数据收发。

## 镜像与接线

- 类型：含引导程序的完整镜像，烧录基址 `0x08000000`。
- 长度：1,051,028 字节；SHA-256 见同目录 `SHA256SUMS` 和 `manifest.json`。
- 传输：硬件 SPI，Mode 0，256 字节 CS_ONLY v2 帧，CS 低有效，无 READY 引脚。
- 日志：SDK UART2，PA6 TX / PA7 RX，115200 8N1。
- 镜像已开启诊断功能。
- BLE 数据事件 `+BLEDATA` 以 CRLF 结束，不追加命令结束标记 `OK`，避免干扰并行的 `CIPSEND`。

| 信号 | H779 SPI1 | TY1 / VW553 |
| --- | --- | --- |
| SCK | PC3 | PA2 |
| MISO | PC2 | PA5 |
| MOSI | PC1 | PA4 |
| CS | PC0 | PA3 |
| 复位 | PH10 | PU |

H779 采用 DMA/IRQ 后端，配置在
[板级 gd32h77x.config](../../platform_config/gd32h77x.config)。
复位是否共用取决于 R66 实装；当前板级启用 `ENABLE_GD32H77X_AT_SHARED_RESET`，
避免通过 PH10 拉低共用 NRST。

CH9114 第二路（`if02`）为 H779 控制台。TY1/TY553 的日志口和下载口已经分离：

- 2026-11-23 实测 TY1 下载口为 `/dev/ttyACM3`，完整镜像烧录成功；
- TY1 日志口是独立接口，不能根据旧文档的 `if06` 或 `ttyACM3` 推断；
- H779 日志和 TY1 日志都应按实时输出内容识别，不能只依赖 `ttyACM*` 编号。

## 构建

从仓库根目录初始化环境后执行：

```bash
cd platform/GD32/gd32_os
VW553_GATEWAY_TRANSPORT=SPI \
VW553_GATEWAY_SPI_CS_ONLY=ON \
VW553_GATEWAY_SPI_DESK_BUDDY_PINS=ON \
VW553_GATEWAY_DIAGNOSTICS=ON \
VW553_GATEWAY_LOG_UART=UART2 \
./cmake_build.sh vw553_at_gateway
```

完整镜像输出为 `scripts/images/image-all.bin`。更新本目录时应同步更新
`manifest.json` 的长度、SHA-256、构建选项和验证结果，以及 `SHA256SUMS`。
`DIAGNOSTICS` 和 `LOG_UART` 必须显式传入，默认值与本镜像不同。

## 烧录

先在本目录执行 `sha256sum -c SHA256SUMS`。通过 TY1 下载口烧录，
BOOT 为高时复位进入 ROM ISP，使用完整镜像从 `0x08000000` 写入并校验。
Linux 可使用仓库自带下载接口；以下在仓库根目录运行。当前台架已验证的下载口为
`/dev/ttyACM3`，换机器或重新枚举后必须重新确认：

```bash
.venv/bin/python - <<'PY'
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path('platform/GD32/tools').resolve()))
from gd32_isp_flash import flash_firmware

logging.basicConfig(level=logging.INFO)
result = flash_firmware(
    device='GD32VW553',
    port='/dev/ttyACM3',  # 本台架已验证的 TY1 下载口
    baud=1000000,
    start='0x08000000',
    binfile='boards/GD32H77X/GD32H779I_DESK_BUDDY/firmware/vw553/vw553_desk_buddy_spi_v2.bin',
    logger=logging.getLogger('vw553'),
)
print(result)
raise SystemExit(0 if result.get('success') else 1)
PY
```

写入后释放 BOOT，检查应用启动日志。下载器会分别报告校验和启动结果；
应分别确认芯片端哈希校验和应用启动；正常运行时 BOOT 保持低电平。
不要将 `MSDK.bin` 应用单体当作完整镜像写到上述基址。
H779 的 QIO 镜像通过 GD32H77X 平台的 `tos.py flash` 流程烧录。

设备授权写入 H779 KV，在 H779 控制台直接执行 `auth <UUID> <KEY> 0`，
用 `auth-read 0` 核对，执行 `sys_reboot` 后再次确认。授权不写入 553 固件。

## 当前构建状态（2026-11-24）

- 完整镜像已由 GD32VW55x V1.0.3g 工程重新构建。
- 固件版本：`v1.0.3g-04f35186ae4a8741`。
- 已更新板级镜像、长度、SHA-256 和 manifest。
- 本轮尚未执行实板烧录、联网、BLE、CIPSEND 并发和长稳回归，因此不沿用旧镜像的验证结论。

## 历史验证记录（2026-11-20）

- 553 完整镜像写入成功，芯片端 SHA-256 校验通过；H779 沿用已有聊天固件与 KV。
- H779 硬件 SPI/DMA 与模块握手成功，自动连接已保存的 Wi-Fi。
- 涂鸦云 MQTT TLS 连接成功，收到 `MQTT_CONNECTED` 及三个 SUBACK。
- 设备元数据 HTTPS 请求完成；AI 服务 TLS、会话建立和串口文本往返成功。
- 本轮助手采集期间未出现 `payload result mismatch` 或 AT 通道 poisoned。
- 用户随后实测确认：本次蓝牙配网没有报错。该次手机复测未由助手采集串口日志。
- 主机回归使用 553 的实际 BLE 输出函数、响应宏与 H779 实际解析器，覆盖二进制
  负载 0/52/129/512 字节、任意字节分片、提示符前后交错，ASan/UBSan 检查通过。
- 尚未完成长期稳定性和反复配网压力验收；AI 回复结束时另有一条空文本日志，
  与本次 AT 响应边界修复分开记录。

本机记录位于 `.cache/gino-validation/desk-ble-urc-fix-20261120/`。
本目录不保存设备 KEY 或运行时 KV 备份。
