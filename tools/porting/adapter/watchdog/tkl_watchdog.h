/**
 * @file tkl_watchdog.h
 * @brief Common process - adapter the watchdog api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_WATCHDOG_H__
#define __TKL_WATCHDOG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief watchdog init
 *
 * @param[in] cfg: watchdog config
 *
 * @return 0, init error; >0 : the actually watchdog interval
 */
uint32_t tkl_watchdog_init(TUYA_WDOG_BASE_CFG_T *cfg);

/**
 * @brief watchdog deinit
 *
 * @param[in] none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_watchdog_deinit(void);

/**
 * @brief refresh watch dog
 *
 * @param[in] none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_watchdog_refresh(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
