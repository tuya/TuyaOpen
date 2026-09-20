# ESP32-S3 Pinmux 对照表

`TUYA_GPIO_NUM_<n>` 对应芯片数据手册中的 `GPIO<n>`。本表列出 TKL 驱动的默认引脚及其可重映射能力。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_21` | GPIO0–GPIO21 |  |
| `TUYA_GPIO_NUM_22`–`TUYA_GPIO_NUM_25` | GPIO22–GPIO25 | 不可用 |
| `TUYA_GPIO_NUM_26`–`TUYA_GPIO_NUM_48` | GPIO26–GPIO48 |  |

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
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_TX` | `GPIO<CONFIG_UART_NUM0_TX_PIN>` | 由 Kconfig 配置；USB-JTAG-only 模式不使用 GPIO。 |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_RX` | `GPIO<CONFIG_UART_NUM0_RX_PIN>` | 由 Kconfig 配置；USB-JTAG-only 模式不使用 GPIO。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | GPIO17 | 可经 Pinmux 重映射；USB-JTAG-only 模式不支持。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | GPIO18 | 可经 Pinmux 重映射；USB-JTAG-only 模式不支持。 |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_0` | LEDC 通道 0 | `TUYA_PWM0` | GPIO18 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_1` | LEDC 通道 1 | `TUYA_PWM1` | GPIO19 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_2` | LEDC 通道 2 | `TUYA_PWM2` | GPIO38 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_3` | LEDC 通道 3 | `TUYA_PWM3` | GPIO39 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_4` | LEDC 通道 4 | `TUYA_PWM4` | GPIO40 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_5` | LEDC 通道 5 | `TUYA_PWM5` | GPIO41 | 可经 Pinmux 重映射。 |

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
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 0` | ADC1_CH0 | GPIO1 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 1` | ADC1_CH1 | GPIO2 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 2` | ADC1_CH2 | GPIO3 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 3` | ADC1_CH3 | GPIO4 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 4` | ADC1_CH4 | GPIO5 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 5` | ADC1_CH5 | GPIO6 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 6` | ADC1_CH6 | GPIO7 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 7` | ADC1_CH7 | GPIO8 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 8` | ADC1_CH8 | GPIO9 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 9` | ADC1_CH9 | GPIO10 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 0` | ADC2_CH0 | GPIO11 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 1` | ADC2_CH1 | GPIO12 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 2` | ADC2_CH2 | GPIO13 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 3` | ADC2_CH3 | GPIO14 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 4` | ADC2_CH4 | GPIO15 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 5` | ADC2_CH5 | GPIO16 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 6` | ADC2_CH6 | GPIO17 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 7` | ADC2_CH7 | GPIO18 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 8` | ADC2_CH8 | GPIO19 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 9` | ADC2_CH9 | GPIO20 |  |

## I2S

I2S 驱动仅在启用 `CONFIG_ENABLE_AUDIO_CODECS` 时编入。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SCK` | GPIO5 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_WS` | GPIO4 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SDO_0` | GPIO6 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SDI_0` | GPIO6 | 与 SDO 共用 GPIO6。 |
| `TUYA_I2S_NUM_1` | I2S1 | `TUYA_I2S1_SCK` | GPIO15 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S1 | `TUYA_I2S1_WS` | GPIO16 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S1 | `TUYA_I2S1_SDO_0` | GPIO7 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_1` | I2S1 | `TUYA_I2S1_SDI_0` | GPIO7 | 与 SDO 共用 GPIO7。 |
