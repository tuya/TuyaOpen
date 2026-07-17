/**
 * @file tal_sleep.h
 * @brief Provides CPU sleep management functions for Tuya IoT applications.
 *
 * This header file defines the interface for managing CPU sleep states in Tuya
 * IoT applications, including functions for registering sleep callbacks,
 * allowing or preventing the CPU from entering sleep mode, forcibly waking up
 * the CPU, and managing low power modes. These functions are designed to help
 * developers efficiently manage power consumption in IoT devices, extending
 * battery life and reducing energy costs.
 *
 * The API supports different levels of sleep and low power modes, providing
 * flexibility in balancing power consumption with the responsiveness and
 * performance requirements of the application. This file is part of the Tuya
 * IoT Development Platform and is intended for use in Tuya-based applications.
 *
 * @note This file is subject to the platform's license and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_SLEEP_H__
#define __TAL_SLEEP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/

/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief sleep callback register
 *
 * @param[in] sleep_cb:  sleep callback
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_cpu_sleep_callback_register(TUYA_SLEEP_CB_T *sleep_cb);

/**
 * @brief allow to sleep
 *
 * @param[in] none
 *
 * @return none
 */
void tal_cpu_allow_sleep(void);

/**
 * @brief force wakeup
 *
 * @param[in] none
 *
 * @return none
 */
void tal_cpu_force_wakeup(void);

/**
 * @brief set cpu lowpower mode
 *
 * @param[in] lp_enable
 *
 * @return none
 */
void tal_cpu_set_lp_mode(BOOL_T lp_enable);

/**
 * @brief get cpu lowpower mode
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
BOOL_T tal_cpu_get_lp_mode(void);

/**
 * @brief cpu lowpower enable
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_cpu_lp_enable(void);

/**
 * @brief cpu lowpower disable
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_cpu_lp_disable(void);

/**
 * @brief wakelock ID enum for CPU sleep management
 */
typedef enum {
    TAL_WAKELOCK_SDK_BASE = 0,
    TAL_WAKELOCK_SDK_WIFI = 1,
    TAL_WAKELOCK_SDK_BT = 2,
    TAL_WAKELOCK_SDK_KEYSCAN = 3,
    // for application
    TAL_WAKELOCK_APP_BASE = 16,
    TAL_WAKELOCK_MAX = 31,
} TAL_WAKELOCK_ID_E;

/**
 * @brief Acquire a wakelock to prevent CPU from sleeping.
 *        Uses bitmap-based management; same ID won't stack.
 *
 * @param[in] id: wakelock ID
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tal_cpu_acquire_wakelock(TAL_WAKELOCK_ID_E id);

/**
 * @brief Release a wakelock to allow CPU to sleep.
 *        When all wakelocks released (bitmap == 0), enables CPU sleep.
 *
 * @param[in] id: wakelock ID
 *
 * @return OPRT_OK on success. Others on error.
 */
OPERATE_RET tal_cpu_release_wakelock(TAL_WAKELOCK_ID_E id);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_SLEEP_H__ */
