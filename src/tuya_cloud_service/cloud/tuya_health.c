/**
 * @file tuya_health.c
 * @brief Implementation of Tuya's health monitoring functionalities.
 *
 * This source file contains the implementation of health monitoring
 * functionalities for Tuya IoT devices. It includes the definition of data
 * structures and functions for tracking and managing health metrics of the
 * device. The health monitoring system is designed to track various operational
 * metrics, such as the last update time and occurrence count of specific
 * events, to ensure the device operates within its expected parameters.
 *
 * The file leverages Tuya's IoT SDK to provide a robust framework for health
 * monitoring, including thread and mutex management for concurrent operations.
 * The health monitoring functionalities are critical for maintaining the
 * reliability and stability of IoT devices in the field, allowing for proactive
 * maintenance and troubleshooting.
 *
 * Conditional compilation flags such as ENABLE_WATCHDOG are used to include
 * additional functionalities like watchdog timer management, based on the
 * project configuration.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tuya_iot_config.h"
#include "tal_api.h"
#include "tuya_health.h"
#if ENABLE_WATCHDOG
#include "tkl_watchdog.h"
#endif
#ifndef STACK_SIZE_HEALTH_MONITOR
#define STACK_SIZE_HEALTH_MONITOR (2048)
#endif

// health monitor detection index
typedef struct {
    health_policy_t policy;
    TIME_T ts;            // Time of the last update for the corresponding metric
    uint32_t cnt;         // Number of occurrences of the current metric
    int detect_time_left; // Remaining detection time
} health_item_t;

typedef struct {
    LIST_HEAD node;
    health_item_t item;
} health_node_t;

typedef struct {
    THREAD_HANDLE thread;
    MUTEX_HANDLE mutex;
    int global_type;
    LIST_HEAD listHead;
} health_mgr_t;

static health_mgr_t *s_health_mgr = NULL;

#if defined(ENABLE_WATCHDOG) && (ENABLE_WATCHDOG == 1)
static uint32_t __watchdog_init_and_start(const int timeval)
{
    PR_DEBUG("init watchdog, interval: %d", timeval);

    TUYA_WDOG_BASE_CFG_T cfg;
    cfg.interval_ms = timeval * 1000;
    return tkl_watchdog_init(&cfg) / 1000;
}
#endif

// Placed in query instead of notify to ensure the work queue does not affect
// the health thread
static bool __watchdog_feed(void)
{
#if defined(ENABLE_WATCHDOG) && (ENABLE_WATCHDOG == 1)
    PR_DEBUG("feed watchdog");
    tkl_watchdog_refresh();
#endif
    return FALSE;
}

static int __health_reboot_cb(void *data)
{
    PR_DEBUG("recive reboot req ack! device will reboot!");
    tal_system_reset();
    return OPRT_OK;
}

/**
 * @brief Adds a health item to the health manager.
 *
 * This function adds a health item to the health manager with the specified
 * threshold, period, check callback, and notify callback.
 *
 * @param threshold The threshold value for the health item.
 * @param period The period at which the health item should be checked.
 * @param check The callback function to be called for checking the health item.
 * @param notify The callback function to be called for notifying about the
 * health item.
 *
 * @return The type of the added health item, or OPRT_INVALID_PARM if the health
 * manager is NULL or the global type is too large, or OPRT_MALLOC_FAILED if a
 * new list node cannot be created.
 */
