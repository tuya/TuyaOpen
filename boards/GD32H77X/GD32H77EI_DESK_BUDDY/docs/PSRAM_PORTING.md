# APS6408L-3OBM-BA 八线 PSRAM 适配

## 器件与拓扑

- 板型：`GD32H77EI_DESK_BUDDY`
- PSRAM：`APS6408L-3OBM-BA`，64 Mbit / 8 MiB
- 控制器：OSPI0 + OSPIM Port0
- 接口：八线 DDR，带 DQS
- 初始时钟：75 MHz
- 硬件复位：PB1
- 访问模式：memory mapped

## 引脚与 AF

| 信号 | 引脚 | AF |
| --- | --- | --- |
| CS | PG6 | AF10 |
| SCK | PF10 | AF9 |
| DQS | PC5 | AF10 |
| IO0 | PF8 | AF10 |
| IO1 | PF9 | AF10 |
| IO2 | PF7 | AF10 |
| IO3 | PD3 | AF8 |
| IO4 | PD4 | AF10 |
| IO5 | PD5 | AF10 |
| IO6 | PD6 | AF10 |
| IO7 | PD7 | AF10 |

## 驱动适配

新增 platform profile：

- `gd32h77x_psram_aps6408l.c`
- `gd32h77x_psram_aps6408l.h`
- profile 名称：`APS6408L-3OBM`

驱动使用参考包 `docs/psram/bsp_driver-main.zip` 中的 `bsp_aps6408l.c` 协议：

- 16-bit DTR 指令，八线地址/数据
- 读寄存器：`0x4040`
- 写寄存器：`0xC0C0`
- Linear read：`0x2020`
- Linear write：`0xA0A0`
- Vendor ID：`0x0D93`
- Device ID：`0x93E0`
- MR0：固定延迟 LC5，`0x28`
- MR8：打开 linear burst 跨 row 位
- 75 MHz：read dummy 10，write dummy 4
- TCK tap 6，RCK tap 16

平台 OSPI 抽象同步扩展了：

- IO4~IO7、DQS 引脚和 OSPIM 高位 IO 源
- 16-bit DTR 指令、32-bit DTR 地址、八线 DTR 数据
- DQS gated read
- 八线 mapped read/write 配置
- PSRAM 硬件复位 GPIO

旧 `APS6404L-3SQR` 四线 profile 保留，原 779 Desk Buddy 路径不变。

## 板级配置

```text
CONFIG_GD32H77X_PSRAM_PROFILE="APS6408L-3OBM"
CONFIG_GD32H77X_PSRAM_ACCESS_MAPPED=y
CONFIG_GD32H77X_PSRAM_CS_PIN="PG6"
CONFIG_GD32H77X_PSRAM_SCK_PIN="PF10"
CONFIG_GD32H77X_PSRAM_DQS_PIN="PC5"
CONFIG_GD32H77X_PSRAM_IO0_PIN="PF8"
CONFIG_GD32H77X_PSRAM_IO1_PIN="PF9"
CONFIG_GD32H77X_PSRAM_IO2_PIN="PF7"
CONFIG_GD32H77X_PSRAM_IO3_PIN="PD3"
CONFIG_GD32H77X_PSRAM_IO4_PIN="PD4"
CONFIG_GD32H77X_PSRAM_IO5_PIN="PD5"
CONFIG_GD32H77X_PSRAM_IO6_PIN="PD6"
CONFIG_GD32H77X_PSRAM_IO7_PIN="PD7"
CONFIG_GD32H77X_PSRAM_RESET_PIN="PB1"
CONFIG_GD32H77X_PSRAM_CLOCK_HZ=75000000
CONFIG_GD32H77X_PSRAM_SIZE_MB=8
```

## 当前状态

- 已完成平台 profile、板级引脚、mapped heap 和编译接入。
- `switch_demo` 构建通过。
- 尚未在 `GD32H77EI_DESK_BUDDY` 实板验证 ID、DQS 时序、全容量和 mapped 读写。
- 实板验证时优先检查 PB1 复位电平、PC5 DQS 波形、PG6 CS 和 75 MHz 时钟质量。
