/**
 * @file otto_motion_ctrl.h
 * @brief Motion preempt (show only) and FIFO queue for other actions
 * @version 1.1
 * @date 2025-05-22
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __OTTO_MOTION_CTRL_H__
#define __OTTO_MOTION_CTRL_H__

#include "tuya_cloud_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Must match ActionType ACTION_SHOW in otto_robot_main.c */
#define OTTO_MOTION_ACTION_SHOW     11
#define OTTO_MOTION_QUEUE_MAX       8

/**
 * @brief Record action running in motion thread
 * @param[in] move_type Current action type
 * @return none
 */
void otto_motion_set_current(uint32_t move_type);

/**
 * @brief Handle new command while motion thread is busy (preempt show or enqueue)
 * @param[in] move_type New action type
 * @return OPRT_OK on success, OPRT_RESOURCE_NOT_READY if queue full
 */
OPERATE_RET otto_motion_on_busy(uint32_t move_type);

/**
 * @brief Clear abort/pending when starting a fresh motion thread
 * @return none
 */
void otto_motion_begin(void);

/**
 * @brief Check if show sequence should stop (only while ACTION_SHOW runs)
 * @return true if aborted
 */
bool otto_motion_should_abort(void);

/**
 * @brief Sleep with periodic abort check during show
 * @param[in] ms Duration in milliseconds
 * @return none
 */
void otto_motion_sleep_ms(uint32_t ms);

/**
 * @brief Take preempted action after show was interrupted
 * @param[out] move_type Preempted next action
 * @return true if available
 */
bool otto_motion_take_pending(uint32_t *move_type);

/**
 * @brief Dequeue next FIFO action after normal completion
 * @param[out] move_type Queued action
 * @return true if available
 */
bool otto_motion_dequeue(uint32_t *move_type);

#ifdef __cplusplus
}
#endif

#endif /* __OTTO_MOTION_CTRL_H__ */
