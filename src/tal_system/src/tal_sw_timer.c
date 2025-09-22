#include "tuya_list.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_semaphore.h"
#include "tal_sw_timer.h"
#include "tal_time_service.h"

#ifndef STACK_SIZE_TIMERQ
#define STACK_SIZE_TIMERQ (4 * 1024)
#endif

typedef struct {
    LIST_HEAD node;

    TAL_TIMER_CB cb;
    void *data;

    uint64_t expire_time;
    TIME_MS interval;
    BOOL_T is_running;
    TIMER_ID timer_id;
    TIMER_TYPE type;
} TIMER_T;

typedef struct {
    LIST_HEAD list_active;
    LIST_HEAD list_standby;
    MUTEX_HANDLE mutex;
    uint16_t total_cnt;
    uint16_t running_cnt;

    BOOL_T inited;
    THREAD_HANDLE thread;
    SEM_HANDLE sem;
    TAL_TIMER_CB last_cb; // used to debug which cb is blocked
} SW_TIMER_MGR_T;

static SW_TIMER_MGR_T s_timer_mgr;

static void __timer_attach(TIMER_T *timer)
{
    tuya_list_del(&(timer->node));

    if (tuya_list_empty(&(s_timer_mgr.list_active))) {
        tuya_list_add_tail(&(timer->node), &(s_timer_mgr.list_active));
    } else {
        TIMER_T *timer_tmp = NULL;
        struct tuya_list_head *p = NULL;
        tuya_list_for_each(p, &(s_timer_mgr.list_active))
        {
            timer_tmp = tuya_list_entry(p, TIMER_T, node);

            if (timer_tmp->expire_time >= timer->expire_time) {
                tuya_list_add(&(timer->node), (&(timer_tmp->node))->prev);
                break;
            }
        }

        if (p == &(s_timer_mgr.list_active)) {
            tuya_list_add_tail(&(timer->node), &(s_timer_mgr.list_active));
        }
    }
}

static void __timer_dump(void)
{
    struct tuya_list_head *p = NULL;
    TIMER_T *timer = NULL;
    TAL_TIMER_CB *cb = NULL;
    TIMER_ID *timer_id = NULL;

    TIME_S nowSecTime = 0;
    TIME_MS nowMsTime = 0;

    tal_time_get_system_time(&nowSecTime, &nowMsTime);

    if (nowSecTime < 30) {
        return;
    }

    PR_NOTICE("current time:%d%03d", nowSecTime, nowMsTime);

    tal_mutex_lock(s_timer_mgr.mutex);

    PR_NOTICE("running timers count:%d", s_timer_mgr.running_cnt);
    tuya_list_for_each(p, &(s_timer_mgr.list_active))
    {
        timer = tuya_list_entry(p, TIMER_T, node);
        cb = &(timer->cb);
        if (timer->data) {
            timer_id = timer->data;
            if (*timer_id == timer->timer_id) {
                cb = (TAL_TIMER_CB *)((char *)timer->data + sizeof(TIMER_ID));
            }
        }
        PR_NOTICE("%08x %d %d %p", timer->timer_id, timer->type, timer->interval, *cb);
    }

    PR_NOTICE("standby timers count:%d", s_timer_mgr.total_cnt - s_timer_mgr.running_cnt);
    tuya_list_for_each(p, &(s_timer_mgr.list_standby))
    {
        timer = tuya_list_entry(p, TIMER_T, node);
        cb = &(timer->cb);
        if (timer->data) {
            timer_id = timer->data;
            if (*timer_id == timer->timer_id) {
                cb = (TAL_TIMER_CB *)((char *)timer->data + sizeof(TIMER_ID));
            }
        }
        PR_NOTICE("%08x %d %d %p", timer->timer_id, timer->type, timer->interval, *cb);
    }

    tal_mutex_unlock(s_timer_mgr.mutex);
}

