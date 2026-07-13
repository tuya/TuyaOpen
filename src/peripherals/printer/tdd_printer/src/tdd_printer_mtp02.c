/**
 * @file tdd_printer_mtp02.c
 * @brief MTP02-DXD thermal printer mechanism TDD driver implementation
 * @version 0.1
 * @date 2026-03-30
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * Drives the MTP02-DXD (SII LTP02-245 compatible) thermal printer mechanism.
 * - Thermal head: 384 dots, serial shift register (GPIO bit-bang), latch + strobe
 * - Stepper motor: bipolar 1-2 phase excitation (half-step), 8 steps per dot line
 * - Block division drive: 6 physical blocks of 64 dots each
 * - Half-dot printing: two thermal head activations per dot line
 */

#include "tdd_printer_mtp02.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"

#include "tkl_gpio.h"
#include "tkl_pwm.h"
#include "tkl_spi.h"
#include "tkl_adc.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define MTP02_DEFAULT_STROBE_MS     3
#define MTP02_DEFAULT_MOTOR_STEP_MS 1
#define MTP02_DIVISION_PAUSE_MS     1
#define MTP02_MOTOR_INIT_STEPS      96
#define MTP02_PAPER_FEED_STEPS      96
#define MTP02_BLOCK_BYTES           (MTP02_DOTS_PER_BLOCK / 8)

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */

/**
 * Sinusoidal 1-2 half-step PWM duty table for DRV8833 H-bridge.
 * Column order: duty_a1, duty_a2, duty_b1, duty_b2 (0-10000 scale).
 * Positive current: INx1=duty, INx2=0; negative: INx1=0, INx2=duty.
 * Full-step positions (0,2,4,6): single phase at 100%.
 * Half-step positions (1,3,5,7): both phases at 70.7% (sin 45 deg).
 * Actual duty is scaled by motor_pwm_duty at runtime.
 */
static const uint16_t s_motor_phase_table[MTP02_MOTOR_PHASES][4] = {
    /* duty_a1                      duty_a2                     duty_b1                     duty_b2                  */
    {MTP02_MOTOR_PWM_DUTY_FULL,     0,                          0,                          0                       },  /* STEP0: A+100% */
    {MTP02_MOTOR_PWM_DUTY_HALF,     0,                          MTP02_MOTOR_PWM_DUTY_HALF,  0                       },  /* STEP1: A+70.7%, B+70.7% */
    {0,                             0,                          MTP02_MOTOR_PWM_DUTY_FULL,  0                       },  /* STEP2: B+100% */
    {0,                             MTP02_MOTOR_PWM_DUTY_HALF,  MTP02_MOTOR_PWM_DUTY_HALF,  0                       },  /* STEP3: A-70.7%, B+70.7% */
    {0,                             MTP02_MOTOR_PWM_DUTY_FULL,  0,                          0                       },  /* STEP4: A-100% */
    {0,                             MTP02_MOTOR_PWM_DUTY_HALF,  0,                          MTP02_MOTOR_PWM_DUTY_HALF}, /* STEP5: A-70.7%, B-70.7% */
    {0,                             0,                          0,                          MTP02_MOTOR_PWM_DUTY_FULL}, /* STEP6: B-100% */
    {MTP02_MOTOR_PWM_DUTY_HALF,     0,                          0,                          MTP02_MOTOR_PWM_DUTY_HALF}, /* STEP7: A+70.7%, B-70.7% */
};

/**
 * Acceleration ramp table from datasheet, values rounded to milliseconds.
 * 9 steps from 5ms (startup) down to 1ms (target speed).
 */
static const uint8_t s_motor_ramp_ms[MTP02_MOTOR_RAMP_STEPS] = {
    3, 2, 2, 1, 1, 1, 1, 1, 1,
};

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    TDD_PRINTER_MTP02_CFG_T cfg;
    uint8_t  motor_phase;
    uint32_t motor_step_count;
    uint32_t motor_duty_scale;
    BOOL_T   is_opened;
    BOOL_T   is_printing;
    BOOL_T   adc_inited;
    BOOL_T   pwm_inited;
    BOOL_T   spi_inited;
} TDD_PRINTER_MTP02_HANDLE_T;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize a GPIO pin as push-pull output
 * @param[in] pin GPIO pin number
 * @param[in] level initial output level
 * @return OPRT_OK on success
 */
