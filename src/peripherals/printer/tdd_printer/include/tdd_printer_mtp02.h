/**
 * @file tdd_printer_mtp02.h
 * @brief MTP02-DXD thermal printer mechanism TDD driver
 * @version 0.1
 * @date 2026-03-30
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * Compatible with SII LTP02-245 series thermal printer mechanisms.
 * Hardware interface: GPIO bit-bang (DI, CLK, LAT, DST, motor, paper sensor).
 * Thermal head: 384 dots/line, 48mm print width, 6 blocks x 64 dots.
 * Motor: PM bipolar stepper, 1-2 phase excitation (half-step), 8 steps/dot line.
 */

#ifndef __TDD_PRINTER_MTP02_H__
#define __TDD_PRINTER_MTP02_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tdl_printer_driver.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define MTP02_DOTS_PER_LINE         384
#define MTP02_BYTES_PER_LINE        (MTP02_DOTS_PER_LINE / 8)
#define MTP02_HEAD_BLOCKS           12
#define MTP02_DOTS_PER_BLOCK        32
#define MTP02_MAX_ACTIVE_DOTS       32
#define MTP02_STEPS_PER_DOT_LINE    4
#define MTP02_MOTOR_PHASES          8
#define MTP02_MOTOR_RAMP_STEPS      9

#define MTP02_MOTOR_PWM_FREQ        20000
#define MTP02_MOTOR_PWM_DUTY_MAX    10000
#define MTP02_MOTOR_PWM_DUTY_FULL   10000
#define MTP02_MOTOR_PWM_DUTY_HALF   7070

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    TUYA_GPIO_NUM_E     pin_power_en;

    TUYA_GPIO_NUM_E     pin_di;
    TUYA_GPIO_NUM_E     pin_clk;
    TUYA_SPI_NUM_E      spi_port;
    uint32_t            spi_clk;

    TUYA_GPIO_NUM_E     pin_lat;
    TUYA_GPIO_NUM_E     pin_strobe;

    TUYA_GPIO_NUM_E     pin_motor_en;
    TUYA_PWM_NUM_E      pwm_motor_a1;
    TUYA_PWM_NUM_E      pwm_motor_a2;
    TUYA_PWM_NUM_E      pwm_motor_b1;
    TUYA_PWM_NUM_E      pwm_motor_b2;

    TUYA_GPIO_NUM_E     pin_paper_sensor;

    TUYA_ADC_NUM_E      adc_port;
    uint8_t             adc_ch;
    int32_t             temp_adc_threshold;

    uint32_t            strobe_time_ms;
    uint32_t            motor_step_ms;
    uint32_t            motor_pwm_duty;
} TDD_PRINTER_MTP02_CFG_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */

/**
 * @brief Register MTP02-DXD thermal printer driver to TDL layer
 * @param[in] name printer name for TDL registration
 * @param[in] cfg hardware pin and timing configuration
 * @return OPRT_OK on success, error code on failure
 * @note The write interface expects raw dot bitmap data, each line is
 *       MTP02_BYTES_PER_LINE (48) bytes. Data length must be a multiple of 48.
 *       Bit order: MSB first, data[0] bit7 = dot #1 (leftmost).
 */
OPERATE_RET tdd_printer_mtp02_register(char *name, TDD_PRINTER_MTP02_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PRINTER_MTP02_H__ */
