/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tuya_ble_os_adapter.h"
#include "tal_thread.h"
#include "tal_semaphore.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_queue.h"
// Base timeq
#include "tal_sw_timer.h"
#include "tal_memory.h"

#define MAX_QUEUE_NUM               16
#define TUYA_OS_ADAPT_QUEUE_FOREVER 0xFFFFFFFF
#define TICK_RATE_MS                1 // use ms,same to tick_count_get

// Tuya Interface Impelement
int tuya_ble_thread_create(void **thread, const char *name, const unsigned int stack_size, const unsigned int priority,
                           const THREAD_FUNC_CB func, void *const arg)
{
    int ret = 0;

    THREAD_CFG_T thread_cfg = {
        .stackDepth = stack_size,
        .priority = priority,
        .thrdname = (char *)name,
    };
    ret = tal_thread_create_and_start(thread, NULL, NULL, func, NULL, &thread_cfg);

    return ret;
}

void tuya_ble_thread_release(void *thread)
{
    tal_thread_delete(thread);
}

static void os_callout_timer_cb(TIMER_ID timerID, void *pTimerArg)
{
    struct tuya_ble_callout *co;

    co = (struct tuya_ble_callout *)(pTimerArg);
    TUYA_HS_ASSERT(co);
    // PR_DEBUG("Co Rc Handle Is:%x, %x ", timerID, co->handle);
    if (co->evq) {
        extern int tuya_ble_eventq_put(tuya_ble_eventq * evq, struct tuya_ble_event * ev);
        tuya_ble_eventq_put(co->evq, &co->ev);
    } else {
        co->ev.fn(&co->ev);
    }
}

static int tuya_callout_timer_create(struct tuya_ble_callout *co, tuya_ble_eventq *evq)
{
    tal_sw_timer_init();
    memset(co, 0, sizeof(*co));

    OPERATE_RET op_ret = tal_sw_timer_create((TAL_TIMER_CB)os_callout_timer_cb, co, &(co->handle));
    co->evq = evq;
    if (op_ret != OPRT_OK) {
        PR_ERR("tal_sw_timer_create error:%d", op_ret);
        tal_sw_timer_delete(co->handle);
        return OPRT_COM_ERROR;
    }

    // PR_DEBUG("Co Cr Handle Is: %x", co->handle);
    return TUYA_BLE_OK;
}

