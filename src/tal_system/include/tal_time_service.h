/**
 * @file tal_time_service.h
 * @brief Provides time management services for Tuya IoT applications.
 *
 * This header file defines the interface for time management services in Tuya
 * IoT applications, including POSIX time operations, time zone and summer time
 * adjustments, time synchronization checks, and system time retrieval in
 * various formats. It facilitates the handling of time-related operations,
 * ensuring that IoT devices operate with accurate time settings, which is
 * crucial for scheduling tasks, logging, and time-stamping data. The API
 * abstracts the complexity of time management across different platforms and
 * regions, providing a unified and simplified interface for developers.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TAL_TIME_SERVICE_H__
#define __TAL_TIME_SERVICE_H__

#include "tuya_cloud_types.h"
#include "tal_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief posix time
 *
 */
typedef struct {
    int tm_sec;  /* seconds [0-59] */
    int tm_min;  /* minutes [0-59] */
    int tm_hour; /* hours [0-23] */
    int tm_mday; /* day of the month [1-31] */
    int tm_mon;  /* month [0-11] */
    int tm_year; /* year. The number of years since 1900 */
    int tm_wday; /* day of the week [0-6] 0-Sunday...6-Saturday */
} POSIX_TM_S;

/**
 * @brief summer time zone
 *
 */
typedef struct {
    TIME_T posix_min; // summer time start
    TIME_T posix_max; // summer time end
} SUM_ZONE_S;

/**
 * @brief number of time zone table
 *
 */
#define SUM_ZONE_TAB_LMT 6

/**
 * @brief sum zone info
 *
 */
typedef struct {
    uint32_t cnt;                      // sum zone count
    SUM_ZONE_S zone[SUM_ZONE_TAB_LMT]; // zone info
} SUM_ZONE_TBL_S;

/**
 * @brief time-management module initialization
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_service_init(void);

/**
 * @brief change posix time to TIME_T, redefine the std C func mktime
 *
 * @param[in] tm the time in posix time format
 * @return the time in TIME_T format
 */
TIME_T tal_time_mktime(const POSIX_TM_S *tm);

/**
 * @brief change TIME_T to posix time, redefine the std C func gmtime_r
 *
 * @param[in] tm the time in TIME_T format
 * @param[out] result the time in posix time format
 * @return the time in posix time format
 */
POSIX_TM_S *tal_time_gmtime_r(const TIME_T *tm, POSIX_TM_S *result);

/**
 * @brief check IoTOS time synchronize status
 *
 * @return OPRT_OK on synchronized. Others on not
 */
OPERATE_RET tal_time_check_time_sync(void);

/**
 * @brief check IoTOS time zone synchronized status
 *
 * @return OPRT_OK on synchronized. Others on not
 */
OPERATE_RET tal_time_check_time_zone_sync(void);

/**
 * @brief set IoTOS UTC time
 *
 * @param[in] tm the new time in posix time format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set(const POSIX_TM_S *tm);

/**
 * @brief set IoTOS UTC time in time_t format
 *
 * @param[in] time the new time in time_t format
 * @param[in] update_source the source of time (0:rtc 1:cloud 2:other)
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_posix(const TIME_T time, const int update_source);

/**
 * @brief get IoTOS UTC time in posix time format
 *
 * @param[out] tm the IoTOS UTC time in posix time format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get(POSIX_TM_S *tm);

/**
 * @brief get IoTOS UTC time in TIME_T format
 *
 * @return the current second time in TIME_T format
 */
TIME_T tal_time_get_posix(void);

/**
 * @brief get IoTOS UTC time in SYS_TICK_T format
 *
 * @return the current micro-second time in SYS_TICK_T format
 */
SYS_TICK_T tal_time_get_posix_ms(void);

/**
 * @brief get IoTOS last synchronized UTC time in TIME_T format
 *
 * @return the time in TIME_T format
 */
TIME_T tal_time_get_cur_posix(void);

/**
 * @brief get IoTOS UTC time, both second and micro-second
 *
 * @param[out] pSecTime the current time in second
 * @param[out] pMsTime the current time in micro-second
 * @return void
 */
void tal_time_get_system_time(TIME_S *pSecTime, TIME_MS *pMsTime);

/**
 * @brief set IoTOS time zone in "+/-hh:mm" format
 *
 * @param[in] time_zone the new time zone in "+/-hh:mm" format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_time_zone(const char *time_zone);

/**
 * @brief set IoTOS time zone in second format
 *
 * @param[in] time_zone the new time zone in second format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_time_zone_seconds(int time_zone_sec);

/**
 * @brief get IoTOS time zone in second format
 *
 * @param[out] time_zone the current time zone in second format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_time_zone_seconds(int *time_zone);

/**
 * @brief set IoTOS summer time zone
 *
 * @param[in] zone the summer time zone table
 * @param[in] cnt the summer time counts
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_time_set_sum_zone_tbl(const SUM_ZONE_S *zone, const uint32_t cnt);

/**
 * @brief get IoTOS UTC summer time in posix time format
 *
 * @param[out] tm the summer time in posix format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_sum_time(POSIX_TM_S *tm);

/**
 * @brief get IoTOS UTC summer time int TIME_T format
 *
 * @return the summer time in TIME_T format
 */
TIME_T tal_time_get_sum_time_posix(void);

/**
 * @brief get IoTOS UTC summer time int TIME_T format
 *
 * @return the summer time in TIME_T format
 */
BOOL_T tal_time_is_in_sum_zone(TIME_T time);

/**
 * @brief get IoTOS local time (local, contains the time zone and summer time
 * zone)
 *
 * @param[in] in_time the time need translate
 * @param[out] tm the local time in posix format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 * @note if in_time is 0, return the IoTOS local time, otherwise, translate the
 * in_time to local time
 */
OPERATE_RET tal_time_get_local_time_custom(TIME_T in_time, POSIX_TM_S *tm);

/**
 * @brief get sum zone info
 *
 * @param[out] sum zone info
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_sum_zone(SUM_ZONE_TBL_S *sum_zone);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TAL_TIME_SERVICE_H__ */
