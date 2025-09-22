/**
 * @file tal_time_service.c
 * @brief Implements time management services for Tuya IoT applications.
 *
 * This source file provides the implementation of time management services
 * within the Tuya Abstract Layer (TAL), offering functionalities such as time
 * synchronization, time zone management, and conversion utilities between
 * different time formats. It encapsulates the complexities of handling leap
 * years, different time zones, and daylight saving adjustments, providing a
 * unified and simplified interface for time operations in Tuya IoT
 * applications.
 *
 * Key functionalities include:
 * - Time synchronization with external time servers.
 * - Time zone management and daylight saving adjustments.
 * - Conversion utilities for Unix timestamp, calendar date, and more.
 * - Thread-safe operations for time updates and retrievals.
 *
 * The implementation leverages mutexes for thread-safe access to time variables
 * and provides efficient algorithms for time conversions, taking into account
 * leap years and varying days per month. It also integrates with the TAL event
 * system for time update notifications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_time_service.h"
#include "tal_mutex.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "tal_log.h"
#include "tal_event.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define SEC_PER_DAY  86400
#define SEC_PER_HOUR 3600

/***********************************************************
*************************variable define********************
***********************************************************/
static const TIME_T SEC_PER_YR[2] = {31536000, 31622400};
static const TIME_T SEC_PER_MT[2][12] = {
    {2678400, 2419200, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400},
    {2678400, 2505600, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400},
};

static MUTEX_HANDLE s_time_mutex = NULL;
static SYS_TIME_T s_time_last_ms = 0;
static BOOL_T s_time_tz_sync = FALSE;
static int s_time_tz = 0;
static SUM_ZONE_TBL_S s_time_sz_tbl = {0};
static BOOL_T s_time_cloud_sync = FALSE;
static TIME_T s_time_cloud_posix = 0;
static BOOL_T s_time_disable_update = FALSE;

/***********************************************************
*************************function define********************
***********************************************************/

/**
 * @brief Checks if the given time is within the summer time zone.
 *
 * This function checks if the given time falls within any of the defined summer
 * time zones.
 *
 * @param time The time to be checked.
 * @return TRUE if the time is within a summer time zone, FALSE otherwise.
 */
BOOL_T tal_time_is_in_sum_zone(TIME_T time)
{
    tal_mutex_lock(s_time_mutex);
    uint32_t i = 0;
    for (i = 0; i < s_time_sz_tbl.cnt; i++) {
        if ((time >= s_time_sz_tbl.zone[i].posix_min) && (time <= s_time_sz_tbl.zone[i].posix_max)) {
            tal_mutex_unlock(s_time_mutex);
            return TRUE;
        }
    }
    tal_mutex_unlock(s_time_mutex);

    return FALSE;
}

/**
 * @brief Checks if the current time is within the summer time zone.
 *
 * This function retrieves the current time using the `tal_time_get_posix()`
 * function and checks if it falls within the summer time zone. The summer time
 * zone is determined by the `tal_time_is_in_sum_zone()` function.
 *
 * @return TRUE if the current time is within the summer time zone, FALSE
 * otherwise.
 */
static BOOL_T __is_in_sum_zone(void)
{
    TIME_T time = tal_time_get_posix();
    if ((time > tal_time_get_posix())) {
        return FALSE;
    }

    return tal_time_is_in_sum_zone(time);
}

/**
 * Returns 1 if current year id a leap year
 */
static int __is_leap(int yr)
{
    if (!(yr % 100)) {
        return (yr % 400 == 0) ? 1 : 0;
    } else {
        return (yr % 4 == 0) ? 1 : 0;
    }
}

/**
 * @brief Checks if the given date and time are valid.
 *
 * This function checks if the provided date and time values are within the
 * valid range.
 *
 * @param tm Pointer to a POSIX_TM_S structure containing the date and time
 * values.
 * @return Returns 0 if the date and time are valid, or -1 if any of the values
 * are out of range.
 */
