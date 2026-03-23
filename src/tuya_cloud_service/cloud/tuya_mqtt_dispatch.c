/**
 * @file tuya_mqtt_dispatch.c
 * @brief tuya_mqtt_dispatch module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_mqtt_dispatch.h"

#include "tuya_iot.h"
#include "tal_api.h"
#include "tuya_slist.h"

#include "tal_event_info.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DISPATCH_NODE_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    SLIST_HEAD node;

    char type_name[DISPATCH_NODE_NAME_MAX_LEN];

    char desc[DISPATCH_NODE_NAME_MAX_LEN];
    tuya_mqtt_dispatch_callback_t callback;
    void *user_data;
} TUYA_MQTT_DISPATCH_NODE_T;

typedef struct {
    SLIST_HEAD head;
    SLIST_HEAD tail;

    uint16_t protocol_id;
    tuya_protocol_callback_t callback;

    MUTEX_HANDLE mutex;
} TUYA_MQTT_DISPATCH_MGR_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static void __mqtt_event_callback(tuya_protocol_event_t *event);

/***********************************************************
***********************variable define**********************
***********************************************************/
static TUYA_MQTT_DISPATCH_MGR_T sg_mqtt_dispatch_mgr[] = {
    {.protocol_id = PRO_DEV_DA_RESP, .callback = __mqtt_event_callback},
    {.protocol_id = PRO_IOT_DA_REQ,  .callback = __mqtt_event_callback},
};

#define DISPATCH_PROTOCOL_CNT (sizeof(sg_mqtt_dispatch_mgr) / sizeof(sg_mqtt_dispatch_mgr[0]))

/***********************************************************
***********************function define**********************
***********************************************************/

static TUYA_MQTT_DISPATCH_NODE_T *__dispatch_node_create(const char *type_name, const char *desc,
                                                         tuya_mqtt_dispatch_callback_t callback, void *user_data)
{
    TUYA_MQTT_DISPATCH_NODE_T *node = (TUYA_MQTT_DISPATCH_NODE_T *)Malloc(sizeof(TUYA_MQTT_DISPATCH_NODE_T));
    if (node == NULL) {
        return NULL;
    }

    memset(node, 0, sizeof(TUYA_MQTT_DISPATCH_NODE_T));
    if (type_name) {
        strncpy(node->type_name, type_name, sizeof(node->type_name) - 1);
    }
    if (desc) {
        strncpy(node->desc, desc, sizeof(node->desc) - 1);
    }
    node->callback  = callback;
    node->user_data = user_data;

    return node;
}

static void __dispatch_node_delete(TUYA_MQTT_DISPATCH_MGR_T *mgr, TUYA_MQTT_DISPATCH_NODE_T *node)
{
    tuya_slist_del(&mgr->head, &node->node);
    Free(node);
}

static void __mqtt_event_workq_callback(void *data)
{
    tuya_protocol_event_t *event = (tuya_protocol_event_t *)data;
    TUYA_MQTT_DISPATCH_MGR_T *mgr = (TUYA_MQTT_DISPATCH_MGR_T *)event->user_data;

    cJSON *req_type = cJSON_GetObjectItem(event->data, "reqType");
    cJSON *inner_data = cJSON_GetObjectItem(event->data, "data");
    cJSON *task_id = inner_data ? cJSON_GetObjectItem(inner_data, "taskId") : NULL;

    PR_DEBUG("mqtt dispatch >> reqType=%s taskId=%s",
             req_type ? req_type->valuestring : "null",
             task_id  ? task_id->valuestring  : "null");

    if (tuya_slist_empty(&mgr->head)) {
        PR_ERR("no dispatch node");
        goto cleanup;
    }

    if (NULL == mgr->mutex) {
        PR_ERR("mutex is null");
        goto cleanup;
    }

    tal_mutex_lock(mgr->mutex);

    // Traverse all nodes and invoke callbacks whose type_name matches reqType
    const char *req_type_str = req_type ? req_type->valuestring : NULL;
    SLIST_HEAD *pos = NULL;
    SLIST_FOR_EACH(pos, &mgr->head)
    {
        TUYA_MQTT_DISPATCH_NODE_T *node = SLIST_ENTRY(pos, TUYA_MQTT_DISPATCH_NODE_T, node);
        if (req_type_str && strncmp(node->type_name, req_type_str, sizeof(node->type_name)) != 0) {
            continue;
        }
        PR_DEBUG("mqtt dispatch >> matched: %s (%s)", node->type_name, node->desc);
        if (node->callback) {
            node->callback(event->data, node->user_data);
        }
    }

    tal_mutex_unlock(mgr->mutex);

    PR_DEBUG("mqtt dispatch << done");

cleanup:
    cJSON_Delete(event->root_json);
    cJSON_Delete(event->data);
    Free(event);
}

