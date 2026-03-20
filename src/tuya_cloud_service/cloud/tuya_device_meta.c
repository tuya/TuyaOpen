/**
 * @file tuya_device_meta.c
 * @brief tuya_device_meta module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_device_meta.h"

#include "atop_base.h"
#include "tal_log.h"
#include "tuya_iot.h"
#include "tal_api.h"

#include "tal_event.h"
#include "tal_event_info.h"

#include "cJSON.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DEVICE_META_SAVE_API    "tuya.device.meta.save"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    cJSON *json;
    MUTEX_HANDLE mutex;
} tuya_device_meta_t;

/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static tuya_device_meta_t s_meta = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static int __device_meta_event_cb(void *data)
{
    (void)data;

    /* Add common default device meta here. Meta for other features may be added
     * after tuya_iot_init() and before tuya_iot_start(). */
    tuya_device_meta_add_number("timerCapability", 1);
    tuya_device_meta_report();

    return 0;
}

OPERATE_RET tuya_device_meta_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_meta.mutex != NULL) {
        return OPRT_OK;
    }

    rt = tal_mutex_create_init(&s_meta.mutex);
    if (OPRT_OK != rt) {
        PR_ERR("mutex create failed:%d", rt);
        return rt;
    }

    tal_event_subscribe(EVENT_MQTT_CONNECTED, "tuya_device_meta", __device_meta_event_cb,
        SUBSCRIBE_TYPE_ONETIME);

    return OPRT_OK;
}

