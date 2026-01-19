/**
 * @file stop_watch.c
 * @brief Stopwatch timer module implementation
 *
 * This module provides a stopwatch functionality for measuring elapsed time
 * and calculating remaining time for timeouts. It uses a function pointer
 * structure to provide a flexible timer interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>
#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "stop_watch.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
/**
@brief Restart the stopwatch timer
@param p_sw Pointer to the stopwatch structure
@return None
*/
static void __restart(void *p_sw)
{
    AI_STOP_WATCH_T *sw = (AI_STOP_WATCH_T *)p_sw;
    sw->_begin_time = tal_system_get_millisecond();
    sw->_end_time = 0;
}

/**
@brief Stop the stopwatch timer
@param p_sw Pointer to the stopwatch structure
@return None
*/
static void __stop(void *p_sw)
{
    AI_STOP_WATCH_T *sw = (AI_STOP_WATCH_T *)p_sw;
    sw->_end_time = tal_system_get_millisecond();
}

/**
@brief Get elapsed time in milliseconds
@param p_sw Pointer to the stopwatch structure
@return Elapsed time in milliseconds
*/
static SYS_TIME_T __elapsed_ms(void *p_sw)
{
    AI_STOP_WATCH_T *sw = (AI_STOP_WATCH_T *)p_sw;
    if (sw->_end_time == 0) {
        return tal_system_get_millisecond() - sw->_begin_time;
    } else {
        return sw->_end_time - sw->_begin_time;
    }
}

/**
@brief Calculate remaining time for a timeout
@param p_sw Pointer to the stopwatch structure
@param timeout Timeout value in milliseconds
@return Remaining time in milliseconds, 0 if timeout has expired
*/
static unsigned long __time_left(void *p_sw, unsigned long timeout)
{
    AI_STOP_WATCH_T *sw = (AI_STOP_WATCH_T *)p_sw;
    SYS_TIME_T es = sw->elapsed_ms(sw);
    if (es >= timeout) {
        return 0;
    } else {
        return timeout - es;
    }
}

/**
@brief Initialize a stopwatch structure
@param sw Pointer to the stopwatch structure to initialize
@return None
*/
void stop_watch_init(AI_STOP_WATCH_T *sw)
{
    memset(sw, 0, sizeof(AI_STOP_WATCH_T));
    sw->_end_time = sw->_begin_time = tal_system_get_millisecond();
    sw->restart = __restart;
    sw->stop = __stop;
    sw->elapsed_ms = __elapsed_ms;
    sw->time_left = __time_left;
}