bool tuya_ble_os_started(void)
{
    return false; // xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

#if 0 // There Is No Tuya Interface
bool tuya_ble_check_current_task_id(void *task)
{
    bool is_self = false;
    
    tuya_hal_thread_is_self(task, &is_self);
    BLE_HS_LOG_INFO("is_self = %d", is_self);   
    return is_self;
}
#endif

void tuya_ble_eventq_init(tuya_ble_eventq *evq)
{
    evq->q_num = 0;
    if (tal_queue_create_init((QUEUE_HANDLE *)&evq->queue, sizeof(void *), MAX_QUEUE_NUM) != 0) {
        PR_ERR("QUEUE CREAT ERR\r\n");
        return;
    }
}

struct tuya_ble_event *tuya_ble_eventq_get(tuya_ble_eventq *evq, uint32_t tmo)
{
    struct tuya_ble_event *ev = NULL;

    if (tal_queue_fetch(evq->queue, (void **)&ev, tmo) != 0) {
        return NULL;
    }

    evq->q_num--;
    if (evq->q_num <= 0) {
        ev->queued = false;
        evq->q_num = 0;
    }
    return ev;
}

int tuya_ble_eventq_put(tuya_ble_eventq *evq, struct tuya_ble_event *ev)
{
    int ret = 0;
    if ((!ev) || (!evq)) {
        PR_ERR("ev Invalid parameter\r\n");
        return TUYA_BLE_INVALID_PARAM;
    }
    ev->queued = true;
    evq->q_num++;

    ret = tal_queue_post(evq->queue, (void *)&ev, TUYA_OS_ADAPT_QUEUE_FOREVER);
    if (ret != 0) { // show error
        evq->q_num--;
        PR_ERR("tuya_ble_eventq_put err\r\n");
    }
    return ret;
}

int tuya_ble_eventq_remove(tuya_ble_eventq *evq, struct tuya_ble_event *ev)
{
    int num_delay = 0;
    while (evq->q_num) {
        num_delay++;
        tal_system_sleep(10);
        if (num_delay >= 200) {
            num_delay = 0;
            PR_ERR("ERR REMOVE Q\r\n");
            return TUYA_BLE_EBUSY;
        }
    }
    tal_queue_free(evq->queue);
    PR_ERR("eventq_remove!!!\n\r");
    return TUYA_BLE_OK;
}

void tuya_ble_event_run(struct tuya_ble_event *ev)
{
    if (ev) {
        ev->fn(ev);
    }
}

ty_ble_os_error_t tuya_ble_mutex_init(const tuya_ble_mutex *mu)
{
    if (tal_mutex_create_init((MUTEX_HANDLE *)mu) == 0) {
        return TUYA_BLE_OK;
    }

    return TUYA_BLE_INVALID_PARAM;
}

ty_ble_os_error_t tuya_ble_mutex_lock(const tuya_ble_mutex mu, uint32_t timeout)
{
    if (!mu) {
        return TUYA_BLE_INVALID_PARAM;
    }

    if (tal_mutex_lock((MUTEX_HANDLE)mu) == 0) {
        return TUYA_BLE_OK;
    }
    return TUYA_BLE_INVALID_PARAM;
}

ty_ble_os_error_t tuya_ble_mutex_unlock(const tuya_ble_mutex mu)
{
    if (tal_mutex_unlock((MUTEX_HANDLE)mu) == 0) {
        return TUYA_BLE_OK;
    }
    return TUYA_BLE_INVALID_PARAM;
}

ty_ble_os_error_t tuya_ble_mutex_release(const tuya_ble_mutex mu)
{
    if (!mu) {
        return TUYA_BLE_INVALID_PARAM;
    }

    if (tal_mutex_release((MUTEX_HANDLE)mu) == 0) {
        return TUYA_BLE_OK;
    }

    return TUYA_BLE_INVALID_PARAM;
}

// Semaphore init
ty_ble_os_error_t tuya_ble_sem_init(tuya_ble_sem *sem, uint16_t tokens)
{
    if (!sem) {
        return TUYA_BLE_INVALID_PARAM;
    }

    if (tal_semaphore_create_init((SEM_HANDLE *)sem, tokens, 16) == 0) {
        return TUYA_BLE_OK;
    }

    return TUYA_BLE_ERROR;
}

ty_ble_os_error_t tuya_ble_sem_pend(tuya_ble_sem *sem, uint32_t timeout)
{
    if (tal_semaphore_wait((SEM_HANDLE)*sem, timeout) == 0) {
        return TUYA_BLE_OK;
    }

    return TUYA_BLE_TIMEOUT;
}

ty_ble_os_error_t tuya_ble_sem_post(tuya_ble_sem *sem)
{
    if (tal_semaphore_post((SEM_HANDLE)*sem) == 0) {
        return TUYA_BLE_OK;
    }
    return TUYA_BLE_ERROR;
}

ty_ble_os_error_t tuya_ble_sem_release(tuya_ble_sem *sem)
{
    if (tal_semaphore_release((SEM_HANDLE)*sem) == 0) {
        return TUYA_BLE_OK;
    }
    return TUYA_BLE_ERROR;
}

uint16_t tuya_ble_sem_get_count(tuya_ble_sem *sem)
{
    return 0; // uxSemaphoreGetCount(sem->handle);
}

void tuya_ble_callout_init(struct tuya_ble_callout *co, tuya_ble_eventq *evq, tuya_ble_event_fn *ev_cb, void *ev_arg)
{
    tuya_callout_timer_create(co, evq);
    tuya_ble_event_set_ev(&co->ev, ev_cb, ev_arg);
}

ty_ble_os_error_t tuya_ble_callout_reset(struct tuya_ble_callout *co, uint32_t ticks)
{
    int tick_rate = TICK_RATE_MS;
    if (TICK_RATE_MS == 0) {
        tick_rate = 1;
    }

    tal_sw_timer_stop(co->handle);
    return tal_sw_timer_start(co->handle, ticks * tick_rate, TAL_TIMER_ONCE);
}

void tuya_ble_callout_stop(struct tuya_ble_callout *co)
{
    tal_sw_timer_stop(co->handle);
}

void tuya_ble_callout_delete(struct tuya_ble_callout *co)
{
    tal_sw_timer_delete(co->handle);
}

bool tuya_ble_callout_is_active(struct tuya_ble_callout *co)
{
    return tal_sw_timer_is_running(co->handle);
}

uint32_t tuya_ble_callout_get_ticks(struct tuya_ble_callout *co)
{
    // sys timer can not get residue ticks,use current ms inside
    return tal_system_get_millisecond();
}

uint32_t tuya_ble_callout_remaining_ticks(struct tuya_ble_callout *co, uint32_t now)
{
    return 0;
}

void tuya_ble_callout_set_arg(struct tuya_ble_callout *co, void *arg)
{
    co->ev.arg = arg;
}

uint32_t tuya_ble_tick_count_get(void)
{
    return tal_system_get_millisecond();
}

uint32_t tuya_ble_time_ms_to_ticks32(uint32_t ms)
{
    uint64_t ticks;
    uint32_t tick_rate = 0;

    tick_rate = TICK_RATE_MS;
    if (TICK_RATE_MS == 0) {
        tick_rate = 1;
    }
    ticks = ((uint64_t)ms / tick_rate);

    return ticks;
}

ty_ble_os_error_t tuya_ble_time_ms_to_ticks(uint32_t ms, uint32_t *out_ticks)
{
    *out_ticks = tuya_ble_time_ms_to_ticks32(ms);
    return TUYA_BLE_OK;
}

// delay
void tuya_ble_time_delay(uint32_t ms)
{
    tal_system_sleep(ms);
}

static tuya_ble_mutex critical_mutex;
static int cri_init_flag = 0;

void tuya_ble_hs_enter_critical(void)
{
    if (cri_init_flag == 0) {
        tuya_ble_mutex_init(&critical_mutex);
        cri_init_flag = 1;
    }
    tuya_ble_mutex_lock(critical_mutex, 0xFFFFFFFF);
    cri_init_flag = 2;
}

void tuya_ble_hs_exit_critical(void)
{
    tuya_ble_mutex_unlock(critical_mutex);
    cri_init_flag = 1;
}

bool tuya_ble_hs_is_in_critical(void)
{
    return (cri_init_flag == 2);
}

/**
 * @brief    Allocate a memory block with required size.
 *
 *
 * @param[in]   size     Required memory size.
 *
 * @return     The address of the allocated memory block. If the address is NULL, the
 *             memory allocation failed.
 */
void *tuya_ble_hs_malloc(uint32_t size)
{
    return (void *)tal_malloc(size);
}

/**
 *
 * @brief    Free a memory block that had been allocated.
 *
 * @param[in]   pv     The address of memory block being freed.
 *
 * @return     None.
 */
void tuya_ble_hs_free(void *pv)
{
    tal_free(pv);
}

// [End] End using critical Region
/********************** We dont need to adjust these interface, for common using ***********************/
void tuya_ble_event_set_ev(struct tuya_ble_event *ev, tuya_ble_event_fn *fn, void *arg)
{
    memset(ev, 0, sizeof(*ev));
    ev->fn = fn;
    ev->arg = arg;
}

bool tuya_ble_eventq_is_empty(tuya_ble_eventq *evq)
{
    return true; // xQueueIsQueueEmptyFromISR(evq->q);  //[Remove][Mesh Only]
}

bool tuya_ble_event_is_queued(struct tuya_ble_event *ev)
{
    return ev->queued;
}

void *tuya_ble_event_get_arg(struct tuya_ble_event *ev)
{
    return ev->arg;
}

void tuya_ble_event_set_arg(struct tuya_ble_event *ev, void *arg)
{
    ev->arg = arg;
}
