#ifndef _TUYA_BLE_OS_ADAPTER_H_
#define _TUYA_BLE_OS_ADAPTER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tal_sw_timer.h"
#include "tal_thread.h"
#include "tal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

// print addr
#ifndef MAC2STR
#define MAC2STR(a)  (a)[5], (a)[4], (a)[3], (a)[2], (a)[1], (a)[0]
#define MACSTR      "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_DIR_STR "%02x%02x%02x%02x%02x%02x"
#endif

#if defined(TARGET_BT_PLATFORM) && (TARGET_BT_PLATFORM == BK_BT_PLATFORM)
#define TUYA_BLE_HOST_STACK_SIZE (1024 * 4)
#else
#define TUYA_BLE_HOST_STACK_SIZE (1024 * 4)
#endif

#define BLE_HS_LOG_DEBUG(fmt, ...) PR_DEBUG(fmt, ##__VA_ARGS__)
#define BLE_HS_LOG_INFO(fmt, ...)  PR_INFO(fmt, ##__VA_ARGS__)
#define BLE_HS_LOG_WARN(fmt, ...)  PR_WARN(fmt, ##__VA_ARGS__)
#define BLE_HS_LOG_ERROR(fmt, ...) PR_ERR(fmt, ##__VA_ARGS__)

#define MESH_HS_LOG_DEBUG(fmt, ...) PR_DEBUG("Mesh-" fmt, ##__VA_ARGS__)
#define MESH_HS_LOG_INFO(fmt, ...)  PR_INFO("[Mesh] " fmt, ##__VA_ARGS__)
#define MESH_HS_LOG_WARN(fmt, ...)  PR_WARN("[Mesh] " fmt, ##__VA_ARGS__)
#define MESH_HS_LOG_ERROR(fmt, ...) PR_ERR("[Mesh] " fmt, ##__VA_ARGS__)

//#define BLE_HS_DUMP_LOG(data, len)              PR_DUMP(data, len)

#define TUYA_BLE_FOREVER_TIME (UINT32_MAX) // portMAX_DELAY//UINT32_MAX  // freertos

#ifndef ble_static_assert
#define ble_static_assert(...)
#endif

#define TUYA_HS_ASSERT(condition)                                                                                      \
    do {                                                                                                               \
        if (condition)                                                                                                 \
            NULL;                                                                                                      \
        else                                                                                                           \
            PR_ERR("Assert Err");                                                                                      \
    } while (0)

// typedef void    (*BLE_THREAD_FUNC_T)(void*);
// struct          tuya_ble_event;
// typedef void    tuya_ble_event_fn(struct tuya_ble_event *ev);
typedef void *tuya_ble_mutex;
typedef void *tuya_ble_sem;

int tuya_ble_thread_create(void **thread, const char *name, const unsigned int stack_size, const unsigned int priority,
                           const THREAD_FUNC_CB func, void *const arg);

void tuya_ble_thread_release(void *thread);

bool tuya_ble_os_started(void);

enum tuya_ble_error {
    TUYA_BLE_OK              = 0,
    TUYA_BLE_ENOMEM          = 1,
    TUYA_BLE_EINVAL          = 2,
    TUYA_BLE_INVALID_PARAM   = 3,
    TUYA_BLE_MEM_NOT_ALIGNED = 4,
    TUYA_BLE_BAD_MUTEX       = 5,
    TUYA_BLE_TIMEOUT         = 6,
    TUYA_BLE_ERR_IN_ISR      = 7,
    TUYA_BLE_ERR_PRIV        = 8,
    TUYA_BLE_OS_NOT_STARTED  = 9,
    TUYA_BLE_ENOENT          = 10,
    TUYA_BLE_EBUSY           = 11,
    TUYA_BLE_ERROR           = 12,
};
typedef enum tuya_ble_error ty_ble_os_error_t;

struct tuya_ble_event;
typedef void tuya_ble_event_fn(struct tuya_ble_event *ev);

