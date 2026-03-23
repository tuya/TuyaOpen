/**
 * @file tuya_device_timer.c
 * @brief Device timer task sync: full/incremental sync, persist, ACK, periodic execute.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_device_timer.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_sw_timer.h"
#include "tal_time_service.h"
#include "tal_workq_service.h"
#include "tuya_mqtt_dispatch.h"
#include "tal_api.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "mqtt_service.h"
#include "tuya_slist.h"
#include "tal_hash.h"

#include "cJSON.h"
#include "tal_event.h"
#include "tal_event_info.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>


/***********************************************************
************************macro define************************
***********************************************************/
#define TIMER_TASKS_KV_KEY       "device_timer_tasks"
#define FULL_SYNC_CHECK_TIMER_MS (15 * 1000)
#define KV_WRITE_DELAY_MS        (30 * 1000)
#define TIMER_CHECK_TIMER_MS     (30 * 1000)

#define REQ_ID_LEN               32
#define TIMER_DATE_LEN           16
#define TIMER_TIME_LEN           6

/* TIMER response error codes */
#define ERR_EXPIRD "F2001" // expired
#define ERR_OTHER  "F2100" // other error

typedef uint8_t TIMER_OPERATE_TYPE_E;
#define TIMER_OPERATE_TYPE_ADD    1    // incremental (add/update/delete)
#define TIMER_OPERATE_TYPE_FULL   2    // full sync (add/update)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    SLIST_HEAD node;

    char id[REQ_ID_LEN];
    char date[TIMER_DATE_LEN];
    char time[TIMER_TIME_LEN];
    uint8_t loops;
    TIME_T modify_time;
    uint8_t is_deleted;
    cJSON *dps;

    uint8_t is_executed;
} tuya_timer_item_t;

typedef struct {
    SLIST_HEAD head;

    MUTEX_HANDLE mutex;
    uint16_t timer_items_count;
    char last_task_id[REQ_ID_LEN];
} tuya_timer_task_list_t;

typedef struct {
    SLIST_HEAD node;
    char req_id[REQ_ID_LEN];
} pending_ack_item_t;

typedef struct {
    uint8_t inited;

    // full sync
    uint8_t need_full_sync;
    uint8_t full_sync_in_progress;
    TIMER_ID full_sync_timer_id;
    int full_sync_total;          // total timer count expected from cloud during full sync
    int full_sync_received_count; // cumulative count received so far during full sync

    // kv write delay timer id
    TIMER_ID kv_timer_id;
    SLIST_HEAD pending_ack_list; // pending ack list
    MUTEX_HANDLE pending_ack_mutex;

    // timer check timer id
    TIMER_ID timer_check_timer_id;

    tuya_timer_task_list_t timer_task_list;
} timer_task_ctx_t;

/***********************************************************
***********************variable define**********************
***********************************************************/
static timer_task_ctx_t s_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Format loops bitmask into readable weekday string
 *        e.g. loops=0x7E -> "Mon,Tue,Wed,Thu,Fri,Sat"
 *             loops=0    -> "" (one-shot timer, no weekday info)
 *
 * @param loops   weekday bitmask (bit6=Sun, bit5=Mon, ... bit0=Sat)
 * @param buf     output buffer
 * @param buf_len buffer size (recommend >= 32)
 */
