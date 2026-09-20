# ESP32-C6 Pinmux 对照表

`TUYA_GPIO_NUM_<n>` 对应芯片数据手册中的 `GPIO<n>`。本表列出 TKL 驱动的默认引脚及其可重映射能力。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_30` | GPIO0–GPIO30 |  |

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
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | GPIO6 | 可经 Pinmux 重映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | GPIO7 | 可经 Pinmux 重映射。 |

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
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MOSI` | GPIO7 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MISO` | GPIO2 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CLK` | GPIO6 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CS` | GPIO16 | 可经 Pinmux 重映射。 |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 0` | ADC1_CH0 | GPIO0 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 1` | ADC1_CH1 | GPIO1 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 2` | ADC1_CH2 | GPIO2 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 3` | ADC1_CH3 | GPIO3 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 4` | ADC1_CH4 | GPIO4 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 5` | ADC1_CH5 | GPIO5 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 6` | ADC1_CH6 | GPIO6 |  |