static void __timer_dispatch(SYS_TIME_T *next_expired)
{
    TIME_S nowSecTime = 0;
    TIME_MS nowMsTime = 0;
    uint64_t nowMS = 0;
    TIMER_T *timer = NULL;
    TAL_TIMER_CB timer_cb = NULL;
    struct tuya_list_head *p = NULL;

    *next_expired = SEM_WAIT_FOREVER;

    do {
        tal_time_get_system_time(&nowSecTime, &nowMsTime);
        nowMS = (uint64_t)nowSecTime * 1000 + (uint64_t)nowMsTime;

        tal_mutex_lock(s_timer_mgr.mutex);

        timer_cb = NULL;
        tuya_list_for_each(p, &(s_timer_mgr.list_active))
        {
            timer = tuya_list_entry(p, TIMER_T, node);

            if (timer->expire_time > nowMS) {
                p = &(s_timer_mgr.list_active);
                *next_expired = timer->expire_time - nowMS;
            } else {
                timer_cb = timer->cb;

                if (TAL_TIMER_ONCE == timer->type) {
                    timer->is_running = FALSE;
                    s_timer_mgr.running_cnt--;
                    tuya_list_del(&(timer->node));
                    tuya_list_add_tail(&(timer->node), &(s_timer_mgr.list_standby));
                } else {
                    timer->expire_time = nowMS + timer->interval;
                    __timer_attach(timer);
                }
            }

            break;
        }

        tal_mutex_unlock(s_timer_mgr.mutex);

        if (timer_cb) {
            s_timer_mgr.last_cb = timer_cb;
            timer_cb(timer->timer_id, timer->data);
            timer_cb = NULL;
            s_timer_mgr.last_cb = NULL;
        }
    } while (p != &(s_timer_mgr.list_active));
}

static void __timer_thread_cb(void *data)
{
    SYS_TIME_T next_expired = SEM_WAIT_FOREVER;

    while (THREAD_STATE_RUNNING == tal_thread_get_state(s_timer_mgr.thread)) {
        // PR_DEBUG_RAW("next_expired:%d\n",next_expired);
        tal_semaphore_wait(s_timer_mgr.sem, next_expired);
        __timer_dispatch(&next_expired);
    }
}

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
OPERATE_RET tal_sw_timer_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (s_timer_mgr.inited) {
        return OPRT_OK;
    }

    tal_mutex_create_init(&s_timer_mgr.mutex);
    tal_semaphore_create_init(&s_timer_mgr.sem, 0, 2);

    INIT_LIST_HEAD(&(s_timer_mgr.list_active));
    INIT_LIST_HEAD(&(s_timer_mgr.list_standby));

    THREAD_CFG_T thread_cfg = {.stackDepth = STACK_SIZE_TIMERQ, .priority = THREAD_PRIO_0, .thrdname = "sys_timer"};

    op_ret = tal_thread_create_and_start(&s_timer_mgr.thread, NULL, NULL, __timer_thread_cb, NULL, &thread_cfg);
    if (OPRT_OK == op_ret) {
        s_timer_mgr.inited = TRUE;
    }

    return op_ret;
}

/**
 * @brief create a software timer
 *
 * @param[in] timer_cb: the processing function of the timer
 * @param[in] timerArg: the parameater of the timer function
 * @param[out] p_timerID: timer id
 *
 * @note This API is used for create a software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_create(TAL_TIMER_CB func, void *arg, TIMER_ID *timer_id)
{
    if ((NULL == func) || (NULL == timer_id)) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)tal_calloc(1, sizeof(TIMER_T));
    if (NULL == timer) {
        return OPRT_MALLOC_FAILED;
    }

    timer->cb = func;
    timer->data = arg;
    timer->timer_id = (TIMER_ID)timer;

    tal_mutex_lock(s_timer_mgr.mutex);
    s_timer_mgr.total_cnt++;
    tuya_list_add_tail(&(timer->node), &(s_timer_mgr.list_standby));
    tal_mutex_unlock(s_timer_mgr.mutex);

    *timer_id = timer->timer_id;

    return OPRT_OK;
}

/**
 * @brief Delete the software timer
 *
 * @param[in] timerID: timer id
 *
 * @note This API is used for deleting the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_delete(TIMER_ID timer_id)
{
    if (NULL == timer_id) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)timer_id;

    tal_mutex_lock(s_timer_mgr.mutex);
    tuya_list_del(&(timer->node));
    s_timer_mgr.total_cnt--;
    if (timer->is_running) {
        s_timer_mgr.running_cnt--;
    }
    tal_mutex_unlock(s_timer_mgr.mutex);
    tal_semaphore_post(s_timer_mgr.sem);
    tal_free(timer);

    return OPRT_OK;
}

/**
 * @brief Stop the software timer
 *
 * @param[in] timerID: timer id
 *
 * @note This API is used for stopping the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_stop(TIMER_ID timer_id)
{
    if (NULL == timer_id) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)timer_id;

    tal_mutex_lock(s_timer_mgr.mutex);
    if (timer->is_running) {
        timer->is_running = FALSE;

        s_timer_mgr.running_cnt--;
        tuya_list_del(&(timer->node));
        tuya_list_add_tail(&(timer->node), &(s_timer_mgr.list_standby));
    }
    tal_mutex_unlock(s_timer_mgr.mutex);
    tal_semaphore_post(s_timer_mgr.sem);

    return OPRT_OK;
}

/**
 * @brief Identify the software timer is running
 *
 * @param[in] timerID: timer id
 *
 * @note This API is used to identify wheather the software timer is running
 *
 * @return TRUE or FALSE
 */
