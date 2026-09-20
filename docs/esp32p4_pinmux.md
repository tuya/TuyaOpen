# ESP32-P4 Pinmux 对照表

`TUYA_GPIO_NUM_<n>` 对应芯片数据手册中的 `GPIO<n>`。本表列出 TKL 驱动的默认引脚及其可重映射能力。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_54` | GPIO0–GPIO54 |  |

## I2C

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SCL` | GPIO0 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SDA` | GPIO1 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | GPIO2 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | GPIO3 | 可经 Pinmux 重映射。 |
| `TUYA_I2C_NUM_2` | I2C2 | `TUYA_IIC2_SCL` | 任意可输出 GPIO | 无默认引脚，需经 Pinmux 配置。 |
| `TUYA_I2C_NUM_2` | I2C2 | `TUYA_IIC2_SDA` | 任意可输出 GPIO | 无默认引脚，需经 Pinmux 配置。 |

## UART

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_TX` | `GPIO<CONFIG_UART_NUM0_TX_PIN>` | 由 Kconfig 配置；不经 Pinmux 重映射。 |
| `TUYA_UART_NUM_0` | UART0 | `TUYA_UART0_RX` | `GPIO<CONFIG_UART_NUM0_RX_PIN>` | 由 Kconfig 配置；不经 Pinmux 重映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | GPIO10 | 可经 Pinmux 重映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | GPIO11 | 可经 Pinmux 重映射。 |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_0` | LEDC 通道 0 | `TUYA_PWM0` | GPIO18 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_1` | LEDC 通道 1 | `TUYA_PWM1` | GPIO19 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_2` | LEDC 通道 2 | `TUYA_PWM2` | GPIO20 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_3` | LEDC 通道 3 | `TUYA_PWM3` | GPIO21 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_4` | LEDC 通道 4 | `TUYA_PWM4` | GPIO22 | 可经 Pinmux 重映射。 |
| `TUYA_PWM_NUM_5` | LEDC 通道 5 | `TUYA_PWM5` | GPIO23 | 可经 Pinmux 重映射。 |

## SPI

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MOSI` | GPIO10 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_MISO` | GPIO11 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CLK` | GPIO9 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_0` | SPI2 | `TUYA_SPI0_CS` | GPIO7 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_MOSI` | GPIO46 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_MISO` | GPIO27 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_CLK` | GPIO53 | 可经 Pinmux 重映射。 |
| `TUYA_SPI_NUM_1` | SPI3 | `TUYA_SPI1_CS` | GPIO47 | 可经 Pinmux 重映射。 |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 0` | ADC1_CH0 | GPIO16 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 1` | ADC1_CH1 | GPIO17 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 2` | ADC1_CH2 | GPIO18 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 3` | ADC1_CH3 | GPIO19 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 4` | ADC1_CH4 | GPIO20 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 5` | ADC1_CH5 | GPIO21 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 6` | ADC1_CH6 | GPIO22 |  |
| `TUYA_ADC_NUM_0` | ADC1 | `ch_id = 7` | ADC1_CH7 | GPIO23 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 0` | ADC2_CH0 | GPIO49 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 1` | ADC2_CH1 | GPIO50 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 2` | ADC2_CH2 | GPIO51 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 3` | ADC2_CH3 | GPIO52 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 4` | ADC2_CH4 | GPIO53 |  |
| `TUYA_ADC_NUM_1` | ADC2 | `ch_id = 5` | ADC2_CH5 | GPIO54 |  |

## I2S

I2S 驱动仅在启用 `CONFIG_ENABLE_AUDIO_CODECS` 时编入。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SCK` | GPIO12 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_WS` | GPIO10 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SDO_0` | GPIO11 | 驱动固定引脚。 |
| `TUYA_I2S_NUM_0` | I2S0 | `TUYA_I2S0_SDI_0` | GPIO11 | 与 SDO 共用 GPIO11。 |