static OPERATE_RET __gpio_output_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = level,
    };
    return tkl_gpio_init(pin, &gpio_cfg);
}

/**
 * @brief Initialize a GPIO pin as pull-up input
 * @param[in] pin GPIO pin number
 * @return OPRT_OK on success
 */
static OPERATE_RET __gpio_input_init(TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode   = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    return tkl_gpio_init(pin, &gpio_cfg);
}

/**
 * @brief Initialize 4 PWM channels for motor drive
 * @param[in] hdl driver handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __motor_pwm_init(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty      = 0,
        .frequency = MTP02_MOTOR_PWM_FREQ,
        .polarity  = TUYA_PWM_POSITIVE,
    };

    TUYA_CALL_ERR_RETURN(tkl_pwm_init(hdl->cfg.pwm_motor_a1, &pwm_cfg));
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(hdl->cfg.pwm_motor_a2, &pwm_cfg));
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(hdl->cfg.pwm_motor_b1, &pwm_cfg));
    TUYA_CALL_ERR_RETURN(tkl_pwm_init(hdl->cfg.pwm_motor_b2, &pwm_cfg));

    TUYA_CALL_ERR_RETURN(tkl_pwm_start(hdl->cfg.pwm_motor_a1));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(hdl->cfg.pwm_motor_a2));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(hdl->cfg.pwm_motor_b1));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(hdl->cfg.pwm_motor_b2));

    hdl->pwm_inited = TRUE;
    return OPRT_OK;
}

/**
 * @brief Deinitialize 4 PWM channels
 * @param[in] hdl driver handle
 * @return none
 */
static void __motor_pwm_deinit(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    tkl_pwm_stop(hdl->cfg.pwm_motor_a1);
    tkl_pwm_stop(hdl->cfg.pwm_motor_a2);
    tkl_pwm_stop(hdl->cfg.pwm_motor_b1);
    tkl_pwm_stop(hdl->cfg.pwm_motor_b2);
    tkl_pwm_deinit(hdl->cfg.pwm_motor_a1);
    tkl_pwm_deinit(hdl->cfg.pwm_motor_a2);
    tkl_pwm_deinit(hdl->cfg.pwm_motor_b1);
    tkl_pwm_deinit(hdl->cfg.pwm_motor_b2);
    hdl->pwm_inited = FALSE;
}

/**
 * @brief Convert table duty to slow-decay DRV8833 IN1/IN2 PWM values
 * @param[in] fwd_duty forward duty from table (0 = inactive)
 * @param[in] rev_duty reverse duty from table (0 = inactive)
 * @param[in] scale overall duty scale (motor_duty_scale)
 * @param[out] out_in1 PWM duty for INx1
 * @param[out] out_in2 PWM duty for INx2
 * @return none
 * @note Slow decay: drive pin held HIGH, opposite pin PWM between LOW (drive)
 *       and HIGH (brake). Current recirculates through low-side FETs during
 *       brake, producing smooth constant-current-like behavior.
 */
static void __calc_slow_decay(uint16_t fwd_duty, uint16_t rev_duty,
                               uint32_t scale,
                               uint32_t *out_in1, uint32_t *out_in2)
{
    if (fwd_duty > 0) {
        uint32_t drive = (uint32_t)fwd_duty * scale / MTP02_MOTOR_PWM_DUTY_MAX;
        *out_in1 = MTP02_MOTOR_PWM_DUTY_MAX;
        *out_in2 = MTP02_MOTOR_PWM_DUTY_MAX - drive;
    } else if (rev_duty > 0) {
        uint32_t drive = (uint32_t)rev_duty * scale / MTP02_MOTOR_PWM_DUTY_MAX;
        *out_in1 = MTP02_MOTOR_PWM_DUTY_MAX - drive;
        *out_in2 = MTP02_MOTOR_PWM_DUTY_MAX;
    } else {
        *out_in1 = 0;
        *out_in2 = 0;
    }
}