static void __mqtt_event_callback(tuya_protocol_event_t *event)
{
    if (event == NULL || event->user_data == NULL) {
        PR_ERR("invalid parameters");
        return;
    }

    // PR_DEBUG("mqtt event callback:%d", event->event_id);

    if (event->data == NULL) {
        PR_ERR("data is null");
        return;
    }

    tuya_protocol_event_t *ev_copy = (tuya_protocol_event_t *)Malloc(sizeof(tuya_protocol_event_t));
    if (ev_copy == NULL) {
        PR_ERR("malloc failed");
        return;
    }

    /* deep copy the event */
    ev_copy->event_id  = event->event_id;
    ev_copy->user_data = event->user_data;
    ev_copy->root_json = event->root_json ? cJSON_Duplicate(event->root_json, 1) : NULL;
    ev_copy->data      = event->data      ? cJSON_Duplicate(event->data,      1) : NULL;

    if (tal_workq_schedule(WORKQ_SYSTEM, __mqtt_event_workq_callback, ev_copy) != OPRT_OK) {
        PR_ERR("tal_workq_schedule failed");
        cJSON_Delete(ev_copy->root_json);
        cJSON_Delete(ev_copy->data);
        Free(ev_copy);
    }
}

static int __mqtt_dispatch_init_cb(void *data)
{
    tuya_mqtt_context_t *mqctx = &tuya_iot_client_get()->mqctx;

    for (int i = 0; i < DISPATCH_PROTOCOL_CNT; i++) {
        TUYA_MQTT_DISPATCH_MGR_T *mgr = &sg_mqtt_dispatch_mgr[i];
        if (mgr == NULL) {
            PR_ERR("invalid parameters");
            return OPRT_INVALID_PARM;
        }
        tuya_mqtt_protocol_register(mqctx, mgr->protocol_id, mgr->callback, mgr);
        PR_DEBUG("tuya_mqtt_dispatch_init register protocol_id:%d", mgr->protocol_id);
    }

    return OPRT_OK;
}

OPERATE_RET tuya_mqtt_dispatch_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    for (int i = 0; i < DISPATCH_PROTOCOL_CNT; i++) {
        TUYA_MQTT_DISPATCH_MGR_T *mgr = &sg_mqtt_dispatch_mgr[i];
        INIT_SLIST_HEAD(&mgr->head);
        INIT_SLIST_HEAD(&mgr->tail);
        rt = tal_mutex_create_init(&mgr->mutex);
        if (rt != OPRT_OK) {
            PR_ERR("tal_mutex_create_init failed:%d", rt);
            tuya_mqtt_dispatch_deinit();
            return OPRT_COM_ERROR;
        }
        PR_DEBUG("tuya_mqtt_dispatch_init success:%d", mgr->protocol_id);
    }

    return tal_event_subscribe(EVENT_MQTT_CONNECTED, "tuya_mqtt_dispatch", __mqtt_dispatch_init_cb,
        SUBSCRIBE_TYPE_ONETIME);
}

OPERATE_RET tuya_mqtt_dispatch_deinit(void)
{
    for (int i = 0; i < DISPATCH_PROTOCOL_CNT; i++) {
        TUYA_MQTT_DISPATCH_MGR_T *mgr = &sg_mqtt_dispatch_mgr[i];

        if (mgr->mutex) {
            tal_mutex_lock(mgr->mutex);
        }
    
        SLIST_HEAD *pos = NULL;
        SLIST_HEAD *n = NULL;
        SLIST_FOR_EACH_SAFE(pos, n, &mgr->head)
        {
            TUYA_MQTT_DISPATCH_NODE_T *node = SLIST_ENTRY(pos, TUYA_MQTT_DISPATCH_NODE_T, node);
            __dispatch_node_delete(mgr, node);
        }

        if (mgr->mutex) {
            tal_mutex_unlock(mgr->mutex);
            tal_mutex_release(mgr->mutex);
            mgr->mutex = NULL;
        }
    }

    return OPRT_OK;
}

OPERATE_RET tuya_mqtt_dispatch_register(uint16_t protocol_id, const char *type_name, const char *desc, tuya_mqtt_dispatch_callback_t callback, void *user_data)
{
    if (callback == NULL || type_name == NULL || desc == NULL) {
        PR_ERR("invalid parameters");
        return OPRT_INVALID_PARM;
    }

    // 1. Find the mgr by protocol_id
    TUYA_MQTT_DISPATCH_MGR_T *mgr = NULL;
    for (int i = 0; i < DISPATCH_PROTOCOL_CNT; i++) {
        if (sg_mqtt_dispatch_mgr[i].protocol_id == protocol_id) {
            mgr = &sg_mqtt_dispatch_mgr[i];
            break;
        }
    }

    if (mgr == NULL) {
        PR_ERR("no mgr found for protocol_id:%d", protocol_id);
        return OPRT_INVALID_PARM;
    }

    // 2. Create a new dispatch node
    TUYA_MQTT_DISPATCH_NODE_T *new_node = __dispatch_node_create(type_name, desc, callback, user_data);
    if (new_node == NULL) {
        PR_ERR("dispatch node create failed");
        return OPRT_MALLOC_FAILED;
    }

    // 3. Append the new node to the tail of mgr
    tal_mutex_lock(mgr->mutex);

    if (tuya_slist_empty(&mgr->head)) {
        // Empty list: head and tail both point to the new node
        mgr->head.next = &new_node->node;
        mgr->tail.next = &new_node->node;
    } else {
        // Non-empty list: append via tail pointer in O(1)
        mgr->tail.next->next = &new_node->node;
        mgr->tail.next = &new_node->node;
    }

    tal_mutex_unlock(mgr->mutex);

    // 4. Log the new node info for debugging
    PR_DEBUG("dispatch register: protocol_id=%d type_name=%s desc=%s", protocol_id, type_name, desc);

    return OPRT_OK;
}
