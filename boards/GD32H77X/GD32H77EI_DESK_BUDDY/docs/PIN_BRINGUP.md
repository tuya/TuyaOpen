# GD32H77EI Desk Buddy 引脚核对

依据：

- `H77E_TY1  SCH 20261123 PSRAM.pdf`
- `GD32H77E GPIO分配表 20261123.xlsx`
- 板主确认：H779 PA15 已与 CI3422 原复位脚断开，飞线至 CI3422 PC1
  `CI_WAKEUP`，与当前 `GD32H779I_DESK_BUDDY` 一致。

## 主要外设

| 功能 | 引脚 | 当前适配 |
| --- | --- | --- |
| 主控 | `GD32H77EIW`，BGA176 | `CHIP_CHOICE="GD32H77EIW"`，构建宏 `GD32H77EXW` |
| 启动布局 | 完整向量表位于 `0x08000000` | 与 H779 的 `0x08010000` 应用向量布局不同；VTOR 指向 `0x08000000` |
| 调试串口 | USART0：PF4 TX、PF5 RX，AF4 | 已配置 |
| TY1 | SPI1：PC0 CS、PC1 MOSI、PC2 MISO、PC3 SCK；PH10 reset | 已配置，硬件 SPI/DMA |
| 音频口 | UART3：PD0 RX、PD1 TX | 已配置 |
| CI3422 控制 | H779 PA15 ↔ CI3422 PC1 `CI_WAKEUP` | 按当前 779 Desk Buddy 的 SPK flow 语义保留；原理图/IO 表中的 `CI_RST` 标记作废 |
| LCD | SPI3/QSPI：PE12 SCK、PC4 CS、PE14 D0、PE13 D1、PE9 D2、PE10 D3 | `TUYA_QSPI_NUM_0` |
| LCD 控制 | PF14 RST、PE15 TE、PH6 BL | 已配置 |
| 触摸 | PH7 SCL、PH8 SDA（TKL `TUYA_I2C_NUM_2`）、PH3 RST、PH4 INT | 已配置 |
| 前摄 CN3 | I2C：PH7/PH8；PG2 reset；PG5 PWDN | 已配置 |
| 后摄 CN4 | I2C3：PD12 SCL、PD13 SDA；PA10 reset；PG5 PWDN | 已配置 |
| DVP | D0 PC6、D1 PC7、D2 PB13、D3 PH12、D4 PH14、D5 PB6、D6 PB8、D7 PB9 | 已配置 |
| DVP 同步/时钟 | PCLK PE3、MCLK PA8、VSYNC PB7、HSYNC PH15 | 已配置；PCLK/D5 相对 779 有变更 |
| IMU | I2C PH7/PH8，PF12 INT | 已配置 |
| 编码器 | PH9 A、PB12 B、PA0 button | 已配置 |
| RGB | PE7/PB14 可选 W2812B | 板级尚未注册 |
| USB | PA11 DM、PA12 DP | 芯片默认 |
| 电源保持 | PC13 PWR_CTRL，高有效 | 已配置 |
| 电池检测 | PF6 ADC2_IN8 | 尚未注册 |
| 充电状态 | PE4 CHG_STAT | 尚未注册 |
| TF 卡 | 表中混用 SDIO0/SDIO1 引脚 | 尚未注册，需实板确认 |

## 外部存储

| 器件 | 引脚 | 状态 |
| --- | --- | --- |
| APS6408L-3OBM-BA PSRAM | CS PG6、SCK PF10、DQS PC5、IO0 PF8、IO1 PF9、IO2 PF7、IO3 PD3、IO4 PD4、IO5 PD5、IO6 PD6、IO7 PD7、RST PB1 | 八线 OSPI0 / OSPIM Port0 已接入 |
| MX25L512EMI-10G NOR | CS PE11、SCK PA5、MOSI PA7、MISO PA6 | 当前 TuyaOpen 板级未注册 |

## 实板验证顺序

1. 电源、复位、25 MHz 晶振、BOOT 和下载口。
2. USART0 日志、SWD/DFU。
3. PSRAM ID、MR0/MR8、间接读写、八线 mapped heap。
4. TY1 SPI1 启动、联网和 BLE 配网。
5. LCD、触摸、前后摄像头（PCLK PE3、D5 PB6）。
6. UART3/CI3422、PA15 ↔ PC1 `CI_WAKEUP` 链路。
7. IMU、编码器、TF 卡、电池和充电检测。
