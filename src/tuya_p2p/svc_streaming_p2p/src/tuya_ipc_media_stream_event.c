/**
 * @file tuya_ipc_media_stream_event.c
 * @brief Media stream event register/dispatch (align OS svc_streaming_p2p)
 * @version 1.0
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_ipc_media_stream_event.h"
#include "tal_mutex.h"
#include "tal_log.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC MEDIA_STREAM_EVENT_CB s_event_cb = NULL;
STATIC MUTEX_HANDLE s_event_lock = NULL;

/**
 * @brief Ensure event mutex exists
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __event_lock_init(VOID_T)
{
    if (s_event_lock != NULL) {
        return OPRT_OK;
    }
    return tal_mutex_create_init(&s_event_lock);
}

/**
 * @brief Register media stream event callback
 * @param[in] event_cb application callback (may be NULL to clear)
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_media_stream_register_event_cb(MEDIA_STREAM_EVENT_CB event_cb)
{
    OPERATE_RET rt = __event_lock_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_mutex_lock(s_event_lock);
    s_event_cb = event_cb;
    tal_mutex_unlock(s_event_lock);
    PR_NOTICE("media_stream event_cb %s", event_cb ? "registered" : "cleared");
    return OPRT_OK;
}

/**
 * @brief Dispatch a media stream event to the registered callback
 * @param[in] device device index
 * @param[in] channel channel index
 * @param[in] event event id
 * @param[in] args event-specific payload (may be NULL)
 * @return callback return value, or OPRT_OK if no callback
 */
OPERATE_RET tuya_ipc_media_stream_event_call(INT_T device, INT_T channel, MEDIA_STREAM_EVENT_E event, PVOID_T args)
{
    MEDIA_STREAM_EVENT_CB cb;
    OPERATE_RET rt;

    rt = __event_lock_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_mutex_lock(s_event_lock);
    cb = s_event_cb;
    tal_mutex_unlock(s_event_lock);

    if (cb == NULL) {
        return OPRT_OK;
    }
    return (OPERATE_RET)cb(device, channel, event, args);
}
