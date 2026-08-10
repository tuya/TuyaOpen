/**
 * @file ipc_cloud_store.h
 * @brief IPC cloud recording store API (OpenSDK skeleton, align OS cloud path)
 * @version 0.1
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 *
 * @note Phase-1 skeleton: APIs exist for app/P2P integration. Full OSS/slice
 *       encrypt upload (OS svc_net_storage / cloud_storage) is TODO.
 */
#ifndef __IPC_CLOUD_STORE_H__
#define __IPC_CLOUD_STORE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cloud store run state
 */
typedef enum {
    IPC_CLOUD_STORE_IDLE = 0,
    IPC_CLOUD_STORE_RUNNING = 1,
    IPC_CLOUD_STORE_PAUSED = 2,
} IPC_CLOUD_STORE_STATE_E;

/**
 * @brief Initialize cloud store module
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_init(VOID_T);

/**
 * @brief Deinitialize cloud store module
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_deinit(VOID_T);

/**
 * @brief Start continuous cloud recording (order / secret from cloud)
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED until OSS path is ported
 */
OPERATE_RET ipc_cloud_store_start(VOID_T);

/**
 * @brief Stop continuous cloud recording
 * @return OPRT_OK on success
 */
OPERATE_RET ipc_cloud_store_stop(VOID_T);

/**
 * @brief Get current cloud store state
 * @return state enum
 */
IPC_CLOUD_STORE_STATE_E ipc_cloud_store_get_state(VOID_T);

/**
 * @brief Push one encoded video frame into cloud store pipeline
 * @param[in] data bitstream
 * @param[in] len length
 * @param[in] pts_ms timestamp ms
 * @param[in] keyframe TRUE if I-frame
 * @return OPRT_OK, or OPRT_NOT_SUPPORTED if not running / not implemented
 */
OPERATE_RET ipc_cloud_store_put_video(CONST UINT8_T *data, UINT32_T len, UINT64_T pts_ms, BOOL_T keyframe);

/**
 * @brief Push one encoded audio frame into cloud store pipeline
 * @param[in] data bitstream
 * @param[in] len length
 * @param[in] pts_ms timestamp ms
 * @return OPRT_OK, or OPRT_NOT_SUPPORTED if not running / not implemented
 */
OPERATE_RET ipc_cloud_store_put_audio(CONST UINT8_T *data, UINT32_T len, UINT64_T pts_ms);

#ifdef __cplusplus
}
#endif

#endif /* __IPC_CLOUD_STORE_H__ */
