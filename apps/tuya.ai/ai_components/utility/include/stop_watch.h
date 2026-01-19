/**
 * @file stop_watch.h
 * @brief Stopwatch timer module header
 *
 * This header file defines the stopwatch structure and functions for
 * measuring elapsed time and calculating remaining time for timeouts.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __STOP_WATCH_H__
#define __STOP_WATCH_H__

#include "tuya_cloud_types.h"
#include "tal_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    void (*restart)(void *p_sw);
    void (*stop)(void *p_sw);
	SYS_TIME_T (*elapsed_ms)(void *p_sw);
	unsigned long (*time_left)(void *p_sw, unsigned long timeout);
	SYS_TIME_T _begin_time;
	SYS_TIME_T _end_time;
} AI_STOP_WATCH_T;


/***********************************************************
********************function declaration********************
***********************************************************/
#define NewStopWatch(sw) \
    AI_STOP_WATCH_T sw; \
    stop_watch_init(&sw);

/**
@brief Initialize a stopwatch structure
@param sw Pointer to the stopwatch structure to initialize
@return None
*/
void stop_watch_init(AI_STOP_WATCH_T *sw);

#ifdef __cplusplus
}
#endif

#endif /* __STOP_WATCH_H__ */