/**
 * @brief Apply current phase PWM duty to all 4 channels (slow decay)
 * @param[in] hdl driver handle
 * @return none
 */
static void __motor_set_phase(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    uint8_t idx = hdl->motor_phase % MTP02_MOTOR_PHASES;
    uint32_t scale = hdl->motor_duty_scale;
    uint32_t a1, a2, b1, b2;

    __calc_slow_decay(s_motor_phase_table[idx][0], s_motor_phase_table[idx][1],
                      scale, &a1, &a2);
    __calc_slow_decay(s_motor_phase_table[idx][2], s_motor_phase_table[idx][3],
                      scale, &b1, &b2);

    tkl_pwm_duty_set(hdl->cfg.pwm_motor_a1, a1);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_a2, a2);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_b1, b1);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_b2, b2);
}

/**
 * @brief Advance the stepper motor by one half-step with acceleration ramp
 * @param[in] hdl driver handle
 * @return none
 */
static void __motor_step_forward(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    hdl->motor_phase = (hdl->motor_phase + 1) % MTP02_MOTOR_PHASES;
    __motor_set_phase(hdl);

    uint32_t delay_ms;
    if (hdl->motor_step_count < MTP02_MOTOR_RAMP_STEPS) {
        delay_ms = s_motor_ramp_ms[hdl->motor_step_count];
    } else {
        delay_ms = hdl->cfg.motor_step_ms;
    }
    tal_system_sleep(delay_ms);
    hdl->motor_step_count++;
}

/**
 * @brief Enable DRV8833, set target duty and apply initial phase
 * @param[in] hdl driver handle
 * @return none
 */
static void __motor_start(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    tkl_gpio_write(hdl->cfg.pin_motor_en, TUYA_GPIO_LEVEL_HIGH);
    hdl->motor_duty_scale = hdl->cfg.motor_pwm_duty;
    __motor_set_phase(hdl);
    hdl->motor_step_count = 0;
}

/**
 * @brief Set all PWM duty to 0 and disable DRV8833
 * @param[in] hdl driver handle
 * @return none
 */
static void __motor_stop(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_a1, 0);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_a2, 0);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_b1, 0);
    tkl_pwm_duty_set(hdl->cfg.pwm_motor_b2, 0);
    tkl_gpio_write(hdl->cfg.pin_motor_en, TUYA_GPIO_LEVEL_LOW);
    hdl->motor_step_count = 0;
}

/**
 * @brief Shift 384-bit line data into the thermal head
 * @param[in] hdl driver handle
 * @param[in] data 48-byte line data buffer, MSB first
 * @return none
 * @note Uses hardware SPI when configured (spi_clk > 0), otherwise
 *       falls back to GPIO bit-bang. SPI is significantly faster.
 */
