/**
 * @file board_charge_detect_api.h
 * @author Tuya Inc.
 * @brief Header file for charge detection / battery measurement API for ZECTRIX_NOTE4_TY.
 *
 * Charger: IP2332. The board exposes two open-drain status lines and a battery
 * voltage divider:
 *   - CHRG_L  -> P21  (charging indication)
 *   - STDBY_H -> P22  (charge-done / standby indication)
 *   - ADC_BAT -> P25  (battery voltage divider tap)
 *
 * Battery divider: VBAT -> 3.09M -> (tap, ADC_BAT) -> 1M -> GND
 *   ratio = (3.09M + 1M) / 1M = 4.09
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_CHARGE_DETECT_API_H__
#define __BOARD_CHARGE_DETECT_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief GPIO pin used for charge-state detection input (CHRG_L, active low)
 */
#define BOARD_CHARGE_DETECT_PIN TUYA_GPIO_NUM_21

/**
 * @brief GPIO pin reporting charge-done / standby (STDBY_H)
 */
#define BOARD_CHARGE_STDBY_PIN TUYA_GPIO_NUM_22

/**
 * @brief Charge-enable / NTC control output (CHRG_EN, V1.1 new).
 *
 * V1.1 schematic added CHRG_EN on P20 (the pin V1.0 used for LED_G), routed
 * through Q13/R73 into the charger / NTC control path. The active level and the
 * required power-on / sleep default state have NOT been verified on hardware yet,
 * so this pin is intentionally left un-inited and undriven for now — do NOT drive
 * it until the polarity is measured, otherwise charging may be disabled by mistake.
 * Kept here only as a documented placeholder for the future charge-enable API.
 */
#define BOARD_CHARGE_EN_PIN TUYA_GPIO_NUM_20

/**
 * @brief Charger status line active levels.
 *
 * Polarity mirrors the ESP32-S3 zectrix board (board zectrix-s3-epaper-4.2):
 *   CHARGE_DETECT_CHARGING_LEVEL = 0  -> CHRG asserted (charging) reads LOW
 *   full_high = gpio_get_level() == 1 -> STDBY asserted (charge done) reads HIGH
 * The CHRG line is an IP2332 open-drain output (pulled low while charging), so we
 * enable an internal pull-up; STDBY is treated active-high with a pull-down so an
 * idle/unplugged charger reads as "not asserted".
 */
#define BOARD_CHARGE_DETECT_ACTIVE_LEVEL TUYA_GPIO_LEVEL_LOW  // CHRG: low = charging
#define BOARD_CHARGE_FULL_ACTIVE_LEVEL   TUYA_GPIO_LEVEL_HIGH // STDBY: high = charge done

/**
 * @brief ADC pin/channel used for battery voltage reading (ADC_BAT -> P25)
 *
 * NOTE: BOARD_BATTERY_ADC_CH is the T5 ADC *channel*, not the GPIO number. The
 * platform map (tkl_adc.c / soc/adc_map.h) wires {ADC_1, GPIO_25}, so P25 reads
 * out on ADC channel 1. tkl_adc_init() also parses ch_list as a bitmask over
 * channels 0..15, so a >15 value selects nothing and the read silently returns 0.
 */
#define BOARD_BATTERY_ADC_PIN TUYA_GPIO_NUM_25
#define BOARD_BATTERY_ADC_NUM TUYA_ADC_NUM_0
#define BOARD_BATTERY_ADC_CH  1 // ADC_1 <-> GPIO_25 (P25)

/**
 * @brief Samples averaged per battery read (the high-Z tap reading is noisy).
 */
#define BOARD_BATTERY_ADC_SAMPLES 16

/**
 * @brief Raw-count -> tap-voltage calibration for tkl_adc_read_single_channel().
 *
 * tkl_adc_read_single_channel() returns a raw 12-bit count (tkl_adc_read_voltage()
 * is unusable here: it mis-declares the float-returning bk_adc_data_calculate() as
 * UINT16 and reads the wrong register). The T5 SARADC is linear; the vendor model is
 * V = (raw - LOW)/SPAN + 1.0 V. Two-point calibrated against metered battery
 * terminals across the pack range (4.15 V / 3.95 V / 3.45 V actual all match the
 * reported value to within a few mV). SPAN is the slope (counts/Volt), LOW the
 * offset. Re-fit both from fresh (actual, reported) pairs if a new pack/board drifts.
 */
#define BOARD_BATTERY_ADC_CAL_LOW  2469 // raw count at ~1.0 V tap
#define BOARD_BATTERY_ADC_CAL_SPAN 2429 // raw counts per 1 V

/**
 * @brief Battery voltage divider ratio
 * Circuit: VBAT -> 3.09MΩ -> (tap) -> 1MΩ -> GND, tap -> 100R -> ADC input
 *   V_ADC = VBAT * 1M / (3.09M + 1M) = VBAT * 0.2445
 *   VBAT  = V_ADC * (3.09M + 1M) / 1M = V_ADC * 4.09
 */
#define BOARD_BATTERY_DIVIDER_RATIO 4.09f

/**
 * @brief Battery voltage range (LiPo)
 */
#define BOARD_BATTERY_VOLTAGE_MIN   3.0f // Minimum voltage (0%)
#define BOARD_BATTERY_VOLTAGE_MAX   4.2f // Maximum voltage (100%)
#define BOARD_BATTERY_VOLTAGE_RANGE 1.2f // (4.2 - 3.0)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Charge state enumeration
 */
typedef enum {
    BOARD_CHARGE_STATE_UNPLUGGED = 0, /**< Discharging (no external power) */
    BOARD_CHARGE_STATE_PLUGGED   = 1, /**< Charging in progress */
    BOARD_CHARGE_STATE_FULL      = 2  /**< Plugged in, charge complete (standby) */
} BOARD_CHARGE_STATE_E;

/**
 * @brief Charge detect callback function type
 */
typedef void (*BOARD_CHARGE_DETECT_CB)(BOARD_CHARGE_STATE_E state, void *arg);

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET board_charge_detect_init(void);
OPERATE_RET board_charge_detect_register_callback(BOARD_CHARGE_DETECT_CB cb, void *arg);
OPERATE_RET board_charge_detect_unregister_callback(void);
OPERATE_RET board_charge_detect_get_state(BOARD_CHARGE_STATE_E *state);
OPERATE_RET board_charge_detect_enable(void);
OPERATE_RET board_charge_detect_disable(void);
OPERATE_RET board_charge_detect_deinit(void);

OPERATE_RET board_battery_adc_init(void);
OPERATE_RET board_battery_read_voltage(uint32_t *voltage_mv);
OPERATE_RET board_battery_read_percentage(uint8_t *percentage);
OPERATE_RET board_battery_read(uint32_t *voltage_mv, uint8_t *percentage);
OPERATE_RET board_battery_adc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CHARGE_DETECT_API_H__ */