int tuya_health_item_add(uint32_t threshold, uint32_t period, health_check_cb check, health_notify_cb notify)
{
    if (NULL == s_health_mgr) {
        PR_ERR("s_health_mgr null");
        return OPRT_INVALID_PARM;
    }

    if (s_health_mgr->global_type > 128) {
        PR_ERR("global_type:%d too large", s_health_mgr->global_type);
        return OPRT_INVALID_PARM;
    }

    health_node_t *health_node;
    NEW_LIST_NODE(health_node_t, health_node);
    if (!health_node) {
        PR_ERR("new list node error");
        return OPRT_MALLOC_FAILED;
    }
    memset(health_node, 0, sizeof(health_node_t));
    int type = s_health_mgr->global_type;
    s_health_mgr->global_type++;

    health_node->item.policy.threshold = threshold;
    health_node->item.policy.detect_period = period;
    health_node->item.policy.check_cb = check;
    health_node->item.policy.notify_cb = notify;

    health_node->item.policy.type = type;
    health_node->item.detect_time_left = period;

    PR_DEBUG("add new node,type:%d", type);

    tal_mutex_lock(s_health_mgr->mutex);
    tuya_list_add(&(health_node->node), &(s_health_mgr->listHead));
    tal_mutex_unlock(s_health_mgr->mutex);

    return type;
}

/**
 * @brief Deletes a health item of a specified type from the health manager.
 *
 * This function deletes a health item of the specified type from the health
 * manager's list. It locks the health manager's mutex, iterates through the
 * list, and deletes the first health item with a matching type. After deleting
 * the item, it unlocks the mutex and returns.
 *
 * @param type The type of the health item to be deleted.
 */
void tuya_health_item_del(int type)
{
    if (NULL == s_health_mgr) {
        PR_ERR("s_health_mgr null");
        return;
    }

    tal_mutex_lock(s_health_mgr->mutex);
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if (health_node) {
            if (health_node->item.policy.type == type) {
                PR_DEBUG("delete old node,type:%d", type);
                DeleteNodeAndFree(health_node, node);
                break;
            }
        }
    }
    tal_mutex_unlock(s_health_mgr->mutex);
    return;
}

/**
 * @brief Updates the period of a health item.
 *
 * This function updates the period of a health item based on the specified
 * type. If the health manager is NULL, an error message is printed and the
 * function returns. The function iterates through the list of health nodes and
 * updates the period of the health item if its type matches the specified type.
 *
 * @param type The type of the health item.
 * @param period The new period for the health item.
 */
void tuya_health_update_item_period(int type, uint32_t period)
{
    if (NULL == s_health_mgr) {
        PR_ERR("s_health_mgr null");
        return;
    }

    tal_mutex_lock(s_health_mgr->mutex);
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if (health_node) {
            if (health_node->item.policy.type == type) {
                PR_DEBUG("update type:%d,period:%d", type, period);
                health_node->item.policy.detect_period = period;
                health_node->item.detect_time_left = period;
            }
        }
    }
    tal_mutex_unlock(s_health_mgr->mutex);
    return;
}

/**
 * @brief Updates the threshold value for a specific health item.
 *
 * This function updates the threshold value for a health item of the specified
 * type. The health item is stored in a linked list managed by the health
 * manager.
 *
 * @param type The type of the health item.
 * @param threshold The new threshold value to be set.
 */
void tuya_health_update_item_threshold(int type, uint32_t threshold)
{
    if (NULL == s_health_mgr) {
        PR_ERR("s_health_mgr null");
        return;
    }

    tal_mutex_lock(s_health_mgr->mutex);
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if (health_node) {
            if (health_node->item.policy.type == type) {
                PR_DEBUG("update type:%d,threshold:%d", type, threshold);
                health_node->item.policy.threshold = threshold;
            }
        }
    }
    tal_mutex_unlock(s_health_mgr->mutex);
    return;
}

/**
 * @brief Dumps the health items stored in the health manager.
 *
 * This function prints the information of each health item stored in the health
 * manager. It prints the node ID, detect time left, count, timestamp, type,
 * check callback function, notify callback function, threshold, and detect
 * period for each health item.
 *
 * @note This function assumes that the health manager and its mutex are
 * properly initialized.
 */
