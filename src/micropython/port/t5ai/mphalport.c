/*
 * MicroPython Hardware Abstraction Layer Implementation for T5AI
 */

#include <stdio.h>
#include <string.h>
#include "mphalport.h"
#include "tal_system.h"
#include "tal_log.h"
#include "tal_uart.h"
#include "tkl_gpio.h"

/* UART handle for REPL */
TUYA_UART_NUM_E sg_repl_uart_num = TUYA_UART_NUM_0;

/* Readline history buffer - global array for command history */
const char *readline_hist[8] = {0};

/* System tick functions */
mp_uint_t mp_hal_ticks_ms(void)
{
    return tal_system_get_millisecond();
}

mp_uint_t mp_hal_ticks_us(void)
{
    /* TuyaOpen doesn't provide microsecond precision, approximate */
    return tal_system_get_millisecond() * 1000;
}

void mp_hal_delay_ms(mp_uint_t ms)
{
    tal_system_sleep(ms);
}

void mp_hal_delay_us(mp_uint_t us)
{
    /* TuyaOpen sleep has millisecond precision minimum */
    if (us >= 1000) {
        tal_system_sleep(us / 1000);
    } else {
        /* Busy wait for microseconds */
        uint32_t start = mp_hal_ticks_us();
        while ((mp_hal_ticks_us() - start) < us) {
            /* Busy wait */
        }
    }
}

/* UART/Console I/O functions */
int mp_hal_stdin_rx_chr(void)
{
    uint8_t ch;

    /* Blocking read from UART - tal_uart_read returns number of bytes read */
    int bytes_read = tal_uart_read(sg_repl_uart_num, &ch, 1);
    if (bytes_read > 0) {
        return ch;
    }
    return -1;
}

void mp_hal_stdout_tx_str(const char *str)
{
    mp_hal_stdout_tx_strn(str, strlen(str));
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len)
{
    /* Send to UART */
    tal_uart_write(sg_repl_uart_num, (const uint8_t*)str, (uint32_t)len);
    return len;  /* Return number of bytes written */
}

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len)
{
    /* Handle line endings */
    const char *last = str;
    while (len--) {
        if (*str == '\n') {
            if (str > last) {
                tal_uart_write(sg_repl_uart_num, (const uint8_t*)last, (uint32_t)(str - last));
            }
            tal_uart_write(sg_repl_uart_num, (const uint8_t*)"\r\n", 2);
            last = str + 1;
        }
        str++;
    }
    if (str > last) {
        tal_uart_write(sg_repl_uart_num, (const uint8_t*)last, (uint32_t)(str - last));
    }
}

/* Interrupt control */
void mp_hal_disable_irq(void)
{
    /* TODO: Implement interrupt disable for T5AI */
    /* tal_cpu_irq_disable(); */
}

void mp_hal_enable_irq(void)
{
    /* TODO: Implement interrupt enable for T5AI */
    /* tal_cpu_irq_enable(); */
}

/* Set the character that triggers keyboard interrupt (usually Ctrl+C) */
static int interrupt_char = -1;

void mp_hal_set_interrupt_char(int c)
{
    interrupt_char = c;
}

uint32_t mp_hal_get_cpu_freq(void)
{
    /* T5AI BK7258 default frequency */
    return 120000000; /* 120MHz */
}

/* Reset and power control */
void mp_hal_reset(void)
{
    PR_NOTICE("System reset requested");
    tal_system_reset();
}

void mp_hal_deep_sleep(void)
{
    /* TODO: Implement deep sleep for T5AI */
    PR_NOTICE("Deep sleep not yet implemented");
}

/* GPIO functions (minimal) */
void mp_hal_pin_config(uint32_t pin, uint32_t mode, uint32_t pull)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {0};

    switch (mode) {
        case MP_HAL_PIN_MODE_INPUT:
            gpio_cfg.mode = TUYA_GPIO_PULLUP;
            gpio_cfg.direct = TUYA_GPIO_INPUT;
            break;
        case MP_HAL_PIN_MODE_OUTPUT:
            gpio_cfg.mode = TUYA_GPIO_PULLUP;
            gpio_cfg.direct = TUYA_GPIO_OUTPUT;
            gpio_cfg.level = TUYA_GPIO_LEVEL_LOW;
            break;
        default:
            return;
    }

    tkl_gpio_init(pin, &gpio_cfg);
}

void mp_hal_pin_set(uint32_t pin, uint32_t value)
{
    tkl_gpio_write(pin, value ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
}

uint32_t mp_hal_pin_get(uint32_t pin)
{
    TUYA_GPIO_LEVEL_E level;
    if (tkl_gpio_read(pin, &level) == OPRT_OK) {
        return (level == TUYA_GPIO_LEVEL_HIGH) ? 1 : 0;
    }
    return 0;
}

/* HAL initialization */
int mp_hal_init(void)
{
    /* Initialize UART for REPL */
    TAL_UART_CFG_T uart_cfg = {0};
    uart_cfg.base_cfg.baudrate = 115200;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.base_cfg.flowctrl = TUYA_UART_FLOWCTRL_NONE;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.open_mode = O_BLOCK;

    OPERATE_RET ret = tal_uart_init(sg_repl_uart_num, &uart_cfg);
    if (OPRT_OK != ret) {
        PR_ERR("Failed to initialize REPL UART: %d", ret);
        return -1;
    }

    PR_NOTICE("HAL initialized for T5AI");
    return 0;
}
