# T5AI Pinmux 对照表

`TUYA_GPIO_NUM_<n>` 对应 BK7258 芯片数据手册中的 `GPIO<n>`。本表列出 TKL 驱动的实际引脚映射。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_55` | GPIO0–GPIO55 |  |

## I2C

- T5AI 有 I2C0、I2C1 两个硬件控制器。`TUYA_I2C_NUM_0/1` 使用表中的硬件 I2C 引脚时走硬件 I2C。
- 初始化前通过 `tkl_io_pinmux_config` 配置为非硬件 I2C 引脚时走软件 I2C。
- `TUYA_I2C_NUM_2` 无硬件控制器，初始化前配置 SCL/SDA 后走软件 I2C。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SCL` | GPIO20 | 硬件 I2C，默认组。 |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SDA` | GPIO21 | 硬件 I2C，默认组。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO14 | 硬件 I2C，默认组。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO15 | 硬件 I2C，默认组。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO0 | 硬件 I2C，备选组 A。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO1 | 硬件 I2C，备选组 A。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO38 | 硬件 I2C，备选组 B。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO39 | 硬件 I2C，备选组 B。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO42 | 硬件 I2C，备选组 C。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO43 | 硬件 I2C，备选组 C。 |
| `TUYA_I2C_NUM_2` | 软件 I2C | `TUYA_IIC2_SCL` | 任意有效 GPIO | 初始化前配置。 |
| `TUYA_I2C_NUM_2` | 软件 I2C | `TUYA_IIC2_SDA` | 任意有效 GPIO | 初始化前配置。 |

## UART

`TUYA_UART_NUM_2` 的 TX/RX 须从同一套引脚中选择；默认映射排在前面。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_TX` | GPIO11 | 驱动固定引脚。 |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_RX` | GPIO10 | 驱动固定引脚。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | GPIO0 | 驱动固定引脚。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | GPIO1 | 驱动固定引脚。 |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_TX` | GPIO31 | 默认组 |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_RX` | GPIO30 | 默认组 |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_TX` | GPIO41 | 备选组 |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_RX` | GPIO40 | 备选组 |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_0` | PWM0 | `TUYA_PWM0` | GPIO18 |  |
| `TUYA_PWM_NUM_1` | PWM4 | `TUYA_PWM1` | GPIO24 |  |
| `TUYA_PWM_NUM_2` | PWM6 | `TUYA_PWM2` | GPIO32 |  |
| `TUYA_PWM_NUM_3` | PWM8 | `TUYA_PWM3` | GPIO34 |  |
| `TUYA_PWM_NUM_4` | PWM10 | `TUYA_PWM4` | GPIO36 |  |
| `TUYA_PWM_NUM_5` | PWM1 | `TUYA_PWM5` | GPIO19 |  |
| `TUYA_PWM_NUM_6` | PWM2 | `TUYA_PWM6` | GPIO8 |  |
| `TUYA_PWM_NUM_7` | PWM3 | `TUYA_PWM7` | GPIO9 |  |
| `TUYA_PWM_NUM_8` | PWM5 | `TUYA_PWM8` | GPIO25 |  |
| `TUYA_PWM_NUM_9` | PWM7 | `TUYA_PWM9` | GPIO33 |  |
| `TUYA_PWM_NUM_10` | PWM9 | `TUYA_PWM10` | GPIO35 |  |
| `TUYA_PWM_NUM_11` | PWM11 | `TUYA_PWM11` | GPIO37 |  |

## SPI

`TUYA_SPI_NUM_0` 的 MOSI、MISO、CLK、CS 须从同一套引脚中选择；默认映射排在前面。

