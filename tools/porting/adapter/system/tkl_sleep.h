/**
 * @file tkl_sleep.h
 * @brief Common process - adapter the sleep manage api
 * @version 0.1
 * @date 2021-08-18
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_SLEEP_H__
#define __TKL_SLEEP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sleep callback register
 *
 * @param[in] sleep_cb:  sleep callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cpu_sleep_callback_register(TUYA_SLEEP_CB_T *sleep_cb);

/**
 * @brief allow to sleep
 *
 * @param[in] none
 *
 * @return none
 */
void tkl_cpu_allow_sleep(void);

/**
 * @brief force wakeup
 *
 * @param[in] none
 *
 * @return none
 */
void tkl_cpu_force_wakeup(void);

/**
 * @brief Set the low power mode of CPU
 *
 * @param[in] enable: enable switch
 * @param[in] mode:   cpu sleep mode
 *
 * @note This API is used for setting the low power mode of CPU.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cpu_sleep_mode_set(BOOL_T enable, TUYA_CPU_SLEEP_MODE_E mode);

#ifdef __cplusplus
}
#endif

#endif
