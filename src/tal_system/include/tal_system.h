/**
 * @file tal_system.h
 * @brief Provides system-level utilities and operations for Tuya IoT
 * applications.
 *
 * This header file defines the interface for system-level operations in Tuya
 * IoT applications, including critical section management, system sleep and
 * reset functionalities, tick count and millisecond timing, random number
 * generation, system reset reason querying, delay operations, and CPU
 * information retrieval. These utilities are essential for managing the
 * application's execution environment, ensuring time-sensitive operations are
 * handled correctly, and facilitating efficient power management and system
 * diagnostics.
 *
 * The API abstracts the underlying hardware and operating system details,
 * offering a portable and consistent interface for application development
 * across different Tuya IoT devices.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_SYSTEM_H__
#define __TAL_SYSTEM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
/**
 * @brief enter critical macro
 */
#define TAL_ENTER_CRITICAL()                                                                                           \
    uint32_t __irq_mask;                                                                                               \
    __irq_mask = tal_system_enter_critical()

/**
 * @brief exit critical macro
 */
#define TAL_EXIT_CRITICAL() tal_system_exit_critical(__irq_mask)

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
 * @brief system enter critical
 *
 * @param[in]   none
 * @return  irq mask
 */
uint32_t tal_system_enter_critical(void);

/**
 * @brief system exit critical
 *
 * @param[in]   irq_mask: irq mask
 * @return  none
 */
void tal_system_exit_critical(uint32_t irq_mask);

/**
 * @brief This API is used for system sleep.
 *
 * @param[in] time_ms: time in MS
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_system_sleep(uint32_t time_ms);

/**
 * @brief tal_system_reset
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_system_reset(void);

/**
 * @brief tal_system_get_tick_count
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
SYS_TICK_T tal_system_get_tick_count(void);

/**
 * @brief tal_system_get_millisecond
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
SYS_TIME_T tal_system_get_millisecond(void);

/**
 * @brief Get system random data
 *
 * @param[in] range: random from 0  to range
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tal_system_get_random(uint32_t range);

/**
 * @brief tal_system_get_reset_reason
 *
 * @param[in] describe: point to reset reason describe
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
TUYA_RESET_REASON_E tal_system_get_reset_reason(char **describe);

/**
 * @brief This API is used for system delay.
 *
 * @param[in] time_ms: time in MS
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_system_delay(uint32_t time_ms);

/**
 * @brief This API is used for system cpu info get.
 *
 * @param[in] **cpu_ary: info of cpus
 * @param[in] *cpu_cnt: num of cpu
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_system_get_cpu_info(TUYA_CPU_INFO_T **cpu_ary, int32_t *cpu_cnt);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_SYSTEM_H__ */
