# GD32H77EI Desk Buddy

板型 `GD32H77EI_DESK_BUDDY`，主控 `GD32H77EIW`，是
`GD32H779I_DESK_BUDDY` 的主控升级版本。

## 当前状态

- 已按 `H77E_TY1 SCH 20261123 PSRAM.pdf` 和
  `GD32H77E GPIO分配表 20261123.xlsx` 完成板级适配。
- DVP 的 PCLK 改为 PE3，D5 改为 PB6；板级启动时会覆盖平台默认 DVP 路由。
- PSRAM 使用 `APS6408L-3OBM-BA`，已接入八线 OSPI0 / OSPIM Port0、
  DQS、IO0~IO7、PB1 硬件复位和 `APS6408L-3OBM` profile。
- PA15 实物飞线连接 CI3422 PC1 `CI_WAKEUP`，不再按原理图/IO 表中的
  `CI_RST` 使用；板级保持当前 779 Desk Buddy 的 SPK flow 语义。
- `GD32H77EIW` 使用直接启动布局：完整中断向量表位于 `0x08000000`，
  `VTOR` 也指向 `0x08000000`；H779 仍保留原来的 `0x08010000` 应用向量布局。
- `GD32H77EIW` 使用板级 CNVM+ECNVM 覆盖布局，`switch_demo` 已构建通过，
  构建报告中的 `vector_address` 为 `0x08000000`。
- 尚未完成新板实板外设、音频链路和 PSRAM 时序验收。

## 目录

| 路径 | 用途 |
| --- | --- |
| `Kconfig` | 板型身份、芯片型号和板级能力 |
| `CMakeLists.txt` | 板级组件构建入口 |
| `board_com_api.c/.h` | 板级外设注册 |
| `platform_config/gd32h77x.config` | 引脚、电源、PSRAM 和下载配置 |
| `platform_config/partitions.csv` | CNVM+ECNVM 板级分区覆盖 |
| `docs/PIN_BRINGUP.md` | 新板引脚核对表 |
| `docs/PSRAM_PORTING.md` | APS6408L 八线 PSRAM 适配说明 |
| `docs/psram/bsp_driver-main.zip` | PSRAM 参考驱动包 |

## 调试期烧录

当前调试阶段临时使用 SWD：

```text
CONFIG_ENABLE_GD32H77X_FLASH_BACKEND_SWD=y
```

调试结束后、提交代码前必须恢复 DFU 默认后端：

```text
CONFIG_ENABLE_GD32H77X_FLASH_BACKEND_DFU=y
```

## 选择与构建

板级默认配置：

```bash
tos.py config choice -d -c GD32H77EI_DESK_BUDDY.config
```

Desk Buddy 应用配置：

```bash
cd apps/tuya.ai/your_desk_buddy
tos.py config choice -c GD32H77EI_DESK_BUDDY.config
tos.py build
```

注意：当前通过的是静态构建验证，实板仍需按 `PIN_BRINGUP.md` 的顺序验收。
