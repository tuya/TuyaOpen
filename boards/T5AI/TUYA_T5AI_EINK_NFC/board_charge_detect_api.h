/**
 * @file board_charge_detect_api.h
 * @author Tuya Inc.
 * @brief Header file for charge detection API for TUYA_T5AI_EINK_NFC board.
 *
 * This module provides APIs to detect charging state changes via GPIO interrupt.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
 * @brief GPIO pin used for charge detection input
 */
#define BOARD_CHARGE_DETECT_PIN TUYA_GPIO_NUM_12

/**
 * @brief ADC pin/channel used for battery voltage reading
 */
#define BOARD_BATTERY_ADC_PIN TUYA_GPIO_NUM_8
#define BOARD_BATTERY_ADC_NUM TUYA_ADC_NUM_0
#define BOARD_BATTERY_ADC_CH  8 // ADC channel for P8 (adjust based on hardware)

/**
 * @brief Battery voltage divider configuration
 * Circuit: VBAT -> 2MΩ -> (junction) -> 510KΩ -> GND
 *          Junction -> 100R -> ADC input
 *
 * Voltage divider ratio: V_ADC = VBAT * (510K) / (2M + 510K)
 * V_ADC = VBAT * 510000 / 2510000 = VBAT * 0.2032
 *
 * To calculate VBAT: VBAT = V_ADC / 0.2032 = V_ADC * 4.922
 * Note: 100R resistor is a small series resistor (protection/filtering) and doesn't affect divider ratio
 */
#define BOARD_BATTERY_DIVIDER_RATIO 4.922f // (2M + 510K) / 510K = 2510000 / 510000

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
    BOARD_CHARGE_STATE_UNPLUGGED = 0, /**< Charger unplugged */
    BOARD_CHARGE_STATE_PLUGGED   = 1  /**< Charger plugged in */
} BOARD_CHARGE_STATE_E;

/**
 * @brief Charge detect callback function type
 *
 * @param[in] state Current charge state (plugged/unplugged)
 * @param[in] arg User-provided argument
 */
typedef void (*BOARD_CHARGE_DETECT_CB)(BOARD_CHARGE_STATE_E state, void *arg);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the charge detect GPIO and interrupt
 *
 * This function initializes the GPIO pin for charge detection and sets up
 * interrupt handling for both rising and falling edges.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_init(void);

/**
 * @brief Register a callback function for charge state changes
 *
 * @param[in] cb Callback function to be called on charge state change
 * @param[in] arg User-provided argument passed to callback
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_register_callback(BOARD_CHARGE_DETECT_CB cb, void *arg);

/**
 * @brief Unregister the charge detect callback
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_unregister_callback(void);

/**
 * @brief Get the current charge state
 *
 * @param[out] state Pointer to store the current charge state
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_get_state(BOARD_CHARGE_STATE_E *state);

/**
 * @brief Enable charge detect interrupt
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_enable(void);

/**
 * @brief Disable charge detect interrupt
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_disable(void);

/**
 * @brief Deinitialize the charge detect GPIO and interrupt
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_charge_detect_deinit(void);

/**
 * @brief Initialize battery voltage ADC reading
 *
 * This function initializes the ADC for reading battery voltage.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_battery_adc_init(void);

/**
 * @brief Read battery voltage
 *
 * @param[out] voltage_mv Pointer to store battery voltage in millivolts
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_battery_read_voltage(uint32_t *voltage_mv);

/**
 * @brief Read battery percentage
 *
 * @param[out] percentage Pointer to store battery percentage (0-100)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_battery_read_percentage(uint8_t *percentage);

/**
 * @brief Read both battery voltage and percentage
 *
 * @param[out] voltage_mv Pointer to store battery voltage in millivolts
 * @param[out] percentage Pointer to store battery percentage (0-100)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_battery_read(uint32_t *voltage_mv, uint8_t *percentage);

/**
 * @brief Deinitialize battery ADC
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_battery_adc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CHARGE_DETECT_API_H__ */