typedef struct {
    void *queue;
    int   q_num;
} tuya_ble_eventq;

struct tuya_ble_event {
    bool               queued;
    tuya_ble_event_fn *fn;
    void              *arg;
};

struct tuya_ble_callout {
    TIMER_ID              handle;
    struct tuya_ble_event ev;
    tuya_ble_eventq      *evq;
};

/*
 * Event queue
 */
void tuya_ble_eventq_init(tuya_ble_eventq *evq);

struct tuya_ble_event *tuya_ble_eventq_get(tuya_ble_eventq *evq, uint32_t tmo);

int tuya_ble_eventq_put(tuya_ble_eventq *evq, struct tuya_ble_event *ev);

int tuya_ble_eventq_remove(tuya_ble_eventq *evq, struct tuya_ble_event *ev);

void tuya_ble_event_set_ev(struct tuya_ble_event *ev, tuya_ble_event_fn *fn, void *arg);

bool tuya_ble_event_is_queued(struct tuya_ble_event *ev);

void *tuya_ble_event_get_arg(struct tuya_ble_event *ev);

void tuya_ble_event_set_arg(struct tuya_ble_event *ev, void *arg);

bool tuya_ble_eventq_is_empty(tuya_ble_eventq *evq);

void tuya_ble_event_run(struct tuya_ble_event *ev);

/*
 * Mutexes
 */
ty_ble_os_error_t tuya_ble_mutex_init(const tuya_ble_mutex *mu);

ty_ble_os_error_t tuya_ble_mutex_lock(const tuya_ble_mutex mu, uint32_t timeout);

ty_ble_os_error_t tuya_ble_mutex_unlock(const tuya_ble_mutex mu);

ty_ble_os_error_t tuya_ble_mutex_release(const tuya_ble_mutex mu);

/*
 * Semaphores
 */
ty_ble_os_error_t tuya_ble_sem_init(tuya_ble_sem *sem, uint16_t tokens);

ty_ble_os_error_t tuya_ble_sem_pend(tuya_ble_sem *sem, uint32_t timeout);

ty_ble_os_error_t tuya_ble_sem_post(tuya_ble_sem *sem);

ty_ble_os_error_t tuya_ble_sem_release(tuya_ble_sem *sem);

uint16_t tuya_ble_sem_get_count(tuya_ble_sem *sem);

/*
 * Callouts
 */
void tuya_ble_callout_init(struct tuya_ble_callout *co, tuya_ble_eventq *evq, tuya_ble_event_fn *ev_cb, void *ev_arg);

ty_ble_os_error_t tuya_ble_callout_reset(struct tuya_ble_callout *co, uint32_t ticks);

void tuya_ble_callout_stop(struct tuya_ble_callout *co);

void tuya_ble_callout_delete(struct tuya_ble_callout *co);

bool tuya_ble_callout_is_active(struct tuya_ble_callout *co);

uint32_t tuya_ble_callout_get_ticks(struct tuya_ble_callout *co);

// uint32_t tuya_ble_callout_remaining_ticks(struct tuya_ble_callout *co, uint32_t time);

void tuya_ble_callout_set_arg(struct tuya_ble_callout *co, void *arg);
/*
 * Time functions
 */

uint32_t tuya_ble_tick_count_get(void);

ty_ble_os_error_t tuya_ble_time_ms_to_ticks(uint32_t ms, uint32_t *out_ticks);

uint32_t tuya_ble_time_ms_to_ticks32(uint32_t ms);

void tuya_ble_time_delay(uint32_t ms);

void tuya_ble_hs_enter_critical(void);

void tuya_ble_hs_exit_critical(void);

bool tuya_ble_hs_is_in_critical(void);

void  tuya_ble_hs_free(void *pv);
void *tuya_ble_hs_malloc(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* end tuya ble os adapter, timcheng-20210524 */
