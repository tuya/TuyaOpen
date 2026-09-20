# GD32VW553 Pinmux 对照表

GPIO 接口使用 `TUYA_GPIO_NUM_<n>`；Pinmux 配置接口使用数值相同的 `TUYA_IO_PIN_<n>`。本表列出 TKL 驱动的实际引脚映射。

## GPIO

| TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- |
| `TUYA_GPIO_NUM_0`–`TUYA_GPIO_NUM_15` | PA0–PA15 |  |
| `TUYA_GPIO_NUM_16`–`TUYA_GPIO_NUM_20` | PB0–PB4 |  |
| `TUYA_GPIO_NUM_21`–`TUYA_GPIO_NUM_23` | PB11–PB13 |  |
| `TUYA_GPIO_NUM_24` | PB15 |  |
| `TUYA_GPIO_NUM_25` | PC8 |  |
| `TUYA_GPIO_NUM_26`–`TUYA_GPIO_NUM_28` | PC13–PC15 |  |

## I2C

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SCL` | PA2 |  |
| `TUYA_I2C_NUM_0` | I2C0 | `TUYA_IIC0_SDA` | PA3 |  |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SCL` | PA15 |  |
| `TUYA_I2C_NUM_1` | I2C1 | `TUYA_IIC1_SDA` | PA8 |  |

除上述硬件 I2C 管脚外，当前 TKL 适配层可将 I2C 配置为软件 I2C，并使用任意有效 GPIO。

## UART

- UART0、UART1 的 TX、RX 存在多种“可选映射”，可在初始化前通过 `tkl_io_pinmux_config` 分别选择；无需整组替换。`uart.h` 的默认引脚随 `CONFIG_BOARD` 变化，因此不在芯片级文档中指定默认组。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_TX` | PA0 | 可选映射。 |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_TX` | PB15 | 可选映射。 |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_RX` | PA1 | 可选映射。 |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_RX` | PA8 | 可选映射。 |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_RX` | PA15 | 可选映射。 |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_RTS` | PA3 |  |
| `TUYA_UART_NUM_0` | USART0 | `TUYA_UART0_CTS` | PA2 |  |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | PA4 | 可选映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_TX` | PB15 | 可选映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | PA5 | 可选映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RX` | PA8 | 可选映射。 |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_RTS` | PA1 |  |
| `TUYA_UART_NUM_1` | UART1 | `TUYA_UART1_CTS` | PA0 |  |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_TX` | PA6 |  |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_RX` | PA7 |  |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_RTS` | PB1 |  |
| `TUYA_UART_NUM_2` | UART2 | `TUYA_UART2_CTS` | PB0 |  |

## PWM

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_PWM_NUM_0` | TIMER0_CH0 | `TUYA_PWM0` | PA8 |  |
| `TUYA_PWM_NUM_1` | TIMER15_CH0 | `TUYA_PWM1` | PB13 |  |
| `TUYA_PWM_NUM_2` | TIMER16_CH0 | `TUYA_PWM2` | PA10 |  |

## SPI

- 当前 `tkl_spi_init()` 固定初始化下列引脚；先前通过 Pinmux 配置的其他 SPI 引脚会被该驱动覆盖。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MOSI` | PA9 | 固定引脚。 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_MISO` | PA10 | 固定引脚。 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CLK` | PA11 | 固定引脚。 |
| `TUYA_SPI_NUM_0` | SPI0 | `TUYA_SPI0_CS` | PA12 | 固定引脚。 |

## QSPI

- 当前 TKL QSPI 驱动固定使用下列引脚，暂不支持通过 Pinmux 切换引脚组。

| TKL 端口 | 芯片数据手册端口 | TKL 标识 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- |
| `TUYA_QSPI_NUM_0` | QSPI SCK | / | PA4 | 固定引脚。 |
| `TUYA_QSPI_NUM_0` | QSPI CS | / | PA5 | 固定引脚。 |
| `TUYA_QSPI_NUM_0` | QSPI IO0 | / | PA6 | 固定引脚。 |
| `TUYA_QSPI_NUM_0` | QSPI IO1 | / | PA7 | 固定引脚。 |
| `TUYA_QSPI_NUM_0` | QSPI IO2 | / | PB3 | 仅四线模式使用。 |
| `TUYA_QSPI_NUM_0` | QSPI IO3 | / | PB4 | 仅四线模式使用。 |

## ADC

| TKL 端口 | 芯片数据手册端口 | TKL 通道 | 芯片数据手册通道 | 芯片数据手册 GPIO | 说明 |
| --- | --- | --- | --- | --- | --- |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 0` | ADC_IN0 | PA0 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 1` | ADC_IN1 | PA1 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 2` | ADC_IN2 | PA2 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 3` | ADC_IN3 | PA3 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 4` | ADC_IN4 | PA4 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 5` | ADC_IN5 | PA5 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 6` | ADC_IN6 | PA6 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 7` | ADC_IN7 | PA7 |  |
| `TUYA_ADC_NUM_0` | ADC | `ch_id = 8` | ADC_IN8 | PB0 |  |