void tuya_health_item_dump(void)
{
    uint32_t node_num = 0;
    tal_mutex_lock(s_health_mgr->mutex);
    PR_DEBUG("global_type_id:%d", s_health_mgr->global_type);
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if (health_node) {
            PR_DEBUG("node id:%d", node_num);
            PR_DEBUG("detect_time_left:%d", health_node->item.detect_time_left);
            PR_DEBUG("cnt:%d", health_node->item.cnt);
            PR_DEBUG("ts:%d", health_node->item.ts);
            PR_DEBUG("type:%d", health_node->item.policy.type);
            if (health_node->item.policy.check_cb) {
                PR_DEBUG("check_cb:0x%p", health_node->item.policy.check_cb);
            }
            if (health_node->item.policy.notify_cb) {
                PR_DEBUG("notify_cb:0x%p", health_node->item.policy.notify_cb);
            }
            PR_DEBUG("threshold:%d", health_node->item.policy.threshold);
            PR_DEBUG("detect_period:%d", health_node->item.policy.detect_period);
            node_num++;
        }
    }
    tal_mutex_unlock(s_health_mgr->mutex);
    return;
}

static bool __health_memory_check(void)
{
    // dump all active threads' wartmark
    extern void tal_thread_dump_watermark(void);
    tal_workq_schedule(WORKQ_SYSTEM, (WORKQUEUE_CB)tal_thread_dump_watermark, NULL);

    int free_heap = 0;
    free_heap = tal_system_get_free_heap_size();
    PR_NOTICE("cur free heap: %d", free_heap);
    PR_NOTICE("cur runtime: %ds", (TIME_S)(tal_system_get_millisecond() / 1000));
    if ((free_heap > 0) && (free_heap < HEALTH_FREE_MEM_THRESHOLD)) {
        return TRUE;
    }

    return FALSE;
}

static void __health_memory_notify(void)
{
    PR_DEBUG("health check found reset req!");
    tal_event_publish(EVENT_REBOOT_REQ, NULL);
    return;
}

static bool __health_workq_check(void)
{
    uint16_t workq_num = tal_workq_get_num(WORKQ_SYSTEM);
    PR_NOTICE("cur workq system num: %d", workq_num);
    if (workq_num > HEALTH_WORKQ_THRESHOLD) {
        return TRUE;
    }

    return FALSE;
}

static void __health_workq_notify(void)
{
    tal_workq_dump(WORKQ_SYSTEM);
}

static bool __health_msgq_check(void)
{
    uint16_t workq_num = tal_workq_get_num(WORKQ_HIGHTPRI);
    PR_NOTICE("cur workq highpri num: %d", workq_num);
    if (workq_num > HEALTH_MSGQ_THRESHOLD) {
        return TRUE;
    }

    return FALSE;
}

static void __health_msgq_notify(void)
{
    tal_workq_dump(WORKQ_HIGHTPRI);
}

static bool __health_timeq_check(void)
{
    int timer_num = tal_sw_timer_get_num();
    PR_NOTICE("cur timeq num: %d", timer_num);
    if (timer_num > HEALTH_TIMEQ_THRESHOLD) {
        return TRUE;
    }

    return FALSE;
}

static void __health_foreach_item(void)
{
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if (health_node) {
            health_node->item.detect_time_left -= HEALTH_SLEEP_INTERVAL;
            if (health_node->item.detect_time_left <= 0) {
                health_node->item.detect_time_left = health_node->item.policy.detect_period;
                if (health_node->item.policy.check_cb) { // Query type
                    if (health_node->item.policy.check_cb()) {
                        health_node->item.cnt++;
                        health_node->item.ts = tal_time_get_posix();
                    } else {
                        health_node->item.cnt = 0;
                        health_node->item.ts = 0;
                    }
                }

                if ((health_node->item.cnt >= health_node->item.policy.threshold) &&
                    (health_node->item.policy.notify_cb)) {
                    PR_TRACE("do notify");
                    tal_workq_schedule(WORKQ_SYSTEM, (WORKQUEUE_CB)health_node->item.policy.notify_cb, NULL);
                    health_node->item.cnt = 0;
                    health_node->item.ts = 0;
                }

                if (!health_node->item.policy.check_cb) { // Event type
                    health_node->item.cnt = 0;
                    health_node->item.ts = 0;
                }
            }
        }
    }

    return;
}