static int __is_valid_date_time(const POSIX_TM_S *tm)
{
    if (tm->tm_sec > 59) {
        return -1;
    }
    if (tm->tm_min > 59) {
        return -1;
    }
    if (tm->tm_hour > 23) {
        return -1;
    }
    if (((tm->tm_year + 1900) < 1970) || ((tm->tm_year + 1900) > 2111)) {
        return -1;
    }
    if (tm->tm_mon >= 12) {
        return -1;
    }
    if (tm->tm_mon == 1) {
        if (!__is_leap(tm->tm_year + 1900)) {
            if (tm->tm_mday > 28) {
                return -1;
            }
        } else {
            if (tm->tm_mday > 29) {
                return -1;
            }
        }
    }
    switch (tm->tm_mon) {
    case 3:
    case 5:
    case 8:
    case 10:
        if (tm->tm_mday > 30) {
            return -1;
        }
    }

    if ((tm->tm_mday < 1) || (tm->tm_mday > 31)) {
        return -1;
    }

    return 0;
}

static uint8_t __get_day_of_week(uint8_t month, uint8_t day, uint16_t year)
{
    /* Month should be a number 0 to 11, Day should be a number 1 to 31 */

    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

static OPERATE_RET __get_time_zone(const char *time_zone, int *posix)
{
    char *p = NULL;
    int offset = 1, plus = 1;

    p = strstr(time_zone, "+");
    if (p == NULL) {
        p = strstr(time_zone, "-");
        if (p == NULL) {
            offset = 0;
        } else {
            plus = 0; //-
        }
    }

    int num = 0, buf[2] = {0};
    num = sscanf(time_zone + offset, "%d:%d", &buf[0], &buf[1]);
    if (num < 2) {
        return OPRT_INVALID_PARM;
    }
    *posix = (buf[0] * 60 + buf[1]) * 60;
    if (plus == 0) {
        *posix = -(*posix);
    }

    return OPRT_OK;
}

/**
 * @brief time-management module initialization
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_service_init(void)
{
    if (s_time_mutex) {
        return OPRT_OK;
    }

    OPERATE_RET op_ret = OPRT_OK;
    op_ret = tal_mutex_create_init(&s_time_mutex);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }

    s_time_cloud_posix = 0;
    s_time_last_ms = tal_system_get_millisecond();
    memset(&s_time_sz_tbl, 0, sizeof(s_time_sz_tbl));

    return OPRT_OK;
}

/**
 * @brief change posix time to TIME_T, redefine the std C func mktime
 *
 * @param[in] tm the time in posix time format
 *
 * @return the time in TIME_T format
 */
TIME_T tal_time_mktime(const POSIX_TM_S *tm)
{
    if (NULL == tm) {
        return 0;
    }

    int i;
    int leapyr = 0;

    TIME_T time = 0;

    for (i = 1970; i < (tm->tm_year + 1900); i++) {
        time += SEC_PER_YR[__is_leap(i)];
    }

    if (__is_leap(tm->tm_year + 1900)) {
        leapyr = 1;
    }

    for (i = 0; i < (tm->tm_mon); i++) {
        time += SEC_PER_MT[leapyr][i];
    }

    time += ((tm->tm_mday) - 1) * SEC_PER_DAY;
    time += (tm->tm_hour) * SEC_PER_HOUR;
    time += (tm->tm_min) * 60;
    time += tm->tm_sec;

    return time;
}

/**
 * @brief change TIME_T to posix time, redefine the std C func gmtime_r
 *
 * @param[in] tm the time in TIME_T format
 * @param[out] result the time in posix time format
 * @return the time in posix time format
 */
POSIX_TM_S *tal_time_gmtime_r(const TIME_T *tm, POSIX_TM_S *result)
{
    int leapyr = 0;
    TIME_T ltm = *tm;

    memset(result, 0, sizeof(POSIX_TM_S));
    result->tm_year = 1970;

    while (1) {
        if (ltm < SEC_PER_YR[__is_leap(result->tm_year)]) {
            break;
        }
        ltm -= SEC_PER_YR[__is_leap(result->tm_year)];
        result->tm_year++;
    }

    leapyr = __is_leap(result->tm_year);

    while (1) {
        if (ltm < SEC_PER_MT[leapyr][result->tm_mon]) {
            break;
        }
        ltm -= SEC_PER_MT[leapyr][result->tm_mon];
        result->tm_mon++;
    }

    result->tm_mday = ltm / SEC_PER_DAY;
    result->tm_mday++;
    ltm = ltm % SEC_PER_DAY;

    result->tm_hour = ltm / SEC_PER_HOUR;
    ltm = ltm % SEC_PER_HOUR;

    result->tm_min = ltm / 60;
    result->tm_sec = ltm % 60;

    result->tm_wday = __get_day_of_week(result->tm_mon + 1, result->tm_mday, result->tm_year);

    /*
     * Solve bug WMSDK-27. 'man gmtime' says:
     * "tm_year   The number of years since 1900."
     */
    result->tm_year -= 1900;

    return result;
}

/**
 * @brief set IoTOS UTC time
 *
 * @param[in] tm the new time in posix time format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set(const POSIX_TM_S *tm)
{
    TIME_T time;
    if (__is_valid_date_time(tm) == 0) {
        time = tal_time_mktime(tm);
        return tal_time_set_posix(time, 2);
    } else {
        return OPRT_COM_ERROR;
    }
}

/**
 * @brief get IoTOS UTC time in posix time format
 *
 * @param[out] tm the IoTOS UTC time in posix time format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get(POSIX_TM_S *tm)
{
    if (NULL == tm) {
        return OPRT_INVALID_PARM;
    }

    TIME_T curtime = tal_time_get_posix();
    if (tal_time_gmtime_r((const TIME_T *)&curtime, tm) == NULL) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief set IoTOS UTC time in time_t format
 *
 * @param[in] time the new time in time_t format
 * @param[in] update_source the source of time (0:rtc 1:cloud 2:other)
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_posix(const TIME_T time, const int update_source)
{
    if (!s_time_disable_update) { // for aging test
        tal_mutex_lock(s_time_mutex);
        s_time_cloud_posix = time;
        s_time_last_ms = tal_system_get_millisecond();
        tal_mutex_unlock(s_time_mutex);

        if (update_source == 1) {
            s_time_cloud_sync = TRUE;
        }
    }

    return OPRT_OK;
}

/**
 * @brief get IoTOS last synchronized UTC time in TIME_T format
 *
 * @return the time in TIME_T format
 */
TIME_T tal_time_get_cur_posix(void)
{
    return s_time_cloud_posix;
}

/**
 * @brief check IoTOS time synchronize status
 *
 * @return OPRT_OK on synchronized. Others on not
 */
OPERATE_RET tal_time_check_time_sync(void)
{
    if (s_time_cloud_sync == FALSE) {
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief check IoTOS time zone synchronized status
 *
 * @return OPRT_OK on synchronized. Others on not
 */
OPERATE_RET tal_time_check_time_zone_sync(void)
{
    if (s_time_tz_sync == FALSE) {
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief get IoTOS UTC time in TIME_T format
 *
 * @return the current second time in TIME_T format
 */
TIME_T tal_time_get_posix(void)
{
    TIME_T tmp_cur_posix_time = 0;

    tal_mutex_lock(s_time_mutex);
    SYS_TIME_T curr_time_ms = tal_system_get_millisecond();

    if (s_time_last_ms > curr_time_ms) { // recycle
        s_time_cloud_posix += ((0x100000000 - s_time_last_ms) / 1000);
        s_time_last_ms = 0;
    }

    tmp_cur_posix_time = s_time_cloud_posix;
    tmp_cur_posix_time += (curr_time_ms - s_time_last_ms) / 1000;
    tal_mutex_unlock(s_time_mutex);

    return tmp_cur_posix_time;
}

/**
 * @brief get IoTOS UTC time in SYS_TICK_T format
 *
 * @return the current micro-second time in SYS_TICK_T format
 */
SYS_TICK_T tal_time_get_posix_ms(void)
{
    SYS_TIME_T tmp_cur_posix_time_ms = 0;

    tal_mutex_lock(s_time_mutex);
    SYS_TIME_T curr_time_ms = tal_system_get_millisecond();

    if (s_time_last_ms > curr_time_ms) { // recycle
        s_time_cloud_posix += ((0x100000000 - s_time_last_ms) / 1000);
        s_time_last_ms = 0;
    }

    tmp_cur_posix_time_ms = (SYS_TICK_T)s_time_cloud_posix * 1000;
    tmp_cur_posix_time_ms += (curr_time_ms - s_time_last_ms);
    tal_mutex_unlock(s_time_mutex);

    return tmp_cur_posix_time_ms;
}

/**
 * @brief how long times system run
 *
 * @param[out] pSecTime the current time in second
 * @param[out] pMsTime the current time in micro-second
 * @return void
 */
void tal_time_get_system_time(TIME_S *pSecTime, TIME_MS *pMsTime)
{
    static SYS_TIME_T last_ms = 0;
    static SYS_TIME_T roll_ms = 0;
    SYS_TIME_T curr_ms = 0;

    tal_mutex_lock(s_time_mutex);

    curr_ms = tal_system_get_millisecond();
    if (last_ms > curr_ms) { // recycle
        roll_ms += 0x100000000;
    }
    if (pMsTime) {
        *pMsTime = (curr_ms + roll_ms) % 1000;
    }
    if (pSecTime) {
        *pSecTime = (curr_ms + roll_ms) / 1000;
    }
    last_ms = curr_ms;

    tal_mutex_unlock(s_time_mutex);
}

/**
 * @brief set IoTOS time zone in "+/-hh:mm" format
 *
 * @param[in] time_zone the new time zone in "+/-hh:mm" format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_time_zone(const char *time_zone)
{
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = __get_time_zone(time_zone, &s_time_tz);
    if (op_ret != OPRT_OK) {
        return op_ret;
    }
    s_time_tz_sync = TRUE;

    return OPRT_OK;
}

/**
 * @brief get IoTOS time zone in second format
 *
 * @param[out] time_zone the current time zone in second format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_time_zone_seconds(int *time_zone)
{
    *time_zone = s_time_tz;
    return OPRT_OK;
}

/**
 * @brief set IoTOS time zone in second format
 *
 * @param[in] time_zone the new time zone in second format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_set_time_zone_seconds(int time_zone_sec)
{
    s_time_tz = time_zone_sec;
    s_time_tz_sync = TRUE;
    return OPRT_OK;
}

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
OPERATE_RET tal_time_get_local_time_custom(TIME_T in_time, POSIX_TM_S *tm)
{
    if (NULL == tm) {
        return OPRT_INVALID_PARM;
    }

    TIME_T local_time = 0;
    if (in_time == 0) {
        local_time = tal_time_get_posix() + s_time_tz;
    } else {
        local_time = in_time + s_time_tz;
    }

    if (TRUE == __is_in_sum_zone()) {
        local_time += SEC_PER_HOUR;
    }

    if (tal_time_gmtime_r((const TIME_T *)&local_time, tm) == NULL) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief set IoTOS summer time zone
 *
 * @param[in] zone the summer time zone table
 * @param[in] cnt the summer time counts
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_time_set_sum_zone_tbl(const SUM_ZONE_S *zone, const uint32_t cnt)
{
    if (NULL == zone || 0 == cnt) {
        s_time_sz_tbl.cnt = 0;
        return;
    }

    tal_mutex_lock(s_time_mutex);
    s_time_sz_tbl.cnt = cnt;
    if (cnt > SUM_ZONE_TAB_LMT) {
        s_time_sz_tbl.cnt = SUM_ZONE_TAB_LMT;
    }

    memcpy(s_time_sz_tbl.zone, zone, sizeof(SUM_ZONE_S) * s_time_sz_tbl.cnt);

    tal_mutex_unlock(s_time_mutex);
    return;
}

/**
 * @brief get IoTOS UTC summer time in posix time format
 *
 * @param[out] tm the summer time in posix format
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_sum_time(POSIX_TM_S *tm)
{
    TIME_T time = tal_time_get_sum_time_posix();

    if (tal_time_gmtime_r((const TIME_T *)&time, tm) == NULL) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief get IoTOS UTC summer time int TIME_T format
 *
 * @return the summer time in TIME_T format
 */
TIME_T tal_time_get_sum_time_posix(void)
{
    TIME_T time = tal_time_get_posix();

    if (TRUE == __is_in_sum_zone()) {
        time += SEC_PER_HOUR;
    }

    return time;
}

/**
 * @brief get sum zone info
 *
 * @param[out] sum zone info
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_time_get_sum_zone(SUM_ZONE_TBL_S *sum_zone)
{
    if (NULL == sum_zone) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_time_mutex);
    memcpy(sum_zone, &s_time_sz_tbl, sizeof(SUM_ZONE_TBL_S));
    tal_mutex_unlock(s_time_mutex);

    return OPRT_OK;
}

/**
 * @brief Updates the time configuration.
 *
 * This function updates the time configuration by enabling or disabling the
 * time update.
 *
 * @param disable A boolean value indicating whether to disable the time update.
 *                - `TRUE` to disable the time update.
 *                - `FALSE` to enable the time update.
 *
 * @return The result of the operation.
 *         - `OPRT_OK` if the operation is successful.
 *         - An error code if the operation fails.
 */
OPERATE_RET tal_time_config_update(BOOL_T disable)
{
    s_time_disable_update = disable;
    return OPRT_OK;
}
