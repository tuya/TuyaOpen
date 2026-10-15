/**
 * @file tuya_pm_internal.h
 * @brief Internal contract between the scheduler (tuya_pm.c) and the built-in scheme
 *        library (tuya_pm_schemes.c). NOT a public API.
 *
 * The scheduler asks the scheme library for its built-in scheme table and a one-time
 * backend init. All ecosystem-specific dependencies (lpmgr / tal_wifi / ble_mgr /
 * tdl_power) live in tuya_pm_schemes.c so the scheduler core stays generic.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_PM_INTERNAL_H__
#define __TUYA_PM_INTERNAL_H__

#include "tuya_pm_scheme.h" // TUYA_PM_SCHEME_T

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Get the read-only table of built-in schemes
 *        (ACTIVE / CEC_T20 / ULP_ONLINE / DEEPSLEEP), indexed by TUYA_PM_SCHEME_E id.
 *
 * @param[out] cnt Number of built-in schemes.
 *
 * @return Pointer to the built-in scheme table (static storage).
 */
const TUYA_PM_SCHEME_T *tuya_pm_get_preset_schemes(uint8_t *cnt);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_PM_INTERNAL_H__ */