static int __health_alert_cb(void *data)
{
    if (NULL == data) {
        PR_ERR("data was null");
        return OPRT_INVALID_PARM;
    }
    health_alert_t *alert = (health_alert_t *)data;

    tal_mutex_lock(s_health_mgr->mutex);
    P_LIST_HEAD pPos, pNext;
    health_node_t *health_node;
    tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
    {
        health_node = tuya_list_entry(pPos, health_node_t, node);
        if ((health_node) && (health_node->item.policy.type == alert->type)) {
            PR_DEBUG("recv evt updata,type:%d", alert->type);
            health_node->item.cnt++;
        }
    }
    tal_mutex_unlock(s_health_mgr->mutex);

    Free(data);

    return OPRT_OK;
}

static void __health_monitor_task(void *arg)
{
    while (1) {
        tal_mutex_lock(s_health_mgr->mutex);
        __health_foreach_item();
        tal_mutex_unlock(s_health_mgr->mutex);
        tal_system_sleep(HEALTH_SLEEP_INTERVAL * 1000);
    }
}

static health_policy_t g_health_policy[] = {
    {HEALTH_RULE_FREE_MEM_SIZE, 1, HEALTH_DETECT_INTERVAL, __health_memory_check, __health_memory_notify},
    {HEALTH_RULE_MAX_MEM_SIZE, 1, HEALTH_DETECT_INTERVAL, NULL, NULL},
    {HEALTH_RULE_ATOP_REFUSE, 5, HEALTH_DETECT_INTERVAL, NULL, NULL},
    {HEALTH_RULE_ATOP_SIGN_FAILED, 5, HEALTH_DETECT_INTERVAL, NULL, NULL},
    {HEALTH_RULE_WORKQ_DEPTH, 1, HEALTH_DETECT_INTERVAL, __health_workq_check, __health_workq_notify},
    {HEALTH_RULE_MSGQ_NUM, 1, HEALTH_DETECT_INTERVAL, __health_msgq_check, __health_msgq_notify},
    {HEALTH_RULE_TIMER_NUM, 1, HEALTH_DETECT_INTERVAL, __health_timeq_check, NULL},
    {HEALTH_RULE_FEED_WATCH_DOG, 0, HEALTH_WATCHDOG_INTERVAL, __watchdog_feed, NULL},
};

static void __health_item_load(void)
{
    int idx = 0;
    for (idx = 0; idx < CNTSOF(g_health_policy); idx++) {
        if (g_health_policy[idx].type != s_health_mgr->global_type) {
            PR_ERR("load item err");
            return;
        }
        tuya_health_item_add(g_health_policy[idx].threshold, g_health_policy[idx].detect_period,
                             g_health_policy[idx].check_cb, g_health_policy[idx].notify_cb);
    }
    return;
}

/**
 * @brief Initializes the health monitor.
 *
 * This function initializes the health monitor by allocating memory for the
 * health manager structure, initializing the list head, creating and
 * subscribing to events, loading health items, initializing and starting the
 * watchdog, creating the health monitor thread, and setting the thread
 * priority.
 *
 * @return Returns OPRT_OK if the health monitor is successfully initialized,
 * otherwise returns an error code.
 */
int tuya_health_monitor_init(void)
{
    if (NULL != s_health_mgr) {
        return OPRT_OK;
    }

    int rt = OPRT_OK;
    s_health_mgr = (health_mgr_t *)Malloc(sizeof(health_mgr_t));
    TUYA_CHECK_NULL_GOTO(s_health_mgr, __exit);
    memset(s_health_mgr, 0, sizeof(health_mgr_t));

    INIT_LIST_HEAD(&s_health_mgr->listHead);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&s_health_mgr->mutex), __exit);
    TUYA_CALL_ERR_GOTO(tal_event_subscribe(EVENT_HEALTH_ALERT, "health_monitor", __health_alert_cb, FALSE), __exit);
    TUYA_CALL_ERR_GOTO(
        tal_event_subscribe(EVENT_REBOOT_ACK, "health_monitor", __health_reboot_cb, SUBSCRIBE_TYPE_NORMAL), __exit);

    __health_item_load();
    // init and start watch dog, use the return value as the real watch dog
    // interval