OPERATE_RET tuya_device_meta_deinit(void)
{
    if (s_meta.mutex != NULL) {
        tal_mutex_release(s_meta.mutex);
        s_meta.mutex = NULL;
    }

    if (s_meta.json != NULL) {
        cJSON_Delete(s_meta.json);
        s_meta.json = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tuya_device_meta_add_string(const char *key, const char *value)
{
    OPERATE_RET rt = OPRT_OK;

    if (key == NULL || value == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (s_meta.mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_meta.mutex);

    if (s_meta.json == NULL) {
        s_meta.json = cJSON_CreateObject();
        if (s_meta.json == NULL) {
            rt = OPRT_MALLOC_FAILED;
            goto __EXIT;
        }
        // {"metas":{},"t":0}
        cJSON_AddObjectToObject(s_meta.json, "metas");
        cJSON_AddNumberToObject(s_meta.json, "t", 0);
    }

    cJSON *metas = cJSON_GetObjectItem(s_meta.json, "metas");
    if (metas == NULL) {
        PR_ERR("metas is null");
        rt = OPRT_INVALID_PARM;
        goto __EXIT;
    }

    cJSON *existing = cJSON_GetObjectItem(metas, key);
    if (existing != NULL) {
        PR_WARN("replace meta key:%s value:%s -> %s", key,
                existing->valuestring != NULL ? existing->valuestring : "", value);
        cJSON *new_val = cJSON_CreateString(value);
        if (new_val == NULL) {
            rt = OPRT_MALLOC_FAILED;
            goto __EXIT;
        }
        cJSON_ReplaceItemInObject(metas, key, new_val);
    } else {
        cJSON_AddStringToObject(metas, key, value);
        PR_DEBUG("add meta key:%s value:%s", key, value);
    }

    char *tmp = cJSON_PrintUnformatted(s_meta.json);
    if (tmp) {
        PR_DEBUG("add string meta json:%s", tmp);
        cJSON_free(tmp);
    }

__EXIT:

    tal_mutex_unlock(s_meta.mutex);

    return rt;
}

OPERATE_RET tuya_device_meta_add_number(const char *key, int value)
{
    OPERATE_RET rt = OPRT_OK;

    if (key == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (s_meta.mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_meta.mutex);

    if (s_meta.json == NULL) {
        s_meta.json = cJSON_CreateObject();
        if (s_meta.json == NULL) {
            rt = OPRT_MALLOC_FAILED;
            goto __EXIT;
        }
        // {"metas":{},"t":0}
        cJSON_AddObjectToObject(s_meta.json, "metas");
        cJSON_AddNumberToObject(s_meta.json, "t", 0);
    }

    cJSON *metas = cJSON_GetObjectItem(s_meta.json, "metas");
    if (metas == NULL) {
        PR_ERR("metas is null");
        rt = OPRT_INVALID_PARM;
        goto __EXIT;
    }

    cJSON *existing = cJSON_GetObjectItem(metas, key);
    if (existing != NULL) {
        PR_WARN("replace meta key:%s value:%d -> %d", key,
                (int)existing->valuedouble, value);
        cJSON *new_val = cJSON_CreateNumber((double)value);
        if (new_val == NULL) {
            rt = OPRT_MALLOC_FAILED;
            goto __EXIT;
        }
        cJSON_ReplaceItemInObject(metas, key, new_val);
    } else {
        cJSON_AddNumberToObject(metas, key, (double)value);
        PR_DEBUG("add meta key:%s value:%d", key, value);
    }

    char *tmp = cJSON_PrintUnformatted(s_meta.json);
    if (tmp) {
        PR_DEBUG("add number meta json:%s", tmp);
        cJSON_free(tmp);
    }

__EXIT:

    tal_mutex_unlock(s_meta.mutex);

    return rt;
}

OPERATE_RET tuya_device_meta_report(void)
{
    OPERATE_RET rt = OPRT_OK;
    tuya_iot_client_t *client = NULL;
    char *buffer = NULL;
    cJSON *local_json = NULL;
    atop_base_request_t atop_request = {0};
    atop_base_response_t response = {0};
    bool response_used = false;

    if (s_meta.mutex == NULL || s_meta.json == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_meta.mutex);

    /* 1. Build payload: update timestamp and serialize */
    cJSON *new_time = cJSON_CreateNumber((double)tal_time_get_posix());
    if (new_time == NULL) {
        rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }
    cJSON_ReplaceItemInObject(s_meta.json, "t", new_time);
    buffer = cJSON_PrintUnformatted(s_meta.json);
    if (buffer == NULL) {
        rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }

    PR_DEBUG("meta json:%s", buffer);

    client = tuya_iot_client_get();
    if (client == NULL) {
        rt = OPRT_INVALID_PARM;
        goto __EXIT;
    }

    atop_request.devid = client->activate.devid;
    atop_request.key = client->activate.seckey;
    atop_request.path = "/d.json";
    atop_request.timestamp = (uint32_t)tal_time_get_posix();
    atop_request.api = DEVICE_META_SAVE_API;
    atop_request.version = "1.0";
    atop_request.data = buffer;
    atop_request.datalen = strlen(buffer);
    atop_request.user_data = NULL;

    /* Transfer json to local variable before unlocking, so concurrent
     * tuya_device_meta_add calls create a fresh json and are not lost. */
    local_json = s_meta.json;
    s_meta.json = NULL;

    /* 2. Send request (blocking) */
    response_used = true;
    rt = atop_base_request(&atop_request, &response);
    tal_free(buffer);
    buffer = NULL;

    /* 3. Free the snapshot that was just reported */
    cJSON_Delete(local_json);
    local_json = NULL;

    if (rt != OPRT_OK) {
        PR_ERR("atop_base_request error:%d", rt);
    } else if (!response.success) {
        rt = OPRT_COM_ERROR;
    }

__EXIT:
    if (buffer != NULL) {
        tal_free(buffer);
        buffer = NULL;
    }
    if (local_json != NULL) {
        cJSON_Delete(local_json);
        local_json = NULL;
    }
    if (response_used) {
        atop_base_response_free(&response);
        response_used = false;
    }
    tal_mutex_unlock(s_meta.mutex);

    return rt;
}

OPERATE_RET tuya_device_meta_reset(void)
{
    if (s_meta.mutex == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_meta.mutex);
    if (s_meta.json != NULL) {
        cJSON_Delete(s_meta.json);
        s_meta.json = NULL;
    }
    tal_mutex_unlock(s_meta.mutex);

    return OPRT_OK;
}
