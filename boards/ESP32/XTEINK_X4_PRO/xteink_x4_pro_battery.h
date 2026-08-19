/**
 * @file xteink_x4_pro_battery.h
 * @brief Xteink X4 Pro CW2017 battery fuel gauge driver (tkl_i2c only).
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_PRO_BATTERY_H__
#define __XTEINK_X4_PRO_BATTERY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CW2017 fuel gauge and verify/upload the BATINFO profile.
 * @return OPRT_OK on success, OPRT_COM_ERROR if the gauge does not ACK.
 */
OPERATE_RET xteink_x4_pro_battery_init(void);

/**
 * @brief Read battery state of charge and cell voltage.
 * @param[out] voltage_mv cell voltage in millivolts, may be NULL.
 * @param[out] percentage 0..100, may be NULL.
 * @return OPRT_OK on success.
 * @note Percentage comes from FreeInk's 1S Li-ion rest-voltage notch curve
 *       (10 % steps, midpoint rounding, 8 mV hysteresis) applied to VCELL:
 *       a full cell reads 100 % straight off the charger, 0 % below 3.45 V.
 *       The CW2017's raw SoC register is logged at init as a cross-check.
 */
OPERATE_RET xteink_x4_pro_battery_read(uint32_t *voltage_mv, uint8_t *percentage);

/**
 * @brief Charge state as estimated by the driver's VCELL-slope estimator.
 */
typedef enum {
    X4PRO_CHARGE_IDLE = 0,    /**< not charging (on battery) */
    X4PRO_CHARGE_CHARGING,    /**< cell voltage climbing under the charger */
    X4PRO_CHARGE_FULL,        /**< pinned at CV top-off voltage */
} X4PRO_CHARGE_STATE_E;

/**
 * @brief Charge-state change callback. Runs in the battery estimator task
 *        context — keep it short and do not touch LVGL directly.
 * @param[in] from previous state.
 * @param[in] to   new state.
 */
typedef void (*X4PRO_CHARGE_STATE_CB)(X4PRO_CHARGE_STATE_E from, X4PRO_CHARGE_STATE_E to);

/**
 * @brief Get the last estimated charge state.
 * @param[out] state non-NULL receives the state.
 * @return OPRT_OK on success.
 * @note The X4 Pro exposes no charger IC or VBUS sense pin (confirmed by
 *       FreeInk hardware bring-up), so the state is estimated from VCELL:
 *       sustained climb = CHARGING, top-off plateau near 4.20 V = FULL,
 *       relaxation = IDLE. It is always IDLE until the first climb is seen.
 */
OPERATE_RET xteink_x4_pro_battery_get_charge_state(X4PRO_CHARGE_STATE_E *state);

/**
 * @brief Register the charge-state change callback (single slot, replaces
 *        any previous registration; NULL unregisters).
 * @param[in] cb callback or NULL.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_battery_on_charge_state(X4PRO_CHARGE_STATE_CB cb);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_PRO_BATTERY_H__ */
