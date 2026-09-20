# <芯片型号> Pinmux 对照表

本文档用于汇总 TKL 驱动已适配的外设端口、信号与芯片 GPIO 的对照关系，供开发者按数据手册和原理图完成外设接线与配置。表格同时记录驱动默认引脚、固定引脚或可重映射能力。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_<起始>`–`TUYA_GPIO_NUM_<结束>` | <GPIO 起始>–<GPIO 结束> |  |
| `TUYA_GPIO_NUM_<n>` | <GPIOn> | <不可用 / 输入专用 / 启动脚等限制> |

## I2C

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2C_NUM_<n>` | I2C<n> | `TUYA_IIC<n>_SCL` | <驱动默认/固定 GPIO> |  |
| `TUYA_I2C_NUM_<n>` | I2C<n> | `TUYA_IIC<n>_SDA` | <驱动默认/固定 GPIO> |  |

## UART

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_UART_NUM_<n>` | UART<n> | `TUYA_UART<n>_TX` | <驱动默认/固定 GPIO> |  |
| `TUYA_UART_NUM_<n>` | UART<n> | `TUYA_UART<n>_RX` | <驱动默认/固定 GPIO> |  |
| `TUYA_UART_NUM_<n>` | UART<n> | `TUYA_UART<n>_RTS` | <驱动默认/固定 GPIO> |  |
| `TUYA_UART_NUM_<n>` | UART<n> | `TUYA_UART<n>_CTS` | <驱动默认/固定 GPIO> |  |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_<n>` | <PWM / 定时器通道> | `TUYA_PWM<n>` | <驱动默认/固定 GPIO> |  |

## SPI

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SPI_NUM_<n>` | SPI<n> | `TUYA_SPI<n>_MOSI` | <驱动默认/固定 GPIO> |  |
| `TUYA_SPI_NUM_<n>` | SPI<n> | `TUYA_SPI<n>_MISO` | <驱动默认/固定 GPIO> |  |
| `TUYA_SPI_NUM_<n>` | SPI<n> | `TUYA_SPI<n>_CLK` | <驱动默认/固定 GPIO> |  |
| `TUYA_SPI_NUM_<n>` | SPI<n> | `TUYA_SPI<n>_CS` | <驱动默认/固定 GPIO> |  |

## QSPI

QSPI 没有通用 `TUYA_PIN_FUNC_E` 信号标识，因此“TKL 标识”列记为 `/`。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> CLK | / | <GPIO> |  |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> CS | / | <GPIO> |  |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> IO0 | / | <GPIO> |  |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> IO1 | / | <GPIO> |  |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> IO2 | / | <GPIO> |  |
| `TUYA_QSPI_NUM_<n>` | QSPI<n> IO3 | / | <GPIO> |  |

## RGB

单实例 RGB 接口使用固定引脚时，TKL 端口和 TKL 标识列填写 `/`。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | RGB R<n> | / | <固定 GPIO> |  |
| / | RGB G<n> | / | <固定 GPIO> |  |
| / | RGB B<n> | / | <固定 GPIO> |  |
| / | RGB CLK | / | <固定 GPIO> |  |
| / | RGB DE | / | <固定 GPIO> |  |
| / | RGB HSYNC | / | <固定 GPIO> |  |
| / | RGB VSYNC | / | <固定 GPIO> |  |

## 8080

单实例 8080 接口使用固定引脚时，TKL 端口和 TKL 标识列填写 `/`。数据线按驱动支持的总线位宽逐根列出。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | 8080 D<n> | / | <固定 GPIO> |  |
| / | 8080 RD | / | <固定 GPIO> |  |
| / | 8080 WR | / | <固定 GPIO> |  |
| / | 8080 DC | / | <固定 GPIO> |  |
| / | 8080 RESET | / | <固定 GPIO> |  |
| / | 8080 CS | / | <固定 GPIO> |  |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_<n>` | ADC<n> | `ch_id = <n>` | ADC<n>_CH<n> | GPIO<n> |  |

若平台通过 `TUYA_ADC<n>` 配置 pinmux，应在“TKL 通道”列填写该标识；若 ADC API 使用 `ch_id`，则按上表填写。

## DAC

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_DAC_NUM_<n>` | DAC<n> | `TUYA_DAC<n>` | <固定 GPIO 或 N/A> |  |

## I2S

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2S_NUM_<n>` | I2S<n> | `TUYA_I2S<n>_SCK` | <驱动默认/固定 GPIO> |  |
| `TUYA_I2S_NUM_<n>` | I2S<n> | `TUYA_I2S<n>_WS` | <驱动默认/固定 GPIO> |  |
| `TUYA_I2S_NUM_<n>` | I2S<n> | `TUYA_I2S<n>_SDO_0` | <驱动默认/固定 GPIO> |  |
| `TUYA_I2S_NUM_<n>` | I2S<n> | `TUYA_I2S<n>_SDI_0` | <驱动默认/固定 GPIO> |  |

## SDIO

- 1-bit 使用 CLK、CMD、D0；4-bit 使用 CLK、CMD、D0–D3。
- 同一 SDIO 端口的信号从同一套引脚映射中选择。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_CLK` | <固定/可选 GPIO> |  |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_CMD` | <固定/可选 GPIO> |  |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_D0` | <固定/可选 GPIO> |  |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_D1` | <固定/可选 GPIO> |  |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_D2` | <固定/可选 GPIO> |  |
| <平台 SDIO 主机标识> | SDIO<n> | `TUYA_SDIO_HOST_D3` | <固定/可选 GPIO> |  |

## DVP

单实例 DVP 接口使用固定引脚时，TKL 端口和 TKL 标识列填写 `/`。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| / | DVP MCLK | / | <固定 GPIO> |  |
| / | DVP PCLK | / | <固定 GPIO> |  |
| / | DVP HSYNC | / | <固定 GPIO> |  |
| / | DVP VSYNC | / | <固定 GPIO> |  |
| / | DVP D<n> | / | <固定 GPIO> |  |
