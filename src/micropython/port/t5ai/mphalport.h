/*
 * MicroPython Hardware Abstraction Layer for T5AI
 */

#ifndef MICROPYTHON_MPHALPORT_H
#define MICROPYTHON_MPHALPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  /* For size_t */

/* Include MicroPython types */
#include "py/mpconfig.h"

/* HAL initialization */
int mp_hal_init(void);

/* System tick functions - match MicroPython's expected signatures */
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);

/* UART/Console I/O functions */
int mp_hal_stdin_rx_chr(void);
void mp_hal_stdout_tx_str(const char *str);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);  /* Return type matches MicroPython's expectation */
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len);
void mp_hal_set_interrupt_char(int c);  /* Set character for keyboard interrupt */

/* Interrupt control */
void mp_hal_disable_irq(void);
void mp_hal_enable_irq(void);
uint32_t mp_hal_get_cpu_freq(void);

/* Reset and power control */
void mp_hal_reset(void);
void mp_hal_deep_sleep(void);

/* GPIO functions (minimal) */
#define MP_HAL_PIN_MODE_INPUT   0
#define MP_HAL_PIN_MODE_OUTPUT  1
#define MP_HAL_PIN_MODE_ALT     2

void mp_hal_pin_config(uint32_t pin, uint32_t mode, uint32_t pull);
void mp_hal_pin_set(uint32_t pin, uint32_t value);
uint32_t mp_hal_pin_get(uint32_t pin);

#endif /* MICROPYTHON_MPHALPORT_H */