#if defined(ENABLE_WATCHDOG) && (ENABLE_WATCHDOG == 1)
    int watch_dog_interval = 0;
    watch_dog_interval = __watchdog_init_and_start(HEALTH_WATCHDOG_INTERVAL);
    tuya_health_update_item_period(HEALTH_RULE_FEED_WATCH_DOG, watch_dog_interval / 3);
#endif
    PR_DEBUG("watch_dog_interval:%d, monitor_detect_interval:%d", HEALTH_WATCHDOG_INTERVAL, HEALTH_DETECT_INTERVAL);

    // create health monitor thread with highest priority
    THREAD_CFG_T thrd_param;
    thrd_param.priority = THREAD_PRIO_0;
    thrd_param.stackDepth = STACK_SIZE_HEALTH_MONITOR;
    thrd_param.thrdname = "health_monitor";
    TUYA_CALL_ERR_GOTO(
        tal_thread_create_and_start(&(s_health_mgr->thread), NULL, NULL, __health_monitor_task, NULL, &thrd_param),
        __exit);

    return rt;

__exit:

    PR_ERR("watch_dog_interval:%d, monitor_detect_interval:%d, ret:%d", HEALTH_WATCHDOG_INTERVAL,
           HEALTH_DETECT_INTERVAL, rt);

    if (s_health_mgr) {
        P_LIST_HEAD pPos, pNext;
        health_node_t *health_node;
        tuya_list_for_each_safe(pPos, pNext, &(s_health_mgr->listHead))
        {
            health_node = tuya_list_entry(pPos, health_node_t, node);
            if (health_node) {
                DeleteNodeAndFree(health_node, node);
            }
        }
        if (s_health_mgr->mutex) {
            tal_mutex_release(s_health_mgr->mutex);
            s_health_mgr->mutex = NULL;
        }
        if (s_health_mgr->thread) {
            tal_thread_delete(s_health_mgr->thread);
            s_health_mgr->thread = NULL;
        }
        tal_event_unsubscribe(EVENT_REBOOT_ACK, "health_monitor", __health_reboot_cb);
        tal_event_unsubscribe(EVENT_HEALTH_ALERT, "health_monitor", __health_alert_cb);

        Free(s_health_mgr);
        s_health_mgr = NULL;
    }

    return rt;
}

/**
 * @brief Disables the watchdog for health monitoring.
 *
 * This function stops feeding the watchdog and updates the period of the health
 * item responsible for feeding the watchdog to 0xFFff (65535). If the watchdog
 * is not enabled, an error message is printed.
 */
void tuya_health_disable_watchdog(void)
{
    if (s_health_mgr) {
        PR_NOTICE("watchdog stop feed %d", 0xFFff);
        tuya_health_update_item_period(HEALTH_RULE_FEED_WATCH_DOG, 0xFFff);
    } else {
        PR_ERR("watchdog is not enabled");
    }
}

/**
 * @brief Updates the detection interval for health monitoring.
 *
 * This function updates the detection interval for health monitoring. If the
 * interval provided is less than 60 seconds, it will be set to 60 seconds.
 *
 * @param interval The new detection interval in seconds.
 */
void tuya_health_udpate_detect_interval(int interval)
{
    int monitor_detect_interval = 0;
    if (s_health_mgr) {
        monitor_detect_interval = (interval < 60 ? 60 : interval);
        PR_NOTICE("health monitor interval %ds", monitor_detect_interval);
        int idx = 0;
        for (idx = 0; idx < CNTSOF(g_health_policy); idx++) {
            if ((g_health_policy[idx].type == HEALTH_RULE_FEED_WATCH_DOG) ||
                (g_health_policy[idx].type == HEALTH_RULE_RUNTIME_REPT)) {
                continue;
            }
            tuya_health_update_item_period(g_health_policy[idx].type, monitor_detect_interval);
        }
    } else {
        PR_ERR("health monitor is not enabled");
    }
}
