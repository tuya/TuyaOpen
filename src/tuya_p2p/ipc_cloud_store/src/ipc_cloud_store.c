/**
 * @file ipc_cloud_store.c
 * @brief IPC cloud recording store skeleton
 * @version 0.1
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ipc_cloud_store.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC IPC_CLOUD_STORE_STATE_E s_state = IPC_CLOUD_STORE_IDLE;
STATIC MUTEX_HANDLE s_lock = NULL;
STATIC BOOL_T s_inited = FALSE;

/**
 * @brief Initialize cloud store module
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_init(VOID_T)
{
    OPERATE_RET rt;

    if (s_inited) {
        return OPRT_OK;
    }
    rt = tal_mutex_create_init(&s_lock);
    if (rt != OPRT_OK) {
        return rt;
    }
    s_state = IPC_CLOUD_STORE_IDLE;
    s_inited = TRUE;
    PR_NOTICE("ipc_cloud_store init (skeleton — OSS upload TODO)");
    return OPRT_OK;
}

/**
 * @brief Deinitialize cloud store module
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_deinit(VOID_T)
{
    if (!s_inited) {
        return OPRT_OK;
    }
    (VOID)ipc_cloud_store_stop();
    if (s_lock) {
        tal_mutex_release(s_lock);
        s_lock = NULL;
    }
    s_inited = FALSE;
    return OPRT_OK;
}

/**
 * @brief Start continuous cloud recording
 * @return OPRT_NOT_SUPPORTED until full port
 */
OPERATE_RET ipc_cloud_store_start(VOID_T)
{
    OPERATE_RET rt = ipc_cloud_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_mutex_lock(s_lock);
    s_state = IPC_CLOUD_STORE_RUNNING;
    tal_mutex_unlock(s_lock);
    /* Order/secret/OSS slice path not ported yet — accept start for API stability */
    PR_WARN("ipc_cloud_store_start: running locally but upload not implemented");
    return OPRT_OK;
}

/**
 * @brief Stop continuous cloud recording
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_stop(VOID_T)
{
    if (!s_inited) {
        return OPRT_OK;
    }
    tal_mutex_lock(s_lock);
    s_state = IPC_CLOUD_STORE_IDLE;
    tal_mutex_unlock(s_lock);
    return OPRT_OK;
}

/**
 * @brief Get current cloud store state
 * @return state enum
 */
IPC_CLOUD_STORE_STATE_E ipc_cloud_store_get_state(VOID_T)
{
    return s_state;
}

/**
 * @brief Push video frame (no-op until OSS pipeline lands)
 * @return OPRT_NOT_SUPPORTED when not fully implemented
 */
OPERATE_RET ipc_cloud_store_put_video(CONST UINT8_T *data, UINT32_T len, UINT64_T pts_ms, BOOL_T keyframe)
{
    (VOID)data;
    (VOID)len;
    (VOID)pts_ms;
    (VOID)keyframe;
    if (s_state != IPC_CLOUD_STORE_RUNNING) {
        return OPRT_COM_ERROR;
    }
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Push audio frame (no-op until OSS pipeline lands)
 * @return OPRT_NOT_SUPPORTED when not fully implemented
 */
OPERATE_RET ipc_cloud_store_put_audio(CONST UINT8_T *data, UINT32_T len, UINT64_T pts_ms)
{
    (VOID)data;
    (VOID)len;
    (VOID)pts_ms;
    if (s_state != IPC_CLOUD_STORE_RUNNING) {
        return OPRT_COM_ERROR;
    }
    return OPRT_NOT_SUPPORTED;
}