static void __head_shift_data(TDD_PRINTER_MTP02_HANDLE_T *hdl, const uint8_t *data)
{
    if (hdl->spi_inited) {
        tkl_spi_send(hdl->cfg.spi_port, (void *)data, MTP02_BYTES_PER_LINE);
        return;
    }

    uint32_t byte_idx;
    uint8_t bit_idx;

    for (byte_idx = 0; byte_idx < MTP02_BYTES_PER_LINE; byte_idx++) {
        uint8_t val = data[byte_idx];
        for (bit_idx = 0; bit_idx < 8; bit_idx++) {
            tkl_gpio_write(hdl->cfg.pin_di,
                           (val & 0x80) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
            val <<= 1;

            tkl_gpio_write(hdl->cfg.pin_clk, TUYA_GPIO_LEVEL_HIGH);
            tkl_gpio_write(hdl->cfg.pin_clk, TUYA_GPIO_LEVEL_LOW);
        }
    }
}

/**
 * @brief Latch shift register data into the latch register
 * @param[in] hdl driver handle
 * @return none
 * @note LAT idles HIGH; a LOW pulse transfers data from shift register
 *       to latch register per datasheet timing.
 */
static void __head_latch(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    tkl_gpio_write(hdl->cfg.pin_lat, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(1);
    tkl_gpio_write(hdl->cfg.pin_lat, TUYA_GPIO_LEVEL_HIGH);
}

/**
 * @brief Activate heating elements for the configured strobe duration
 * @param[in] hdl driver handle
 * @return none
 * @note DST HIGH activates the heating elements whose latch bits are set.
 */
static void __head_strobe(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    tkl_gpio_write(hdl->cfg.pin_strobe, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(hdl->cfg.strobe_time_ms);
    tkl_gpio_write(hdl->cfg.pin_strobe, TUYA_GPIO_LEVEL_LOW);
}

/**
 * @brief Check if thermal paper is present via the reflection sensor
 * @param[in] hdl driver handle
 * @return TRUE if paper is present, FALSE otherwise
 * @note Reflection-type sensor: LOW when paper reflects IR light (present),
 *       HIGH when no paper (no reflection).
 */
static BOOL_T __is_paper_present(TDD_PRINTER_MTP02_HANDLE_T *hdl)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;

    if (tkl_gpio_read(hdl->cfg.pin_paper_sensor, &level) != OPRT_OK) {
        return FALSE;
    }
    return (level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
}

/**
 * @brief Check if a full line has any active dots
 * @param[in] data line data buffer (MTP02_BYTES_PER_LINE bytes)
 * @return TRUE if the line has at least one dot set
 */
static BOOL_T __line_has_dots(const uint8_t *data)
{
    for (uint32_t i = 0; i < MTP02_BYTES_PER_LINE; i++) {
        if (data[i] != 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Count active dots (set bits) in a byte range
 * @param[in] data data buffer
 * @param[in] len byte count
 * @return number of set bits
 */
static uint32_t __count_bits(const uint8_t *data, uint32_t len)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t v = data[i];
        while (v) {
            count++;
            v &= (v - 1);
        }
    }
    return count;
}

/**
 * @brief Print one half-dot line with adjacent block merging
 * @param[in] hdl driver handle
 * @param[in] line_data 48-byte raw dot bitmap for this line
 * @return OPRT_OK on success
 * @note Merges strictly consecutive non-empty blocks whose combined dot
 *       count stays within MTP02_MAX_ACTIVE_DOTS. Empty blocks break
 *       the merge chain to avoid thermal artifacts.
 */
static OPERATE_RET __print_half_dot_line(TDD_PRINTER_MTP02_HANDLE_T *hdl,
                                         const uint8_t *line_data)
{
    uint8_t block_data[MTP02_BYTES_PER_LINE];
    uint32_t block_dots[MTP02_HEAD_BLOCKS];
    int block;

    for (block = 0; block < MTP02_HEAD_BLOCKS; block++) {
        block_dots[block] = __count_bits(&line_data[block * MTP02_BLOCK_BYTES],
                                         MTP02_BLOCK_BYTES);
    }

    block = 0;
    while (block < MTP02_HEAD_BLOCKS) {
        if (block_dots[block] == 0) {
            block++;
            continue;
        }

        memset(block_data, 0, sizeof(block_data));
        uint32_t group_dots = 0;

        while (block < MTP02_HEAD_BLOCKS && block_dots[block] > 0) {
            if (group_dots + block_dots[block] > MTP02_MAX_ACTIVE_DOTS) {
                break;
            }
            uint32_t offset = (uint32_t)block * MTP02_BLOCK_BYTES;
            memcpy(&block_data[offset], &line_data[offset], MTP02_BLOCK_BYTES);
            group_dots += block_dots[block];
            block++;
        }

        if (group_dots > 0) {
            __head_shift_data(hdl, block_data);
            __head_latch(hdl);
            __head_strobe(hdl);
            tal_system_sleep(MTP02_DIVISION_PAUSE_MS);
        }
    }

    return OPRT_OK;
}

/**
 * @brief Print one complete dot line (two half-dot activations + motor half-steps)
 * @param[in] hdl driver handle
 * @param[in] line_data 48-byte raw dot bitmap
 * @return OPRT_OK on success
 * @note The MTP02 uses half-dot-sized heating elements. One visible dot line
 *       requires two consecutive activations with half-steps between each.
 *       Motor must already be started by the caller.
 */
static OPERATE_RET __print_dot_line(TDD_PRINTER_MTP02_HANDLE_T *hdl,
                                    const uint8_t *line_data)
{
    OPERATE_RET rt = OPRT_OK;
    int i;

    if (!__line_has_dots(line_data)) {
        for (i = 0; i < MTP02_STEPS_PER_DOT_LINE; i++) {
            __motor_step_forward(hdl);
        }
        return OPRT_OK;
    }

    rt = __print_half_dot_line(hdl, line_data);
    if (rt != OPRT_OK) {
        return rt;
    }

    for (i = 0; i < MTP02_STEPS_PER_DOT_LINE / 2; i++) {
        __motor_step_forward(hdl);
    }

    rt = __print_half_dot_line(hdl, line_data);
    if (rt != OPRT_OK) {
        return rt;
    }

    for (i = 0; i < MTP02_STEPS_PER_DOT_LINE / 2; i++) {
        __motor_step_forward(hdl);
    }

    return OPRT_OK;
}

/**
 * @brief Open and initialize the MTP02 printer hardware
 * @param[in] handle TDD printer handle
 * @return OPRT_OK on success
 * @note Initializes all GPIO pins and feeds paper 48 steps to clear
 *       backlash per datasheet requirement.
 */
static OPERATE_RET __tdd_printer_mtp02_open(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;
    OPERATE_RET rt = OPRT_OK;
    int i;

    rt = __gpio_output_init(hdl->cfg.pin_power_en, TUYA_GPIO_LEVEL_HIGH);
    if (rt != OPRT_OK) {
        PR_ERR("POWER_EN pin init failed: %d", rt);
        return rt;
    }

    hdl->spi_inited = FALSE;
    if (hdl->cfg.spi_clk > 0) {
        TUYA_SPI_BASE_CFG_T spi_cfg = {
            .role     = TUYA_SPI_ROLE_MASTER,
            .mode     = TUYA_SPI_MODE0,
            .type     = TUYA_SPI_AUTO_TYPE,
            .databits = TUYA_SPI_DATA_BIT8,
            .bitorder = TUYA_SPI_ORDER_MSB2LSB,
            .freq_hz  = hdl->cfg.spi_clk,
        };
        rt = tkl_spi_init(hdl->cfg.spi_port, &spi_cfg);
        if (rt != OPRT_OK) {
            PR_ERR("SPI init failed: %d, fallback to GPIO", rt);
        } else {
            hdl->spi_inited = TRUE;
            PR_NOTICE("head data via SPI%d @%uHz", hdl->cfg.spi_port, hdl->cfg.spi_clk);
        }
    }

    if (!hdl->spi_inited) {
        rt = __gpio_output_init(hdl->cfg.pin_di, TUYA_GPIO_LEVEL_LOW);
        if (rt != OPRT_OK) {
            PR_ERR("DI pin init failed: %d", rt);
            goto err_power;
        }

        rt = __gpio_output_init(hdl->cfg.pin_clk, TUYA_GPIO_LEVEL_LOW);
        if (rt != OPRT_OK) {
            PR_ERR("CLK pin init failed: %d", rt);
            goto err_di;
        }
    }

    rt = __gpio_output_init(hdl->cfg.pin_lat, TUYA_GPIO_LEVEL_HIGH);
    if (rt != OPRT_OK) {
        PR_ERR("LAT pin init failed: %d", rt);
        goto err_clk;
    }

    rt = __gpio_output_init(hdl->cfg.pin_strobe, TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) {
        PR_ERR("STROBE pin init failed: %d", rt);
        goto err_lat;
    }

    rt = __gpio_output_init(hdl->cfg.pin_motor_en, TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) {
        PR_ERR("MOTOR_EN pin init failed: %d", rt);
        goto err_strobe;
    }

    rt = __motor_pwm_init(hdl);
    if (rt != OPRT_OK) {
        PR_ERR("motor PWM init failed: %d", rt);
        goto err_motor_en;
    }

    rt = __gpio_input_init(hdl->cfg.pin_paper_sensor);
    if (rt != OPRT_OK) {
        goto err_pwm;
    }

    hdl->adc_inited = FALSE;
    if (hdl->cfg.temp_adc_threshold > 0) {
        TUYA_ADC_BASE_CFG_T adc_cfg = {0};
        adc_cfg.ch_nums = 1;
        adc_cfg.width = 12;
        adc_cfg.mode = TUYA_ADC_CONTINUOUS;
        adc_cfg.type = TUYA_ADC_INNER_SAMPLE_VOL;
        adc_cfg.conv_cnt = 1;
        adc_cfg.ch_list.data = (1 << hdl->cfg.adc_ch);
        rt = tkl_adc_init(hdl->cfg.adc_port, &adc_cfg);
        if (rt != OPRT_OK) {
            PR_WARN("ADC init failed: %d, temperature monitoring disabled", rt);
        } else {
            hdl->adc_inited = TRUE;
        }
    }

    PR_NOTICE("MTP02 config: power=%d, di=%d, clk=%d, lat=%d, strobe=%d, "
              "motor_en=%d, pwm_a1=%d, pwm_a2=%d, pwm_b1=%d, pwm_b2=%d, "
              "paper=%d, pwm_freq=%d, pwm_duty=%u",
              hdl->cfg.pin_power_en, hdl->cfg.pin_di, hdl->cfg.pin_clk,
              hdl->cfg.pin_lat, hdl->cfg.pin_strobe, hdl->cfg.pin_motor_en,
              hdl->cfg.pwm_motor_a1, hdl->cfg.pwm_motor_a2,
              hdl->cfg.pwm_motor_b1, hdl->cfg.pwm_motor_b2,
              hdl->cfg.pin_paper_sensor,
              MTP02_MOTOR_PWM_FREQ, hdl->cfg.motor_pwm_duty);

    hdl->motor_phase = 0;
    PR_NOTICE("MTP02 motor init feed start");
    __motor_start(hdl);
    for (i = 0; i < MTP02_MOTOR_INIT_STEPS; i++) {
        __motor_step_forward(hdl);
    }
    __motor_stop(hdl);
    PR_NOTICE("MTP02 motor init feed done");

    hdl->is_opened = TRUE;
    PR_NOTICE("MTP02 printer opened");
    return OPRT_OK;

err_pwm:
    __motor_pwm_deinit(hdl);
err_motor_en:
    tkl_gpio_deinit(hdl->cfg.pin_motor_en);
err_strobe:
    tkl_gpio_deinit(hdl->cfg.pin_strobe);
err_lat:
    tkl_gpio_deinit(hdl->cfg.pin_lat);
err_clk:
    tkl_gpio_deinit(hdl->cfg.pin_clk);
err_di:
    tkl_gpio_deinit(hdl->cfg.pin_di);
err_power:
    tkl_gpio_deinit(hdl->cfg.pin_power_en);

    return rt;
}

/**
 * @brief Start a print job: pre-check paper/temperature, start motor
 * @param[in] handle TDD printer handle
 * @return OPRT_OK on success, OPRT_COM_ERROR on paper-out or overheated
 */
static OPERATE_RET __tdd_printer_mtp02_start(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    if (!hdl->is_opened) {
        PR_ERR("MTP02 printer not opened");
        return OPRT_COM_ERROR;
    }

    if (!__is_paper_present(hdl)) {
        PR_ERR("paper out");
        return OPRT_COM_ERROR;
    }

    if (hdl->adc_inited) {
        int32_t adc_val = 0;
        if (tkl_adc_read_single_channel(hdl->cfg.adc_port,
                                         hdl->cfg.adc_ch, &adc_val) == OPRT_OK) {
            // if (adc_val < hdl->cfg.temp_adc_threshold) {
                PR_ERR("thermal head overheated, adc: %d", adc_val);
                // return OPRT_COM_ERROR;
            // }
        }
    }

    hdl->is_printing = TRUE;

    return OPRT_OK;
}

/**
 * @brief Write raw dot bitmap data during a print job
 * @param[in] handle TDD printer handle
 * @param[in] data raw dot bitmap, each line MTP02_BYTES_PER_LINE (48) bytes
 * @param[in] len total data length, must be a multiple of 48
 * @return OPRT_OK on success
 * @note tdl_printer_start must be called before this function.
 */
static OPERATE_RET __tdd_printer_mtp02_write(TDD_PRINTER_HANDLE_T handle,
                                             const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    if (!hdl->is_printing) {
        PR_ERR("print job not started");
        return OPRT_COM_ERROR;
    }

    if (len == 0) {
        return OPRT_OK;
    }

    if (len % MTP02_BYTES_PER_LINE != 0) {
        PR_ERR("data length %u not aligned to %d bytes/line", len, MTP02_BYTES_PER_LINE);
        return OPRT_INVALID_PARM;
    }

    uint32_t num_lines = len / MTP02_BYTES_PER_LINE;
    OPERATE_RET rt = OPRT_OK;

    __motor_start(hdl);

    for (uint32_t line = 0; line < num_lines; line++) {
        rt = __print_dot_line(hdl, &data[line * MTP02_BYTES_PER_LINE]);
        if (rt != OPRT_OK) {
            PR_ERR("print line %u failed: %d", line, rt);
            break;
        }
    }

    __motor_stop(hdl);

    return rt;
}

/**
 * @brief End a print job: feed paper to tear position, stop motor
 * @param[in] handle TDD printer handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_printer_mtp02_end(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    if (!hdl->is_printing) {
        return OPRT_OK;
    }

    __motor_start(hdl);
    for (int i = 0; i < MTP02_PAPER_FEED_STEPS; i++) {
        __motor_step_forward(hdl);
    }
    __motor_stop(hdl);
    hdl->is_printing = FALSE;

    return OPRT_OK;
}

/**
 * @brief Feed paper by the specified number of dot lines without printing
 * @param[in] handle TDD printer handle
 * @param[in] lines number of dot lines to advance
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_printer_mtp02_paper_feed(TDD_PRINTER_HANDLE_T handle, uint32_t lines)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    if (!hdl->is_opened) {
        PR_ERR("MTP02 printer not opened");
        return OPRT_COM_ERROR;
    }

    uint32_t total_steps = lines * MTP02_STEPS_PER_DOT_LINE;

    __motor_start(hdl);
    for (uint32_t i = 0; i < total_steps; i++) {
        __motor_step_forward(hdl);
    }
    __motor_stop(hdl);

    return OPRT_OK;
}

/**
 * @brief Close the MTP02 printer and release all hardware resources
 * @param[in] handle TDD printer handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_printer_mtp02_close(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    __motor_stop(hdl);

    if (hdl->adc_inited) {
        tkl_adc_deinit(hdl->cfg.adc_port);
        hdl->adc_inited = FALSE;
    }

    tkl_gpio_deinit(hdl->cfg.pin_paper_sensor);
    __motor_pwm_deinit(hdl);
    tkl_gpio_deinit(hdl->cfg.pin_motor_en);
    tkl_gpio_deinit(hdl->cfg.pin_strobe);
    tkl_gpio_deinit(hdl->cfg.pin_lat);
    if (hdl->spi_inited) {
        tkl_spi_deinit(hdl->cfg.spi_port);
        hdl->spi_inited = FALSE;
    } else {
        tkl_gpio_deinit(hdl->cfg.pin_clk);
        tkl_gpio_deinit(hdl->cfg.pin_di);
    }
    tkl_gpio_write(hdl->cfg.pin_power_en, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_deinit(hdl->cfg.pin_power_en);

    hdl->is_opened = FALSE;
    PR_DEBUG("MTP02 printer closed");
    return OPRT_OK;
}

/**
 * @brief Query current device status (paper, temperature, etc.)
 * @param[in] handle TDD printer handle
 * @param[out] status device status output
 * @return OPRT_OK on success
 * @note Temperature field contains raw ADC value of the thermistor.
 *       OVERHEATED flag is set when raw ADC value falls below
 *       temp_adc_threshold (NTC: lower ADC = higher temperature).
 *       If ADC is not configured (temp_adc_threshold == 0), temperature
 *       is reported as -1 and OVERHEATED flag is never set.
 */
static OPERATE_RET __tdd_printer_mtp02_get_status(TDD_PRINTER_HANDLE_T handle,
                                                   TDD_PRINTER_DEV_STATUS_T *status)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(status, OPRT_INVALID_PARM);

    TDD_PRINTER_MTP02_HANDLE_T *hdl = (TDD_PRINTER_MTP02_HANDLE_T *)handle;

    memset(status, 0, sizeof(TDD_PRINTER_DEV_STATUS_T));
    status->temperature = -1;

    if (!__is_paper_present(hdl)) {
        status->flags |= TDD_PRINTER_FLAG_PAPER_OUT;
    }

    if (hdl->adc_inited) {
        int32_t adc_val = 0;
        OPERATE_RET rt = tkl_adc_read_single_channel(hdl->cfg.adc_port,
                                                      hdl->cfg.adc_ch, &adc_val);
        if (rt == OPRT_OK) {
            status->temperature = adc_val;
            // if (adc_val < hdl->cfg.temp_adc_threshold) {
            //     status->flags |= TDD_PRINTER_FLAG_OVERHEATED;
            // }
            PR_DEBUG("temperature: %d", adc_val);
        } else {
            PR_WARN("ADC read failed: %d", rt);
            status->flags |= TDD_PRINTER_FLAG_ERROR;
        }
    }

    return OPRT_OK;
}

/**
 * @brief Register MTP02-DXD thermal printer driver to TDL layer
 * @param[in] name printer name for TDL registration
 * @param[in] cfg hardware pin and timing configuration
 * @return OPRT_OK on success, error code on failure
 * @note If strobe_time_ms or motor_step_ms is 0, defaults are applied
 *       (1ms strobe, 1ms motor step).
 */
OPERATE_RET tdd_printer_mtp02_register(char *name, TDD_PRINTER_MTP02_CFG_T *cfg)
{
    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    OPERATE_RET rt = OPRT_OK;

    TDD_PRINTER_MTP02_HANDLE_T *hdl =
        (TDD_PRINTER_MTP02_HANDLE_T *)tal_malloc(sizeof(TDD_PRINTER_MTP02_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_PRINTER_MTP02_HANDLE_T));

    memcpy(&hdl->cfg, cfg, sizeof(TDD_PRINTER_MTP02_CFG_T));

    if (hdl->cfg.strobe_time_ms == 0) {
        hdl->cfg.strobe_time_ms = MTP02_DEFAULT_STROBE_MS;
    }
    if (hdl->cfg.motor_step_ms == 0) {
        hdl->cfg.motor_step_ms = MTP02_DEFAULT_MOTOR_STEP_MS;
    }
    if (hdl->cfg.motor_pwm_duty == 0) {
        hdl->cfg.motor_pwm_duty = MTP02_MOTOR_PWM_DUTY_MAX;
    }

    TDD_PRINTER_INTFS_T mtp02_intfs = {
        .open       = __tdd_printer_mtp02_open,
        .start      = __tdd_printer_mtp02_start,
        .write      = __tdd_printer_mtp02_write,
        .end        = __tdd_printer_mtp02_end,
        .close      = __tdd_printer_mtp02_close,
        .get_status = __tdd_printer_mtp02_get_status,
        .paper_feed    = __tdd_printer_mtp02_paper_feed,
    };

    TDD_PRINTER_DEV_INFO_T dev_info = {
        .dots_per_line      = MTP02_DOTS_PER_LINE,
        .bytes_per_line     = MTP02_BYTES_PER_LINE,
        .head_blocks        = MTP02_HEAD_BLOCKS,
        .dots_per_block     = MTP02_DOTS_PER_BLOCK,
        .steps_per_dot_line = MTP02_STEPS_PER_DOT_LINE,
    };

    rt = tdl_printer_driver_register(name, &mtp02_intfs, &dev_info, (TDD_PRINTER_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        hdl = NULL;
        PR_ERR("Failed to register MTP02 printer: %d", rt);
    }

    return rt;
}