static void __format_loops_str(uint8_t loops, char *buf, size_t buf_len)
{
    static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    buf[0] = '\0';
    if (loops == 0) {
        return;
    }
    if (loops == 0x7F) {
        strncpy(buf, "Everyday", buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }
    /* Mon-Fri = bits 5,4,3,2,1 = 0x3E */
    if ((loops & 0x7F) == 0x3E) {
        strncpy(buf, "Weekdays", buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }
    /* Sat,Sun = bits 6,0 = 0x41 */
    if ((loops & 0x7F) == 0x41) {
        strncpy(buf, "Weekends", buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }

    size_t offset = 0;
    for (int i = 0; i < 7; i++) {
        if (loops & (1 << (6 - i))) {
            if (offset > 0 && offset < buf_len - 1) {
                buf[offset++] = ',';
            }
            size_t name_len = strlen(day_names[i]);
            if (offset + name_len < buf_len) {
                memcpy(buf + offset, day_names[i], name_len);
                offset += name_len;
            }
        }
    }
    buf[offset] = '\0';
}

/**
 * @brief Format dps JSON into a concise action string
 *        e.g. {"1":true,"2":25} -> "dp1=true, dp2=25"
 *
 * @param dps     cJSON object of dps
 * @param buf     output buffer
 * @param buf_len buffer size (recommend >= 128)
 */
static void __format_dps_str(const cJSON *dps, char *buf, size_t buf_len)
{
    buf[0] = '\0';
    if (dps == NULL) {
        strncpy(buf, "(none)", buf_len - 1);
        buf[buf_len - 1] = '\0';
        return;
    }

    size_t offset = 0;
    cJSON *dp = NULL;
    cJSON_ArrayForEach(dp, dps) {
        if (offset > 0 && offset < buf_len - 2) {
            offset += snprintf(buf + offset, buf_len - offset, ", ");
        }
        if (cJSON_IsBool(dp)) {
            offset += snprintf(buf + offset, buf_len - offset, "dp%s=%s",
                               dp->string, cJSON_IsTrue(dp) ? "true" : "false");
        } else if (cJSON_IsNumber(dp)) {
            offset += snprintf(buf + offset, buf_len - offset, "dp%s=%d",
                               dp->string, dp->valueint);
        } else if (cJSON_IsString(dp)) {
            offset += snprintf(buf + offset, buf_len - offset, "dp%s=\"%s\"",
                               dp->string, dp->valuestring);
        } else {
            char *val = cJSON_PrintUnformatted(dp);
            if (val) {
                offset += snprintf(buf + offset, buf_len - offset, "dp%s=%s",
                                   dp->string, val);
                cJSON_free(val);
            }
        }
        if (offset >= buf_len) break;
    }
}

static OPERATE_RET __timer_item_from_json(const cJSON *obj, tuya_timer_item_t **p_item);
static cJSON *__timer_item_to_json(const tuya_timer_item_t *item);
static OPERATE_RET __timer_list_serialize(const tuya_timer_task_list_t *list, char **out);
static OPERATE_RET __timer_list_deserialize(const char *json_str, tuya_timer_task_list_t *list);

static OPERATE_RET __timer_item_del(const char *id);
static tuya_timer_item_t *__timer_item_find(const char *id);
static OPERATE_RET __timer_item_partial_update_from_json(tuya_timer_item_t *item, const cJSON *obj);

static void __timer_list_clear(void);

static void __send_timer_sync_ack(char *req_id, const char *error_code)
{
    tuya_mqtt_context_t *mqctx = &tuya_iot_client_get()->mqctx;

    if (mqctx == NULL) {
        PR_ERR("mqctx is null");
        return;
    }

#define MQTT_EVENT_ACK_BUFFER_LEN 128
    char *ack_buffer = Malloc(MQTT_EVENT_ACK_BUFFER_LEN);
    if (ack_buffer == NULL) {
        PR_ERR("malloc failed");
        return;
    }
    memset(ack_buffer, 0, MQTT_EVENT_ACK_BUFFER_LEN);

    if (error_code == NULL) {
        snprintf(ack_buffer, MQTT_EVENT_ACK_BUFFER_LEN, "{\"reqType\":\"timer_sync\",\"reqId\":\"%s\",\"success\":true}", req_id);
    } else {
        snprintf(ack_buffer, MQTT_EVENT_ACK_BUFFER_LEN, "{\"reqType\":\"timer_sync\",\"reqId\":\"%s\",\"success\":false,\"errorCode\":\"%s\"}", req_id, error_code);
    }

    // PR_DEBUG("ack_buffer:%s", ack_buffer);

    tuya_mqtt_protocol_data_publish_common(mqctx, PRO_IOT_DA_RESP, (uint8_t *)ack_buffer, strlen(ack_buffer), NULL, NULL, 3000, false);

    Free(ack_buffer);
}

/**
 * @brief Check if a req_id already exists in the pending ACK list.
 *        Caller must hold s_ctx.pending_ack_mutex.
 */
static uint8_t __pending_ack_list_has(const char *req_id)
{
    SLIST_HEAD *pos = NULL;
    pending_ack_item_t *ack_item = NULL;

    SLIST_FOR_EACH_ENTRY(ack_item, pending_ack_item_t, pos, &s_ctx.pending_ack_list, node) {
        if (strncmp(ack_item->req_id, req_id, REQ_ID_LEN) == 0) {
            return 1;
        }
    }
    return 0;
}

static void __pending_ack_list_flush(const char *error_code)
{
    SLIST_HEAD *pos = NULL;
    SLIST_HEAD *n = NULL;
    pending_ack_item_t *ack_item = NULL;

    tal_mutex_lock(s_ctx.pending_ack_mutex);
    SLIST_FOR_EACH_ENTRY_SAFE(ack_item, pending_ack_item_t, pos, n, &s_ctx.pending_ack_list, node) {
        s_ctx.pending_ack_list.next = n;

        pending_ack_item_t *tmp_ack_item = (pending_ack_item_t *)ack_item;
        // Remove from pending ACK list
        tuya_slist_del(&s_ctx.pending_ack_list, &tmp_ack_item->node);
        // Send ACK
        __send_timer_sync_ack(tmp_ack_item->req_id, error_code);
        // Free memory
        Free(tmp_ack_item);

    }

    s_ctx.pending_ack_list.next = NULL;
    tal_mutex_unlock(s_ctx.pending_ack_mutex);
}

static void __kv_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    // Write to kv
    char *serialized_str = NULL;
    __timer_list_serialize(&s_ctx.timer_task_list, &serialized_str);
    if (serialized_str == NULL) {
        PR_ERR("serialize failed");
        __pending_ack_list_flush(ERR_OTHER);
        return;
    }
    // PR_DEBUG("serialized_str:%s", serialized_str);
    tal_kv_set(TIMER_TASKS_KV_KEY, (const uint8_t *)serialized_str, strlen(serialized_str));
    Free(serialized_str);
    serialized_str = NULL;

    // Send ACK after kv write completes
    __pending_ack_list_flush(NULL);
}

/**
 * @brief Apply cloud timer array to local list (add/update/delete) while holding lock.
 *        Caller must already hold s_ctx.timer_task_list.mutex.
 */
static OPERATE_RET __apply_timer_updates(const cJSON *inner_data)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON *timers_arr = cJSON_GetObjectItem(inner_data, "timers");
    int timers_count = cJSON_GetArraySize(timers_arr);

    for (int i = 0; i < timers_count; i++) {
        cJSON *timer_json = cJSON_GetArrayItem(timers_arr, i);
        if (timer_json == NULL) {
            PR_ERR("timer item[%d] is null", i);
            continue;
        }

        cJSON *jid = cJSON_GetObjectItem(timer_json, "id");
        if (!cJSON_IsString(jid)) {
            PR_ERR("timer item[%d] missing id field", i);
            continue;
        }

        tuya_timer_item_t *existing_item = __timer_item_find(jid->valuestring);
        if (existing_item != NULL) {
            rt = __timer_item_partial_update_from_json(existing_item, timer_json);
            if (rt != OPRT_OK) {
                PR_ERR("timer item[%d] partial update failed: %d", i, rt);
                return rt;
            }
            if (existing_item->is_executed) {
                existing_item->is_executed = 0;
            }
        } else {
            cJSON *jdel = cJSON_GetObjectItem(timer_json, "isDeleted");
            if (cJSON_IsNumber(jdel) && jdel->valueint) {
                PR_WARN("timer item[%d] is deleted and not exist locally, ignore", i);
                continue;
            }

            tuya_timer_item_t *item = NULL;
            rt = __timer_item_from_json(timer_json, &item);
            if (rt != OPRT_OK || item == NULL) {
                PR_ERR("timer item[%d] from_json failed: %d", i, rt);
                continue;
            }
            tuya_slist_add_tail(&s_ctx.timer_task_list.head, &item->node);
            s_ctx.timer_task_list.timer_items_count++;
        }
    }

    /* remove deleted items */
    SLIST_HEAD *pos = NULL;
    SLIST_HEAD *n = NULL;
    tuya_timer_item_t *item = NULL;
    SLIST_FOR_EACH_ENTRY_SAFE(item, tuya_timer_item_t, pos, n, &s_ctx.timer_task_list.head, node) {
        if (item->is_deleted) {
            __timer_item_del(item->id);
        }
    }

    return OPRT_OK;
}

static int __on_timer_sync_callback(cJSON *data, void *user_data)
{
    OPERATE_RET rt = OPRT_OK;

    (void)user_data;

    if (data == NULL) {
        PR_ERR("data is null");
        return OPRT_INVALID_PARM;
    }

    // char *sync_data_str = cJSON_PrintUnformatted(data);
    // if (sync_data_str) {
    //     PR_DEBUG("timer sync data:%s", sync_data_str);
    //     cJSON_free(sync_data_str);
    // }

    if (cJSON_HasObjectItem(data, "reqId") == false) {
        PR_ERR("req_id is null");
        return OPRT_INVALID_PARM;
    }

    char *req_id = cJSON_GetObjectItem(data, "reqId")->valuestring;
    if (req_id == NULL) {
        PR_ERR("req_id is null");
        return OPRT_INVALID_PARM;
    }

    if (s_ctx.inited == false) {
        PR_ERR("Device timer not initialized");
        __send_timer_sync_ack(req_id, ERR_OTHER);
        return OPRT_INVALID_PARM;
    }

    cJSON *inner_data = cJSON_GetObjectItem(data, "data");
    if (inner_data == NULL) {
        PR_ERR("inner_data is null");
        __send_timer_sync_ack(req_id, ERR_OTHER);
        return OPRT_INVALID_PARM;
    }

    int count = cJSON_GetObjectItem(inner_data, "count")->valueint;
    int operateType = cJSON_GetObjectItem(inner_data, "operateType")->valueint;
    char *task_id = cJSON_GetObjectItem(inner_data, "taskId")->valuestring;

    /* total: expected total timer count for full sync (may span multiple batches) */
    cJSON *jtotal = cJSON_GetObjectItem(inner_data, "total");
    int total = cJSON_IsNumber(jtotal) ? jtotal->valueint : count;

    if (NULL != task_id) {
        memset(s_ctx.timer_task_list.last_task_id, 0, REQ_ID_LEN);
        strncpy(s_ctx.timer_task_list.last_task_id, task_id, REQ_ID_LEN - 1);
    }

    /* verify timers array count matches declared count */
    int timers_count = cJSON_GetArraySize(cJSON_GetObjectItem(inner_data, "timers"));
    if (timers_count != count) {
        PR_ERR("timer sync failed, timers_count:%d != count:%d", timers_count, count);
        __send_timer_sync_ack(req_id, ERR_OTHER);
        tuya_device_timer_full_sync_req();
        return OPRT_COM_ERROR;
    }

    /* enqueue ACK (skip duplicates), start kv write delay timer */
    pending_ack_item_t *ack_item = (pending_ack_item_t *)Malloc(sizeof(pending_ack_item_t));
    if (ack_item != NULL) {
        memset(ack_item, 0, sizeof(pending_ack_item_t));
        strncpy(ack_item->req_id, req_id, REQ_ID_LEN - 1);
        tal_mutex_lock(s_ctx.pending_ack_mutex);
        if (__pending_ack_list_has(req_id)) {
            Free(ack_item);
        } else {
            tuya_slist_add_tail(&s_ctx.pending_ack_list, &ack_item->node);
        }
        tal_mutex_unlock(s_ctx.pending_ack_mutex);
    }
    if (s_ctx.kv_timer_id == NULL) {
        tal_sw_timer_create(__kv_timer_cb, NULL, &s_ctx.kv_timer_id);
    }
    if (!tal_sw_timer_is_running(s_ctx.kv_timer_id)) {
        tal_sw_timer_start(s_ctx.kv_timer_id, KV_WRITE_DELAY_MS, TAL_TIMER_ONCE);
    }

    if (s_ctx.full_sync_in_progress && operateType != TIMER_OPERATE_TYPE_FULL) {
        return OPRT_OK;
    }

    /* full sync: track total and cumulative received count */
    if (operateType == TIMER_OPERATE_TYPE_FULL && s_ctx.full_sync_in_progress) {
        s_ctx.full_sync_total = total;
        s_ctx.full_sync_received_count += count;
    }

    /* apply timer updates under mutex */
    tal_mutex_lock(s_ctx.timer_task_list.mutex);
    rt = __apply_timer_updates(inner_data);
    tal_mutex_unlock(s_ctx.timer_task_list.mutex);

    /* error recovery: stop kv timer, flush pending ACKs with error, request full sync (all outside mutex) */
    if (rt != OPRT_OK) {
        tal_sw_timer_stop(s_ctx.kv_timer_id);
        __pending_ack_list_flush(ERR_OTHER);
        s_ctx.full_sync_in_progress = 0;
        s_ctx.full_sync_received_count = 0;
        s_ctx.full_sync_total = 0;
        tuya_device_timer_full_sync_req();
    }

    /* full sync complete: all batches received (total=0 means no timers, also valid) */
    if (operateType == TIMER_OPERATE_TYPE_FULL && s_ctx.full_sync_in_progress &&
        s_ctx.full_sync_received_count >= s_ctx.full_sync_total) {
        PR_NOTICE("full sync complete: received %d/%d", s_ctx.full_sync_received_count, s_ctx.full_sync_total);
        s_ctx.full_sync_in_progress = 0;
        s_ctx.full_sync_received_count = 0;
        s_ctx.full_sync_total = 0;
        if (s_ctx.full_sync_timer_id != NULL) {
            tal_sw_timer_stop(s_ctx.full_sync_timer_id);
        }
    }

    // tuya_device_timer_dump();

    return rt;
}

static int __on_timer_full_syn_ack_callback(cJSON *data, void *user_data)
{
    (void)data;
    (void)user_data;

    return OPRT_OK;
}

// Check if the item needs to be executed
static uint8_t __check_timer_item(const tuya_timer_item_t *item, POSIX_TM_S *current_time)
{
    if (item == NULL || current_time == NULL) {
        return 0;
    }

    if (item->is_deleted || item->is_executed) {
        return 0;
    }

    // check date: one-shot timer has specific date, recurring timer has "00000000"
    char now_date[TIMER_DATE_LEN] = {0};
    snprintf(now_date, TIMER_DATE_LEN, "%04d%02d%02d", (current_time->tm_year + 1900) % 10000, (current_time->tm_mon + 1) % 100, current_time->tm_mday % 100);

    if ((strncmp(item->date, now_date, TIMER_DATE_LEN) != 0) && \
        (strncmp(item->date, "00000000", TIMER_DATE_LEN) != 0)) {
        return 0;
    }

    // check time
    char now_time[TIMER_TIME_LEN] = {0};
    snprintf(now_time, TIMER_TIME_LEN, "%02d:%02d", current_time->tm_hour, current_time->tm_min);

    if (strncmp(item->time, now_time, TIMER_TIME_LEN) != 0) {
        return 0;
    }

    // check weekday for recurring timers
    // loops bitmask: bit6=Sun, bit5=Mon, ..., bit0=Sat
    // loops == 0: one-shot timer, execute once
    if (item->loops == 0) {
        return 1;
    }

    if (item->loops & (1 << (6 - current_time->tm_wday))) {
        return 1;
    }

    return 0;
}

static void __timer_check_timer_cb(TIMER_ID timer_id, void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    (void)timer_id;
    (void)arg;

    if (s_ctx.inited == false || s_ctx.timer_task_list.mutex == NULL) {
        return;
    }

    if (tal_time_check_time_sync() != OPRT_OK) {
        return;
    }

    if (tuya_slist_empty(&s_ctx.timer_task_list.head)) {
        return;
    }

    POSIX_TM_S current_time = {0};
    rt = tal_time_get(&current_time);
    if (rt != OPRT_OK) {
        return;
    }

    tal_mutex_lock(s_ctx.timer_task_list.mutex);



    SLIST_HEAD *pos = NULL;
    tuya_timer_item_t *item = NULL;
    SLIST_FOR_EACH_ENTRY(item, tuya_timer_item_t, pos, &s_ctx.timer_task_list.head, node) {
        if (__check_timer_item(item, &current_time)) {
            item->is_executed = 1;

            char loops_str[32] = {0};
            char dps_str[128] = {0};
            __format_loops_str(item->loops, loops_str, sizeof(loops_str));
            __format_dps_str(item->dps, dps_str, sizeof(dps_str));

            char now_str[32] = {0};
            snprintf(now_str, sizeof(now_str), "%04d-%02d-%02d %02d:%02d:%02d",
                        (current_time.tm_year + 1900) % 10000, (current_time.tm_mon + 1) % 100, current_time.tm_mday % 100,
                        current_time.tm_hour % 24, current_time.tm_min % 60, current_time.tm_sec % 60);

            if (item->loops == 0) {
                /* one-shot timer: show date */
                char fmt_date[16] = {0};
                if (strlen(item->date) == 8) {
                    snprintf(fmt_date, sizeof(fmt_date), "%.4s-%.2s-%.2s",
                             item->date, item->date + 4, item->date + 6);
                }
                PR_NOTICE("#%s  [NOW: %s]  [ONCE: %s %s]  >> Action: %s",
                          item->id, now_str, fmt_date, item->time, dps_str);
            } else {
                /* recurring timer: show weekdays */
                PR_NOTICE("#%s  [NOW: %s]  [RECURRING: %s %s]  >> Action: %s",
                          item->id, now_str, loops_str, item->time, dps_str);
            }

            cJSON *cmd_js = cJSON_CreateObject();
            if (cmd_js == NULL) {
                PR_ERR("cJSON_CreateObject failed");
                continue;
            }
            cJSON_AddStringToObject(cmd_js, "devId", tuya_iot_client_get()->activate.devid);
            cJSON_AddItemToObject(cmd_js, "dps", cJSON_Duplicate(item->dps, 1));
            // cmd_js format example: {"devId":"xxxx","dps":{"1":true,"2":25}}
            // cmd_js will be freed after being used by tuya_iot_dp_parse, 
            // so there is no need to free it here.
            rt = tuya_iot_dp_parse(tuya_iot_client_get(), DP_CMD_TIMER, cmd_js);
            if (rt != OPRT_OK) {
                PR_ERR("execute timer item failed: %d", rt);
                cJSON_Delete(cmd_js);
                continue;
            }
        }

        if (item->is_executed && item->is_deleted == 0) {
            POSIX_TM_S posix_tm = {0};
            rt = tal_time_get(&posix_tm);
            if (rt != OPRT_OK) {
                continue;
            }

            char now_time[TIMER_TIME_LEN] = {0};
            snprintf(now_time, TIMER_TIME_LEN, "%02d:%02d", posix_tm.tm_hour, posix_tm.tm_min);

            if (strncmp(item->time, now_time, TIMER_TIME_LEN) != 0) {
                item->is_executed = 0;
            }
        }
    }

    tal_mutex_unlock(s_ctx.timer_task_list.mutex);
}

static int __device_timer_task_event_cb(void *data)
{
    (void)data;

    if (s_ctx.need_full_sync) {
        tuya_device_timer_full_sync_req();
    }

    if (s_ctx.timer_check_timer_id == NULL) {
        tal_sw_timer_create(__timer_check_timer_cb, NULL, &s_ctx.timer_check_timer_id);
    }
    tal_sw_timer_start(s_ctx.timer_check_timer_id, TIMER_CHECK_TIMER_MS, TAL_TIMER_CYCLE);

    return 0;
}

static tuya_timer_item_t *__timer_item_find(const char *id)
{
    SLIST_HEAD *pos = NULL;
    tuya_timer_item_t *item = NULL;

    SLIST_FOR_EACH_ENTRY(item, tuya_timer_item_t, pos, &s_ctx.timer_task_list.head, node) {
        if (strncmp(item->id, id, REQ_ID_LEN) == 0) {
            return item;
        }
    }

    return NULL;
}

__attribute__((unused))
static OPERATE_RET __timer_item_add(const char *id, const char *date, const char *time, uint8_t loops,
                                    TIME_T modify_time, uint8_t is_deleted, cJSON *dps)
{
    tuya_timer_item_t *item = (tuya_timer_item_t *)Malloc(sizeof(tuya_timer_item_t));
    if (item == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(item, 0, sizeof(tuya_timer_item_t));

    strncpy(item->id, id, REQ_ID_LEN - 1);
    if (date) {
        strncpy(item->date, date, TIMER_DATE_LEN - 1);
    }
    if (time) {
        strncpy(item->time, time, TIMER_TIME_LEN - 1);
    }
    item->loops = loops;
    item->modify_time = modify_time;
    item->is_deleted = is_deleted;

    if (dps) {
        item->dps = cJSON_Duplicate(dps, 1);
        if (item->dps == NULL) {
            Free(item);
            return OPRT_MALLOC_FAILED;
        }
    }

    tuya_slist_add_tail(&s_ctx.timer_task_list.head, &item->node);
    s_ctx.timer_task_list.timer_items_count++;

    return OPRT_OK;
}

/**
 * @brief Partial update an existing timer item (only overwrite fields present in JSON,
 *        retain original values for absent fields).
 *
 * @param item  pointer to existing timer item
 * @param obj   incremental cJSON object from cloud
 * @return OPRT_OK on success, other values on failure
 */
static OPERATE_RET __timer_item_partial_update_from_json(tuya_timer_item_t *item, const cJSON *obj)
{
    if (item == NULL || obj == NULL) {
        return OPRT_INVALID_PARM;
    }

    cJSON *jdate  = cJSON_GetObjectItem(obj, "date");
    cJSON *jtime  = cJSON_GetObjectItem(obj, "time");
    cJSON *jloops = cJSON_GetObjectItem(obj, "loops");
    cJSON *jdel   = cJSON_GetObjectItem(obj, "isDeleted");
    cJSON *jt     = cJSON_GetObjectItem(obj, "t");
    cJSON *jdps   = cJSON_GetObjectItem(obj, "dps");

    if (cJSON_IsString(jdate)) {
        memset(item->date, 0, TIMER_DATE_LEN);
        strncpy(item->date, jdate->valuestring, TIMER_DATE_LEN - 1);
    }
    if (cJSON_IsString(jtime)) {
        memset(item->time, 0, TIMER_TIME_LEN);
        strncpy(item->time, jtime->valuestring, TIMER_TIME_LEN - 1);
    }
    if (cJSON_IsNumber(jloops)) {
        item->loops = (uint8_t)jloops->valueint;
    }
    if (cJSON_IsNumber(jdel)) {
        item->is_deleted = (uint8_t)jdel->valueint;
    }
    if (cJSON_IsNumber(jt)) {
        item->modify_time = (TIME_T)jt->valuedouble;
    }
    if (jdps != NULL) {
        if (item->dps) {
            cJSON_Delete(item->dps);
        }
        item->dps = cJSON_Duplicate(jdps, 1);
        if (item->dps == NULL) {
            return OPRT_MALLOC_FAILED;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __timer_item_del(const char *id)
{
    tuya_timer_item_t *item = __timer_item_find(id);
    if (item == NULL) {
        return OPRT_COM_ERROR;
    }

    tuya_slist_del(&s_ctx.timer_task_list.head, &item->node);
    s_ctx.timer_task_list.timer_items_count--;

    if (item->dps) {
        cJSON_Delete(item->dps);
    }
    Free(item);

    return OPRT_OK;
}

/**
 * @brief Serialize a single timer item to cJSON object.
 *        Format: {"id","date","dps":{...},"loops","time","isDeleted","t"}
 *
 * @param item  timer item to serialize
 * @return newly created cJSON object, caller is responsible for freeing; returns NULL on failure
 */
static cJSON *__timer_item_to_json(const tuya_timer_item_t *item)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        PR_ERR("cJSON_CreateObject failed for timer item");
        return NULL;
    }

    cJSON_AddStringToObject(obj, "id", item->id);
    cJSON_AddStringToObject(obj, "date", item->date);
    cJSON_AddStringToObject(obj, "time", item->time);
    cJSON_AddNumberToObject(obj, "loops", item->loops);
    cJSON_AddNumberToObject(obj, "isDeleted", item->is_deleted);
    cJSON_AddNumberToObject(obj, "t", (double)item->modify_time);

    /* dps field: item->dps stores the raw dps object directly, e.g. {"1":true} */
    if (item->dps != NULL) {
        cJSON *dps = cJSON_Duplicate(item->dps, 1);
        if (dps == NULL) {
            PR_ERR("timer item id=%s dps duplicate failed", item->id);
            cJSON_Delete(obj);
            return NULL;
        }
        cJSON_AddItemToObject(obj, "dps", dps);
    } else {
        cJSON_AddItemToObject(obj, "dps", cJSON_CreateObject());
    }

    return obj;
}

/**
 * @brief Deserialize a timer item from cJSON object.
 *        The dps field stores the raw dps object directly (e.g. {"1":true}).
 *
 * @param obj     cJSON object containing timer fields
 * @param p_item  output parameter, created internally and returned
 * @return OPRT_OK on success, other values on failure
 */
static OPERATE_RET __timer_item_from_json(const cJSON *obj, tuya_timer_item_t **p_item)
{
    cJSON *jid = cJSON_GetObjectItem(obj, "id");
    cJSON *jdate = cJSON_GetObjectItem(obj, "date");
    cJSON *jtime = cJSON_GetObjectItem(obj, "time");
    cJSON *jloops = cJSON_GetObjectItem(obj, "loops");
    cJSON *jdel = cJSON_GetObjectItem(obj, "isDeleted");
    cJSON *jt = cJSON_GetObjectItem(obj, "t");
    cJSON *jdps = cJSON_GetObjectItem(obj, "dps");

    tuya_timer_item_t *item = (tuya_timer_item_t *)Malloc(sizeof(tuya_timer_item_t));
    if (item == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(item, 0, sizeof(tuya_timer_item_t));

    if (!cJSON_IsString(jid) || !cJSON_IsString(jdate) || !cJSON_IsString(jtime) ||
        !cJSON_IsNumber(jloops) || !cJSON_IsNumber(jdel) || !cJSON_IsNumber(jt)) {
        PR_ERR("timer item json missing required fields");
        Free(item);
        return OPRT_INVALID_PARM;
    }

    strncpy(item->id,   jid->valuestring,   REQ_ID_LEN    - 1);
    strncpy(item->date, jdate->valuestring,  TIMER_DATE_LEN - 1);
    strncpy(item->time, jtime->valuestring,  TIMER_TIME_LEN - 1);
    item->loops       = (uint8_t)jloops->valueint;
    item->is_deleted  = (uint8_t)jdel->valueint;
    item->modify_time = (TIME_T)jt->valuedouble;

    if (jdps != NULL) {
        item->dps = cJSON_Duplicate(jdps, 1);
        if (item->dps == NULL) {
            PR_ERR("timer item id=%s dps duplicate failed", jid->valuestring);
            Free(item);
            return OPRT_MALLOC_FAILED;
        }
    }

    *p_item = item;

    return OPRT_OK;
}

/**
 * @brief Compute SHA256 of data_str, write lowercase hex to hex_out (64 bytes + '\0').
 */
static OPERATE_RET __sha256_hex(const char *data_str, char hex_out[65])
{
    uint8_t digest[32];
    OPERATE_RET rt = tal_sha256_ret((const uint8_t *)data_str, strlen(data_str), digest, 0);
    if (rt != OPRT_OK) {
        PR_ERR("tal_sha256_ret failed: %d", rt);
        return rt;
    }
    for (int i = 0; i < 32; i++) {
        snprintf(hex_out + i * 2, 3, "%02x", digest[i]);
    }
    hex_out[64] = '\0';
    return OPRT_OK;
}

/**
 * @brief Serialize timer_task_list to JSON string.
 *        Format: {"sha256":"...","data":{"last_task_id":"...","timer_items_count":N,"timers":[...]}}
 *
 * @param list  task list to serialize
 * @param out   output JSON string, caller must Free() it
 * @return OPRT_OK on success, other values on failure
 */
static OPERATE_RET __timer_list_serialize(const tuya_timer_task_list_t *list, char **out)
{
    OPERATE_RET rt = OPRT_OK;

    /* Build data object */
    cJSON *data = cJSON_CreateObject();
    if (data == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    cJSON_AddStringToObject(data, "last_task_id", list->last_task_id);
    cJSON_AddNumberToObject(data, "timer_items_count", list->timer_items_count);

    cJSON *timers_arr = cJSON_CreateArray();
    if (timers_arr == NULL) {
        cJSON_Delete(data);
        return OPRT_MALLOC_FAILED;
    }

    SLIST_HEAD *pos = NULL;
    tuya_timer_item_t *item = NULL;
    SLIST_FOR_EACH_ENTRY(item, tuya_timer_item_t, pos, &list->head, node) {
        cJSON *item_json = __timer_item_to_json(item);
        if (item_json == NULL) {
            cJSON_Delete(data);
            return OPRT_COM_ERROR;
        }
        cJSON_AddItemToArray(timers_arr, item_json);
    }
    cJSON_AddItemToObject(data, "timers", timers_arr);

    /* Serialize data to string, compute sha256 */
    char *data_str = cJSON_PrintUnformatted(data);
    if (data_str == NULL) {
        cJSON_Delete(data);
        return OPRT_MALLOC_FAILED;
    }

    char sha256_hex[65];
    rt = __sha256_hex(data_str, sha256_hex);
    if (rt != OPRT_OK) {
        cJSON_free(data_str);
        cJSON_Delete(data);
        return rt;
    }

    /* Assemble final JSON */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        cJSON_free(data_str);
        cJSON_Delete(data);
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(root, "sha256", sha256_hex);
    cJSON_AddItemToObject(root, "data", data);   /* data 所有权转移给 root */
    data = NULL;

    char *result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    cJSON_free(data_str);

    if (result == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    /* Convert to Malloc-managed memory before returning */
    size_t len = strlen(result) + 1;
    *out = (char *)Malloc(len);
    if (*out == NULL) {
        cJSON_free(result);
        return OPRT_MALLOC_FAILED;
    }
    memcpy(*out, result, len);
    cJSON_free(result);

    return OPRT_OK;
}

/**
 * @brief Deserialize from JSON string, populate timer_task_list.
 *        Verifies sha256; the list must be initialized as empty before deserialization.
 *
 * @param json_str  input JSON string
 * @param list      output target, an initialized empty list
 * @return OPRT_OK on success, other values on failure (including sha256 verification failure)
 */
static OPERATE_RET __timer_list_deserialize(const char *json_str, tuya_timer_task_list_t *list)
{
    OPERATE_RET rt = OPRT_OK;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        PR_ERR("timer list json parse failed");
        return OPRT_INVALID_PARM;
    }

    cJSON *jsha256 = cJSON_GetObjectItem(root, "sha256");
    cJSON *jdata   = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsString(jsha256) || !cJSON_IsObject(jdata)) {
        PR_ERR("timer list json missing sha256 or data");
        cJSON_Delete(root);
        return OPRT_INVALID_PARM;
    }

    /* Verify sha256 */
    char *data_str = cJSON_PrintUnformatted(jdata);
    if (data_str == NULL) {
        cJSON_Delete(root);
        return OPRT_MALLOC_FAILED;
    }
    char sha256_hex[65];
    rt = __sha256_hex(data_str, sha256_hex);
    cJSON_free(data_str);
    if (rt != OPRT_OK) {
        cJSON_Delete(root);
        return rt;
    }
    if (strncmp(sha256_hex, jsha256->valuestring, 64) != 0) {
        PR_ERR("timer list sha256 mismatch: expected=%s got=%s",
               jsha256->valuestring, sha256_hex);
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    // PR_DEBUG("timer list sha256 verified: %s", sha256_hex);

    /* Parse data field */
    cJSON *jlast_id = cJSON_GetObjectItem(jdata, "last_task_id");
    if (cJSON_IsString(jlast_id)) {
        strncpy(list->last_task_id, jlast_id->valuestring, REQ_ID_LEN - 1);
    }

    cJSON *jtimers = cJSON_GetObjectItem(jdata, "timers");
    if (!cJSON_IsArray(jtimers)) {
        PR_ERR("timer list json missing timers array");
        cJSON_Delete(root);
        return OPRT_INVALID_PARM;
    }

    int count = cJSON_GetArraySize(jtimers);

    for (int i = 0; i < count; i++) {
        cJSON *jitem = cJSON_GetArrayItem(jtimers, i);
        if (jitem == NULL) {
            continue;
        }

        tuya_timer_item_t *item = NULL;

        rt = __timer_item_from_json(jitem, &item);
        if (rt != OPRT_OK) {
            PR_ERR("timer item[%d] from_json failed: %d", i, rt);
            cJSON_Delete(root);
            return rt;
        }

        tuya_slist_add_tail(&list->head, &item->node);
        list->timer_items_count++;
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

static void __timer_list_clear(void)
{
    SLIST_HEAD *pos = NULL;
    SLIST_HEAD *n = NULL;
    tuya_timer_item_t *item = NULL;

    SLIST_FOR_EACH_ENTRY_SAFE(item, tuya_timer_item_t, pos, n, &s_ctx.timer_task_list.head, node) {
        s_ctx.timer_task_list.head.next = n;
        if (item->dps) {
            cJSON_Delete(item->dps);
        }
        Free(item);
    }

    s_ctx.timer_task_list.head.next = NULL;
    s_ctx.timer_task_list.timer_items_count = 0;
}

int tuya_device_timer_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_ctx.inited) {
        return OPRT_OK;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));

    INIT_SLIST_HEAD(&s_ctx.timer_task_list.head);
    INIT_SLIST_HEAD(&s_ctx.pending_ack_list);

    s_ctx.full_sync_timer_id = NULL;

    rt = tal_mutex_create_init(&s_ctx.timer_task_list.mutex);
    if (rt != OPRT_OK) {
        PR_ERR("timer mutex create failed: %d", rt);
        return rt;
    }

    rt = tal_mutex_create_init(&s_ctx.pending_ack_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("pending ack mutex create failed: %d", rt);
        tal_mutex_release(s_ctx.timer_task_list.mutex);
        s_ctx.timer_task_list.mutex = NULL;
        return rt;
    }

    tuya_mqtt_dispatch_register(PRO_DEV_DA_RESP, "timer_full_syn", "device timer full sync", __on_timer_full_syn_ack_callback, NULL);
    tuya_mqtt_dispatch_register(PRO_IOT_DA_REQ, "timer_sync", "device timer sync", __on_timer_sync_callback, NULL);

    tal_event_subscribe(EVENT_MQTT_CONNECTED, "tuya_device_timer", __device_timer_task_event_cb, SUBSCRIBE_TYPE_ONETIME);

    // Load from kv
    char *kv_str = NULL;
    size_t len = 0;
    rt = tal_kv_get(TIMER_TASKS_KV_KEY, (uint8_t **)&kv_str, &len);
    if (rt == OPRT_OK) {
        tal_mutex_lock(s_ctx.timer_task_list.mutex);
        rt = __timer_list_deserialize(kv_str, &s_ctx.timer_task_list);
        tal_mutex_unlock(s_ctx.timer_task_list.mutex);
        Free(kv_str);
        kv_str = NULL;
        s_ctx.need_full_sync = 1;
    } else {
        s_ctx.need_full_sync = 1;
    }

    s_ctx.inited = true;
     
    // tuya_device_timer_dump();

    return OPRT_OK;
}

void tuya_device_timer_deinit(void)
{
    if (!s_ctx.inited) {
        return;
    }

    tal_event_unsubscribe(EVENT_MQTT_CONNECTED, "tuya_device_timer", __device_timer_task_event_cb);

    tal_mutex_lock(s_ctx.timer_task_list.mutex);
    __timer_list_clear();
    tal_kv_del(TIMER_TASKS_KV_KEY);
    tal_mutex_unlock(s_ctx.timer_task_list.mutex);

    tal_mutex_release(s_ctx.timer_task_list.mutex);
    s_ctx.timer_task_list.mutex = NULL;

    tal_mutex_release(s_ctx.pending_ack_mutex);
    s_ctx.pending_ack_mutex = NULL;

    s_ctx.inited = false;
}

void tuya_device_timer_dump(void)
{
    if (!s_ctx.inited) {
        PR_NOTICE("Device timer not initialized");
        return;
    }

    tal_mutex_lock(s_ctx.timer_task_list.mutex);

    uint16_t total = s_ctx.timer_task_list.timer_items_count;

    PR_NOTICE("========================================================");
    PR_NOTICE("           DEVICE TIMER DUMP  (total: %d)", total);
    PR_NOTICE("========================================================");
    PR_NOTICE("  last_task_id: %s", s_ctx.timer_task_list.last_task_id);
    PR_NOTICE("  full_sync: %s", s_ctx.full_sync_in_progress ? "in progress" : "idle");

    POSIX_TM_S now = {0};
    if (tal_time_get(&now) == OPRT_OK) {
        PR_NOTICE("  current_time: %04d-%02d-%02d %02d:%02d:%02d",
                  now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                  now.tm_hour, now.tm_min, now.tm_sec);
    }

    PR_NOTICE("--------------------------------------------------------");

    if (total == 0) {
        PR_NOTICE("  (no timers)");
        PR_NOTICE("========================================================");
        tal_mutex_unlock(s_ctx.timer_task_list.mutex);
        return;
    }

    SLIST_HEAD *pos = NULL;
    tuya_timer_item_t *item = NULL;
    int idx = 0;

    SLIST_FOR_EACH_ENTRY(item, tuya_timer_item_t, pos, &s_ctx.timer_task_list.head, node) {
        idx++;

        char loops_str[32] = {0};
        char dps_str[128] = {0};
        __format_loops_str(item->loops, loops_str, sizeof(loops_str));
        __format_dps_str(item->dps, dps_str, sizeof(dps_str));

        char type_str[64] = {0};
        if (item->loops == 0) {
            /* one-shot: format date as YYYY-MM-DD */
            char fmt_date[16] = {0};
            if (strlen(item->date) == 8) {
                snprintf(fmt_date, sizeof(fmt_date), "%.4s-%.2s-%.2s",
                         item->date, item->date + 4, item->date + 6);
            } else {
                strncpy(fmt_date, item->date, sizeof(fmt_date) - 1);
            }
            snprintf(type_str, sizeof(type_str), "ONCE: %s", fmt_date);
        } else {
            snprintf(type_str, sizeof(type_str), "RECURRING: %s", loops_str);
        }

        PR_NOTICE("  [%d/%d] #%s", idx, total, item->id);
        PR_NOTICE("        Type   : [%s]", type_str);
        PR_NOTICE("        Time   : %s", item->time);
        PR_NOTICE("        Action : %s", dps_str);
        if (idx < total) {
            PR_NOTICE("  - - - - - - - - - - - - - - - - - - - - - - - - - -");
        }
    }

    PR_NOTICE("========================================================");

    tal_mutex_unlock(s_ctx.timer_task_list.mutex);
}

static void __full_sync_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;

    PR_ERR("full sync timeout (15s), received %d/%d",
           s_ctx.full_sync_received_count, s_ctx.full_sync_total);

    s_ctx.full_sync_in_progress = 0;
    s_ctx.full_sync_received_count = 0;
    s_ctx.full_sync_total = 0;

    /* stop kv timer, flush all pending ACKs with error */
    if (s_ctx.kv_timer_id != NULL && tal_sw_timer_is_running(s_ctx.kv_timer_id)) {
        tal_sw_timer_stop(s_ctx.kv_timer_id);
    }
    __pending_ack_list_flush(ERR_OTHER);

    /* re-request full sync */
    tuya_device_timer_full_sync_req();
}

void tuya_device_timer_full_sync_req(void)
{
    const char buf[] = "{\"reqType\":\"timer_full_syn\"}";

    if (!s_ctx.inited) {
        return;
    }

    tuya_iot_client_t *client = NULL;
    client = tuya_iot_client_get();
    if (client == NULL) {
        return;
    }

    if (tuya_mqtt_protocol_data_publish(&client->mqctx, PRO_DEV_DA_REQ, (const uint8_t *)buf, (uint16_t)sizeof(buf) - 1) != OPRT_OK) {
        PR_ERR("timer full sync req failed");
    } else {
        s_ctx.full_sync_in_progress = 1;
        s_ctx.full_sync_received_count = 0;
        s_ctx.full_sync_total = 0;
        if (s_ctx.full_sync_timer_id == NULL) {
            tal_sw_timer_create(__full_sync_timer_cb, NULL, &s_ctx.full_sync_timer_id);
        }
        tal_sw_timer_start(s_ctx.full_sync_timer_id, FULL_SYNC_CHECK_TIMER_MS, TAL_TIMER_ONCE);

        // Clear all timer items
        tal_mutex_lock(s_ctx.timer_task_list.mutex);
        __timer_list_clear();
        tal_mutex_unlock(s_ctx.timer_task_list.mutex);
    }
}

OPERATE_RET tuya_device_timer_kv_delete(void)
{
    if (!s_ctx.inited) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(s_ctx.timer_task_list.mutex);
    __timer_list_clear();
    OPERATE_RET rt = tal_kv_del(TIMER_TASKS_KV_KEY);
    tal_mutex_unlock(s_ctx.timer_task_list.mutex);

    return rt;
}
