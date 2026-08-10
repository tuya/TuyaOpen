/**
 * @file tdl_power_manage.h
 * @brief Application-facing power interface. One board = one power device (one handle);
 *        power_domain / battery / charger are operation groups on it.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_POWER_MANAGE_H__
#define __TDL_POWER_MANAGE_H__

#include "tuya_cloud_types.h"
#include "tdl_power_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Find the power device by name.
 *
 * @param[in] name The power device name (convention "power").
 *
 * @return Handle to the power device if registered, otherwise NULL.
 */
TDL_POWER_HANDLE tdl_power_find(const char *name);

/* ---- power_domain (on/off only; voltage fixed at registration) ---- */

/**
 * @brief Switch one or more power domains on/off.
 *
 * @param[in] h           Handle to the power device.
 * @param[in] domain_mask OR of TDL_POWER_DOMAIN_E roles; roles absent on this board are skipped.
 * @param[in] on          TRUE to enable, FALSE to disable.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_domain_set(TDL_POWER_HANDLE h, uint32_t domain_mask, BOOL_T on);

/**
 * @brief Query the state of a single power domain.
 *
 * @param[in]  h      Handle to the power device.
 * @param[in]  domain A single TDL_POWER_DOMAIN_E role (not a mask).
 * @param[out] on     TRUE if the domain is enabled.
 *
 * @return OPRT_OK on success, or OPRT_NOT_SUPPORTED if the domain is absent.
 */
OPERATE_RET tdl_power_domain_get(TDL_POWER_HANDLE h, TDL_POWER_DOMAIN_E domain, BOOL_T *on);

/* ---- battery ---- */

/**
 * @brief Read the raw battery voltage.
 *
 * @param[in]  h  Handle to the power device.
 * @param[out] mv Battery voltage in millivolts.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_battery_get_voltage(TDL_POWER_HANDLE h, uint32_t *mv);

/**
 * @brief Read the battery percentage.
 *
 * @param[in]  h   Handle to the power device.
 * @param[out] pct Battery percentage (0-100); backend gauge if present, else derived from voltage.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_battery_get_percent(TDL_POWER_HANDLE h, uint8_t *pct);

/**
 * @brief Read the board-declared device info (battery landmarks, ...).
 *
 * @param[in]  h    Handle to the power device.
 * @param[out] info Device info.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_get_info(TDL_POWER_HANDLE h, TDL_POWER_INFO_T *info);

/* ---- charger ---- */

/**
 * @brief Read the current charge state.
 *
 * @param[in]  h  Handle to the power device.
 * @param[out] st Current charge state.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_charger_get_state(TDL_POWER_HANDLE h, TDL_CHG_STATE_E *st);

/**
 * @brief Register a callback invoked on charge state changes.
 *
 * @param[in] h   Handle to the power device.
 * @param[in] cb  Callback function.
 * @param[in] arg User argument passed to the callback.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_charger_on_event(TDL_POWER_HANDLE h, TDL_CHG_EVENT_CB cb, void *arg);

/* ---- deep sleep ---- */

/**
 * @brief Enter real deep sleep: arm the board-declared GPIO wakeup sources (plus an
 *        optional one-shot timer wakeup) and power the CPU down. Waking reboots the
 *        device. Does not return on success; returns only on failure.
 *
 * @param[in] h             Handle to the power device.
 * @param[in] timer_wake_ms Optional one-shot timer wakeup in ms (0 = none). At least
 *                          one wake source is required: this timer, or a GPIO wake
 *                          source declared by the backend; otherwise the call is
 *                          refused (would strand the device asleep).
 *
 * @return OPRT_NOT_SUPPORTED if the platform lacks TKL wakeup (ENABLE_WAKEUP off) or
 *         no backend implements deep sleep; OPRT_INVALID_PARM if no wake source;
 *         otherwise does not return on success (waking reboots).
 */
OPERATE_RET tdl_power_enter_deepsleep(TDL_POWER_HANDLE h, uint32_t timer_wake_ms);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_POWER_MANAGE_H__ */