`TUYA_SPI_NUM_2` 和 `TUYA_SPI_NUM_3` 分别复用 QSPI0、QSPI1，以 1-wire 模式提供标准 SPI 协议；均仅支持 Master。数据通过 QSPI IO0 单线半双工传输，不是独立的硬件 SPI2、SPI3。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MOSI` | GPIO16 | 默认组 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MISO` | GPIO17 | 默认组 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CLK` | GPIO14 | 默认组 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CS` | GPIO15 | 默认组 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MOSI` | GPIO35 | 备选组 A |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MISO` | GPIO36 | 备选组 A |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CLK` | GPIO33 | 备选组 A |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CS` | GPIO34 | 备选组 A |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MOSI` | GPIO46 | 备选组 B |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MISO` | GPIO47 | 备选组 B |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CLK` | GPIO44 | 备选组 B |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CS` | GPIO45 | 备选组 B |
| `TUYA_SPI_NUM_1` | SPI1 | `TUYA_SPI1_MOSI` | GPIO4 | 驱动固定引脚。 |
| `TUYA_SPI_NUM_1` | SPI1 | `TUYA_SPI1_MISO` | GPIO5 | 驱动固定引脚。 |
| `TUYA_SPI_NUM_1` | SPI1 | `TUYA_SPI1_CLK` | GPIO2 | 驱动固定引脚。 |
| `TUYA_SPI_NUM_1` | SPI1 | `TUYA_SPI1_CS` | GPIO3 | 驱动固定引脚。 |
| `TUYA_SPI_NUM_2` | QSPI0（1-wire 标准 SPI） | `TUYA_SPI2_MOSI` | GPIO24 | 对应 QSPI0 IO0；与 MISO 共用。 |
| `TUYA_SPI_NUM_2` | QSPI0（1-wire 标准 SPI） | `TUYA_SPI2_MISO` | GPIO24 | 对应 QSPI0 IO0；当前 `tkl_spi_recv()` 未接受 SPI2。 |
| `TUYA_SPI_NUM_2` | QSPI0（1-wire 标准 SPI） | `TUYA_SPI2_CLK` | GPIO22 | 对应 QSPI0 CLK。 |
| `TUYA_SPI_NUM_2` | QSPI0（1-wire 标准 SPI） | `TUYA_SPI2_CS` | GPIO23 | 对应 QSPI0 CS。 |
| `TUYA_SPI_NUM_3` | QSPI1（1-wire 标准 SPI） | / | GPIO4 | MOSI/MISO 对应 QSPI1 IO0；无通用 SPI3 Pinmux 标识。 |
| `TUYA_SPI_NUM_3` | QSPI1（1-wire 标准 SPI） | / | GPIO2 | CLK 对应 QSPI1 CLK；无通用 SPI3 Pinmux 标识。 |
| `TUYA_SPI_NUM_3` | QSPI1（1-wire 标准 SPI） | / | GPIO3 | CS 对应 QSPI1 CS；无通用 SPI3 Pinmux 标识。 |

## QSPI

