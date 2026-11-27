# GD32H779I Desk Buddy

板型 `GD32H779I_DESK_BUDDY`，主控 `GD32H779IW`，无线模块 TY1 / GD32VW553。
当前 TY1 链路采用硬件 SPI1 + DMA/IRQ，CS_ONLY v2 协议。

## 目录

| 路径 | 用途 |
| --- | --- |
| `Kconfig`、`CMakeLists.txt` | 板型选择及板级构建入口 |
| `board_com_api.c/.h` | 板级外设注册 |
| `platform_config/gd32h77x.config` | 引脚、电源、下载后端等平台配置 |
| `docs/` | 引脚核对、PSRAM 适配及分配说明 |
| `doc/` | 本地原始资料：原理图、GPIO 表、器件手册和参考工程，保持现有忽略规则 |
| `firmware/vw553/` | 当前 TY1 配套固件、README、manifest 和 SHA256SUMS |
| `firmware/ci3422/` | CI3422 音频桥固件、README、manifest 和 SHA256SUMS |

## 文档入口

- [引脚核对与联调记录](docs/PIN_BRINGUP.md)
- [SWD 连接与调试](docs/SWD_DEBUG.md)
- [PSRAM 适配记录](docs/PSRAM_PORTING.md)
- [PSRAM 内存分配](docs/PSRAM_ALLOCATION.md)
- [当前 TY1 固件与烧录说明](firmware/vw553/README.md)
- [当前 CI3422 音频桥固件与烧录说明](firmware/ci3422/README.md)

原理图及 GPIO 分配表位于本地 `doc/`：
`H779_TY1  SCH 20261116.pdf`、`GD32H779IWK7 GPIO分配表 20261116.xlsx`。

## switch_demo 验证入口

在仓库根目录初始化环境后执行：

```bash
cd apps/tuya_cloud/switch_demo
tos.py config choice -c GD32H779I_DESK_BUDDY.config
tos.py build
```

H779 当前构建产物位于应用的 `dist/`，使用 `tos.py flash` 烧录完整 QIO 包。
TY1 使用板级当前固件目录中的完整镜像，具体接线、端口和烧录方法见对应 README。

2026-11-20 已验证 H779 与 TY1 两端烧录校验、硬件 SPI 启动握手、AP/BLE
配网启动，以及 H779 授权在重启后的读回一致性；用户确认本次链路已跑通。
详细采集范围及已知问题见当前 TY1 固件 README。

板级目录不保存设备 KEY 或运行时 KV 备份。更新镜像时同步更新同目录元数据。
