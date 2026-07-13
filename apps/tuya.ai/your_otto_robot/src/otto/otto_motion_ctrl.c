/**
 * @file otto_motion_ctrl.c
 * @brief Motion preempt (show only) and FIFO queue for other actions
 * @version 1.1
 * @date 2025-05-22
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "otto_motion_ctrl.h"
#include "tal_api.h"

#define OTTO_MOTION_SLEEP_SLICE_MS  50

static volatile bool s_motion_abort = false;
static volatile bool s_motion_pending_valid = false;
static uint32_t s_motion_pending_type = 0;
static uint32_t s_motion_current_type = 0;

static uint32_t s_motion_queue[OTTO_MOTION_QUEUE_MAX];
static uint8_t s_motion_queue_head = 0;
static uint8_t s_motion_queue_count = 0;

/**
 * @brief Enqueue action when motion thread is busy (non-show)
 * @param[in] move_type Action type
 * @return OPRT_OK or OPRT_RESOURCE_NOT_READY
 */
static OPERATE_RET __otto_motion_enqueue(uint32_t move_type)
{
    if (s_motion_queue_count >= OTTO_MOTION_QUEUE_MAX) {
        PR_WARN("motion queue full, drop action %d", move_type);
        return OPRT_RESOURCE_NOT_READY;
    }
    uint8_t tail = (s_motion_queue_head + s_motion_queue_count) % OTTO_MOTION_QUEUE_MAX;
    s_motion_queue[tail] = move_type;
    s_motion_queue_count++;
    PR_DEBUG("motion enqueue %d, depth %d", move_type, s_motion_queue_count);
    return OPRT_OK;
}

/**
 * @brief Preempt running show and run new action next
 * @param[in] move_type New action type
 * @return none
 */
static void __otto_motion_preempt(uint32_t move_type)
{
    s_motion_abort = true;
    s_motion_pending_type = move_type;
    s_motion_pending_valid = true;
    PR_DEBUG("motion preempt show -> %d", move_type);
}

/**
 * @brief Record action running in motion thread
 * @param[in] move_type Current action type
 * @return none
 */
void otto_motion_set_current(uint32_t move_type)
{
    s_motion_current_type = move_type;
}

/**
 * @brief Handle new command while motion thread is busy
 * @param[in] move_type New action type
 * @return OPRT_OK on success, OPRT_RESOURCE_NOT_READY if queue full
 */
OPERATE_RET otto_motion_on_busy(uint32_t move_type)
{
    if (s_motion_current_type == OTTO_MOTION_ACTION_SHOW) {
        __otto_motion_preempt(move_type);
        return OPRT_OK;
    }
    return __otto_motion_enqueue(move_type);
}

/**
 * @brief Clear abort/pending when starting a fresh motion thread
 * @return none
 */
void otto_motion_begin(void)
{
    s_motion_abort = false;
    s_motion_pending_valid = false;
}

/**
 * @brief Check if show sequence should stop
 * @return true if aborted during show
 */
bool otto_motion_should_abort(void)
{
    if (!s_motion_abort) {
        return false;
    }
    return (s_motion_current_type == OTTO_MOTION_ACTION_SHOW);
}

/**
 * @brief Sleep with periodic abort check during show
 * @param[in] ms Duration in milliseconds
 * @return none
 */
void otto_motion_sleep_ms(uint32_t ms)
{
    uint32_t elapsed = 0;

    while (elapsed < ms) {
        if (otto_motion_should_abort()) {
            return;
        }
        uint32_t slice = ms - elapsed;
        if (slice > OTTO_MOTION_SLEEP_SLICE_MS) {
            slice = OTTO_MOTION_SLEEP_SLICE_MS;
        }
        tal_system_sleep(slice);
        elapsed += slice;
    }
}

/**
 * @brief Take preempted action after show was interrupted
 * @param[out] move_type Preempted action
 * @return true if available
 */
bool otto_motion_take_pending(uint32_t *move_type)
{
    if (move_type == NULL || !s_motion_pending_valid) {
        return false;
    }
    *move_type = s_motion_pending_type;
    s_motion_pending_valid = false;
    s_motion_abort = false;
    return true;
}

/**
 * @brief Dequeue next FIFO action
 * @param[out] move_type Queued action
 * @return true if available
 */
bool otto_motion_dequeue(uint32_t *move_type)
{
    if (move_type == NULL || s_motion_queue_count == 0) {
        return false;
    }
    *move_type = s_motion_queue[s_motion_queue_head];
    s_motion_queue_head = (s_motion_queue_head + 1) % OTTO_MOTION_QUEUE_MAX;
    s_motion_queue_count--;
    PR_DEBUG("motion dequeue %d, depth left %d", *move_type, s_motion_queue_count);
    return true;
}