`TUYA_QSPI_NUM_0/1` 分别对应 QSPI0/1；当通过 `TUYA_SPI_NUM_2/3` 使用时，两者工作在 1-wire 标准 SPI 模式，不能同时作为独立 QSPI 端口使用。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_QSPI_NUM_0` | QSPI0 CLK | / | GPIO22 |  |
| `TUYA_QSPI_NUM_0` | QSPI0 CS | / | GPIO23 |  |
| `TUYA_QSPI_NUM_0` | QSPI0 IO0 | / | GPIO24 |  |
| `TUYA_QSPI_NUM_0` | QSPI0 IO1 | / | GPIO25 |  |
| `TUYA_QSPI_NUM_0` | QSPI0 IO2 | / | GPIO26 |  |
| `TUYA_QSPI_NUM_0` | QSPI0 IO3 | / | GPIO27 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 CLK | / | GPIO2 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 CS | / | GPIO3 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 IO0 | / | GPIO4 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 IO1 | / | GPIO5 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 IO2 | / | GPIO6 |  |
| `TUYA_QSPI_NUM_1` | QSPI1 IO3 | / | GPIO7 |  |

## RGB

- RGB 为单实例接口，使用固定引脚。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | RGB R0 | / | GPIO50 |  |
| / | RGB R1 | / | GPIO49 |  |
| / | RGB R2 | / | GPIO48 |  |
| / | RGB R3 | / | GPIO23 |  |
| / | RGB R4 | / | GPIO22 |  |
| / | RGB R5 | / | GPIO21 |  |
| / | RGB R6 | / | GPIO20 |  |
| / | RGB R7 | / | GPIO19 |  |
| / | RGB G0 | / | GPIO52 |  |
| / | RGB G1 | / | GPIO51 |  |
| / | RGB G2 | / | GPIO42 |  |
| / | RGB G3 | / | GPIO41 |  |
| / | RGB G4 | / | GPIO40 |  |
| / | RGB G5 | / | GPIO26 |  |
| / | RGB G6 | / | GPIO25 |  |
| / | RGB G7 | / | GPIO24 |  |
| / | RGB B0 | / | GPIO55 |  |
| / | RGB B1 | / | GPIO54 |  |
| / | RGB B2 | / | GPIO53 |  |
| / | RGB B3 | / | GPIO47 |  |
| / | RGB B4 | / | GPIO46 |  |
| / | RGB B5 | / | GPIO45 |  |
| / | RGB B6 | / | GPIO44 |  |
| / | RGB B7 | / | GPIO43 |  |
| / | RGB CLK | / | GPIO14 |  |
| / | RGB DISP | / | GPIO15 |  |
| / | RGB DE | / | GPIO16 |  |
| / | RGB HSYNC | / | GPIO17 |  |
| / | RGB VSYNC | / | GPIO18 |  |

## 8080

- 8-bit 使用 D0–D7；9-bit、16-bit、18-bit 分别依次增加 D8、D9–D15、D16–D17。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | 8080 D0 | / | GPIO47 |  |
| / | 8080 D1 | / | GPIO46 |  |
| / | 8080 D2 | / | GPIO45 |  |
| / | 8080 D3 | / | GPIO44 |  |
| / | 8080 D4 | / | GPIO43 |  |
| / | 8080 D5 | / | GPIO42 |  |
| / | 8080 D6 | / | GPIO41 |  |
| / | 8080 D7 | / | GPIO40 |  |
| / | 8080 D8 | / | GPIO21 |  |
| / | 8080 D9 | / | GPIO20 |  |
| / | 8080 D10 | / | GPIO19 |  |
| / | 8080 D11 | / | GPIO18 |  |
| / | 8080 D12 | / | GPIO17 |  |
| / | 8080 D13 | / | GPIO16 |  |
| / | 8080 D14 | / | GPIO15 |  |
| / | 8080 D15 | / | GPIO14 |  |
| / | 8080 D16 | / | GPIO48 |  |
| / | 8080 D17 | / | GPIO49 |  |
| / | 8080 RDX | / | GPIO26 |  |
| / | 8080 WRX | / | GPIO25 |  |
| / | 8080 RSX | / | GPIO24 |  |
| / | 8080 RESET | / | GPIO23 |  |
| / | 8080 CSX | / | GPIO22 |  |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 1` | ADC1 | GPIO25 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 2` | ADC2 | GPIO24 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 3` | ADC3 | GPIO23 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 4` | ADC4 | GPIO28 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 5` | ADC5 | GPIO22 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 6` | ADC6 | GPIO21 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 10` | ADC10 | GPIO8 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 12` | ADC12 | GPIO0 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 13` | ADC13 | GPIO1 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 14` | ADC14 | GPIO12 |  |
| `TUYA_ADC_NUM_0` | SARADC | `ch_id = 15` | ADC15 | GPIO13 |  |

## I2S

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2S_NUM_0` | I2S1 | `TUYA_I2S0_SCK` | GPIO6 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S1 | `TUYA_I2S0_WS` | GPIO7 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S1 | `TUYA_I2S0_SDI_0` | GPIO8 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S1 | `TUYA_I2S0_SDO_0` | GPIO9 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S2 | `TUYA_I2S1_SCK` | GPIO40 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S2 | `TUYA_I2S1_WS` | GPIO41 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S2 | `TUYA_I2S1_SDI_0` | GPIO42 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S2 | `TUYA_I2S1_SDO_0` | GPIO43 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_2` | I2S3 | / | GPIO44 | SCK；驱动固定引脚。 |
| `TUYA_I2S_NUM_2` | I2S3 | / | GPIO45 | WS；驱动固定引脚。 |
| `TUYA_I2S_NUM_2` | I2S3 | / | GPIO46 | SDI；驱动固定引脚。 |
| `TUYA_I2S_NUM_2` | I2S3 | / | GPIO47 | SDO；驱动固定引脚。 |

## SDIO

- 支持 1-bit 和 4-bit：1-bit 使用 CLK、CMD、D0；4-bit 使用全部六个信号。
- `TUYA_SDIO_NUM_0` 的六个信号须从同一套引脚中选择；默认映射排在前面。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SDIO_NUM_0` | SDIO CLK | `TUYA_SDIO_CLK` | GPIO2 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO CMD | `TUYA_SDIO_CMD` | GPIO3 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO D0 | `TUYA_SDIO_DATA0` | GPIO4 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO D1 | `TUYA_SDIO_DATA1` | GPIO5 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO D2 | `TUYA_SDIO_DATA2` | GPIO10 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO D3 | `TUYA_SDIO_DATA3` | GPIO11 | 默认组 |
| `TUYA_SDIO_NUM_0` | SDIO CLK | `TUYA_SDIO_CLK` | GPIO14 | 备选组 |
| `TUYA_SDIO_NUM_0` | SDIO CMD | `TUYA_SDIO_CMD` | GPIO15 | 备选组 |
| `TUYA_SDIO_NUM_0` | SDIO D0 | `TUYA_SDIO_DATA0` | GPIO16 | 备选组 |
| `TUYA_SDIO_NUM_0` | SDIO D1 | `TUYA_SDIO_DATA1` | GPIO17 | 备选组 |
| `TUYA_SDIO_NUM_0` | SDIO D2 | `TUYA_SDIO_DATA2` | GPIO18 | 备选组 |
| `TUYA_SDIO_NUM_0` | SDIO D3 | `TUYA_SDIO_DATA3` | GPIO19 | 备选组 |

## DVP

- DVP 为单实例固定引脚接口；当前 TKL DVP 驱动未自动初始化这些引脚的 Pinmux，使用前需确认已完成相应配置。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | CIS MCLK | / | GPIO27 |  |
| / | CIS PCLK | / | GPIO29 |  |
| / | CIS HSYNC | / | GPIO30 |  |
| / | CIS VSYNC | / | GPIO31 |  |
| / | CIS PXD0 | / | GPIO32 |  |
| / | CIS PXD1 | / | GPIO33 |  |
| / | CIS PXD2 | / | GPIO34 |  |
| / | CIS PXD3 | / | GPIO35 |  |
| / | CIS PXD4 | / | GPIO36 |  |
| / | CIS PXD5 | / | GPIO37 |  |
| / | CIS PXD6 | / | GPIO38 |  |
| / | CIS PXD7 | / | GPIO39 |  |
