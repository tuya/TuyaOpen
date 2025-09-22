/**
 * @file tal_sw_timer.h
 * @brief Provides software timer management functions for Tuya IoT
 * applications.
 *
 * This header file defines the interface for managing software timers in Tuya
 * IoT applications, including functions for initializing the timer system,
 * creating, starting, stopping, deleting timers, and querying timer status.
 * Software timers facilitate time-based operations and scheduling in
 * applications, allowing for timed actions, periodic tasks, and timeout
 * mechanisms without relying on hardware timer resources.
 *
 * The API abstracts the underlying implementation details, offering a simple
 * and efficient way to incorporate timing and scheduling capabilities into IoT
 * applications. This is particularly useful in scenarios where precise timing
 * or periodic task execution is required.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_SW_TIMER_H__
#define __TAL_SW_TIMER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
/**
 * @brief the type of timer
 */
typedef enum {
    TAL_TIMER_ONCE = 0,
    TAL_TIMER_CYCLE,
} TIMER_TYPE;

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/
// Timer ID
typedef void *TIMER_ID;

typedef void (*TAL_TIMER_CB)(TIMER_ID timer_id, void *arg);

/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief Initializing the software timer
 *
 * @param void
 *
 * @note This API is used for initializing the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_init(void);

/**
 * @brief create a software timer
 *
 * @param[in] func: the processing function of the timer
 * @param[in] arg: the parameater of the timer function
 * @param[out] timer_id: timer id
 *
 * @note This API is used for create a software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_create(TAL_TIMER_CB func, void *arg, TIMER_ID *timer_id);

/**
 * @brief Delete the software timer
 *
 * @param[in] timer_id: timer id
 *
 * @note This API is used for deleting the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_delete(TIMER_ID timer_id);

/**
 * @brief Stop the software timer
 *
 * @param[in] timer_id: timer id
 *
 * @note This API is used for stopping the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_stop(TIMER_ID timer_id);

/**
 * @brief Identify the software timer is running
 *
 * @param[in] timer_id: timer id
 *
 * @note This API is used to identify wheather the software timer is running
 *
 * @return TRUE or FALSE
 */
BOOL_T tal_sw_timer_is_running(TIMER_ID timer_id);

/**
 * @brief Identify the software timer is running
 *
 * @param[in] timer_id: timer id
 * @param[in] remain_time: ms
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_remain_time_get(TIMER_ID timer_id, uint32_t *remain_time);

/**
 * @brief Start the software timer
 *
 * @param[in] timer_id: timer id
 * @param[in] time_ms: timer running cycle
 * @param[in] timer_type: timer type
 *
 * @note This API is used for starting the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_start(TIMER_ID timer_id, TIME_MS time_ms, TIMER_TYPE timer_type);

/**
 * @brief Trigger the software timer
 *
 * @param[in] timer_id: timer id
 *
 * @note This API is used for triggering the software timer instantly.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_trigger(TIMER_ID timer_id);

/**
 * @brief Release all resource of the software timer
 *
 * @param void
 *
 * @note This API is used for releasing all resource of the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_release(void);

/**
 * @brief Get timer node currently
 *
 * @param void
 *
 * @note This API is used for getting the timer node currently.
 *
 * @return the timer node count.
 */
int tal_sw_timer_get_num(void);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_SW_TIMER_H__ */
