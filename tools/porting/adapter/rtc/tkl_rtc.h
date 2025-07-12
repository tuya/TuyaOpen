/**
 * @file tkl_rtc.h
 * @brief Common process - adapter the rtc api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_RTC_H__
#define __TKL_RTC_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief rtc init
 *
 * @param[in] none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rtc_init(void);

/**
 * @brief rtc deinit
 * @param[in] none
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rtc_deinit(void);

/**
 * @brief rtc time set
 *
 * @param[in] time_sec: rtc time seconds
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rtc_time_set(TIME_T time_sec);

/**
 * @brief rtc time get
 *
 * @param[in] time_sec:rtc time seconds
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rtc_time_get(TIME_T *time_sec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