BOOL_T tal_sw_timer_is_running(TIMER_ID timer_id)
{
    if (NULL == timer_id) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)timer_id;
    BOOL_T is_running = FALSE;

    tal_mutex_lock(s_timer_mgr.mutex);
    is_running = timer->is_running;
    tal_mutex_unlock(s_timer_mgr.mutex);

    return is_running;
}

/**
 * @brief Identify the software timer is running
 *
 * @param[in] timer_id: timer id
 * @param[in] remain_time: ms
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_remain_time_get(TIMER_ID timer_id, uint32_t *remain_time)
{
    if (NULL == timer_id || NULL == remain_time) {
        return OPRT_INVALID_PARM;
    }

    uint64_t nowMS = 0;
    TIME_S secTime = 0;
    TIME_MS msTime = 0;
    tal_time_get_system_time(&secTime, &msTime);
    nowMS = (uint64_t)secTime * 1000 + (uint64_t)msTime;

    TIMER_T *timer = (TIMER_T *)timer_id;
    if (!timer->is_running) {
        return timer->interval;
    }

    tal_mutex_lock(s_timer_mgr.mutex);
    if (timer->expire_time > nowMS) {
        *remain_time = timer->expire_time - nowMS;
    } else {
        *remain_time = 0;
    }
    tal_mutex_unlock(s_timer_mgr.mutex);

    return OPRT_OK;
}

/**
 * @brief Start the software timer
 *
 * @param[in] timerID: timer id
 * @param[in] timeCycle: timer running cycle
 * @param[in] timer_type: timer type
 *
 * @note This API is used for starting the software timer
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_start(TIMER_ID timer_id, TIME_MS time_ms, TIMER_TYPE timer_type)
{
    if (NULL == timer_id) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)timer_id;

    TIME_S secTime = 0;
    TIME_MS msTime = 0;
    tal_time_get_system_time(&secTime, &msTime);

    tal_mutex_lock(s_timer_mgr.mutex);

    if (!timer->is_running) {
        timer->is_running = TRUE;
        s_timer_mgr.running_cnt++;
    }

    if (time_ms) {
        timer->interval = time_ms;
    }

    timer->type = timer_type;
    timer->expire_time = (uint64_t)secTime * 1000 + (uint64_t)msTime + timer->interval;
    __timer_attach(timer);

    tal_mutex_unlock(s_timer_mgr.mutex);
    tal_semaphore_post(s_timer_mgr.sem);

    return OPRT_OK;
}

/**
 * @brief Trigger the software timer
 *
 * @param[in] timerID: timer id
 *
 * @note This API is used for triggering the software timer instantly.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sw_timer_trigger(TIMER_ID timer_id)
{
    if (NULL == timer_id) {
        return OPRT_INVALID_PARM;
    }

    TIMER_T *timer = (TIMER_T *)timer_id;

    tal_mutex_lock(s_timer_mgr.mutex);
    timer->expire_time = 0;
    if (timer->is_running) {
        tuya_list_del(&(timer->node));
        tuya_list_add(&(timer->node), &(s_timer_mgr.list_active));
    }
    tal_mutex_unlock(s_timer_mgr.mutex);
    tal_semaphore_post(s_timer_mgr.sem);

    return OPRT_OK;
}

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
OPERATE_RET tal_sw_timer_release(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    return op_ret;
}

/**
 * @brief Get timer node currently
 *
 * @param void
 *
 * @note This API is used for getting the timer node currently.
 *
 * @return the timer node count.
 */
int tal_sw_timer_get_num(void)
{
    if (s_timer_mgr.last_cb) {
        PR_NOTICE("last_cb %p", s_timer_mgr.last_cb);
    }

    return s_timer_mgr.running_cnt;
}

// used for debug
void tal_sw_timer_dump(void)
{
    PR_NOTICE("---------timer queue dump begin---------");
    __timer_dump();
    PR_NOTICE("---------timer queue dump end---------");
}
