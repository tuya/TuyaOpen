# ESP32 Pinmux 对照表

`TUYA_GPIO_NUM_<n>` 对应芯片数据手册中的 `GPIO<n>`。本表列出 TKL 驱动的默认引脚及其可重映射能力。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_23` | GPIO0–GPIO23 |  |
| `TUYA_GPIO_NUM_24` | GPIO24 | 不可用 |
| `TUYA_GPIO_NUM_25`–`TUYA_GPIO_NUM_27` | GPIO25–GPIO27 |  |
| `TUYA_GPIO_NUM_28`–`TUYA_GPIO_NUM_31` | GPIO28–GPIO31 | 不可用 |
| `TUYA_GPIO_NUM_32`–`TUYA_GPIO_NUM_33` | GPIO32–GPIO33 |  |
| `TUYA_GPIO_NUM_34`–`TUYA_GPIO_NUM_39` | GPIO34–GPIO39 | 仅输入 |

## I2C

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SCL` | GPIO0 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SDA` | GPIO1 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO2 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO3 | 可经 Pinmux 重映射。 |

## UART

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_TX` | `GPIO<CONFIG_UART_NUM0_TX_PIN>` | 由 Kconfig 配置；不经 Pinmux 重映射。 |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_RX` | `GPIO<CONFIG_UART_NUM0_RX_PIN>` | 由 Kconfig 配置；不经 Pinmux 重映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | GPIO10 | 可经 Pinmux 重映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | GPIO9 | 可经 Pinmux 重映射。 |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_0` | LEDC 通道 0 | `TUYA_PWM0` | GPIO18 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_1` | LEDC 通道 1 | `TUYA_PWM1` | GPIO19 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_2` | LEDC 通道 2 | `TUYA_PWM2` | GPIO22 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_3` | LEDC 通道 3 | `TUYA_PWM3` | GPIO23 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_4` | LEDC 通道 4 | `TUYA_PWM4` | GPIO25 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_5` | LEDC 通道 5 | `TUYA_PWM5` | GPIO26 | 可经 Pinmux 重映射。 |

## SPI

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MOSI` | GPIO11 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MISO` | GPIO13 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CLK` | GPIO12 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CS` | GPIO10 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_MOSI` | GPIO23 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_MISO` | GPIO19 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_CLK` | GPIO18 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_CS` | GPIO5 | 可经 Pinmux 重映射。 |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 0` | ADC1_CH0 | GPIO36 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 1` | ADC1_CH1 | GPIO37 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 2` | ADC1_CH2 | GPIO38 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 3` | ADC1_CH3 | GPIO39 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 4` | ADC1_CH4 | GPIO32 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 5` | ADC1_CH5 | GPIO33 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 6` | ADC1_CH6 | GPIO34 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 7` | ADC1_CH7 | GPIO35 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 0` | ADC2_CH0 | GPIO4 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 1` | ADC2_CH1 | GPIO0 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 2` | ADC2_CH2 | GPIO2 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 3` | ADC2_CH3 | GPIO15 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 4` | ADC2_CH4 | GPIO13 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 5` | ADC2_CH5 | GPIO12 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 6` | ADC2_CH6 | GPIO14 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 7` | ADC2_CH7 | GPIO27 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 8` | ADC2_CH8 | GPIO25 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 9` | ADC2_CH9 | GPIO26 |  |
