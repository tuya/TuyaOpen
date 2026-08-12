/**
 * @file tuya_ring_buffer.h
 * @brief Minimal IPC AV ring buffer (align OS svc_ring_buffer subset)
 * @version 1.0
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef _TUYA_RING_BUFFER_
#define _TUYA_RING_BUFFER_

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_media.h"

typedef enum {
    E_RBUF_READ,
    E_RBUF_WRITE,
} RBUF_OPEN_TYPE_E;

typedef void (*FUNC_REQUEST_KEY_FRAME_CB)(int device, int channel, IPC_STREAM_E stream);

typedef struct {
    uint32_t index;
    MEDIA_FRAME_TYPE_E type;
    uint8_t *raw_data;
    uint32_t size;
    uint64_t pts;
    uint64_t timestamp;
    uint32_t seq_no;
    uint8_t *extra_data;
    uint32_t extra_size;
    uint32_t seq_sync;
} RING_BUFFER_NODE_T;

typedef struct {
    uint32_t bitrate;
    uint32_t fps;
    uint32_t max_buffer_seconds;
    FUNC_REQUEST_KEY_FRAME_CB request_key_frame_cb;
} RING_BUFFER_INIT_PARAM_T;

typedef void *RING_BUFFER_USER_HANDLE_T;

/**
 * @brief Initialize one ring buffer for one stream
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] pparam init params
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_init(int device, int channel, IPC_STREAM_E stream,
                                      RING_BUFFER_INIT_PARAM_T *pparam);

/**
 * @brief Uninitialize one ring buffer
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_uninit(int device, int channel, IPC_STREAM_E stream);

/**
 * @brief Open read or write handle
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] open_type read/write
 * @return handle or NULL
 */
RING_BUFFER_USER_HANDLE_T tuya_ipc_ring_buffer_open(int device, int channel, IPC_STREAM_E stream,
                                                    RBUF_OPEN_TYPE_E open_type);

/**
 * @brief Close handle
 * @param[in] handle user handle
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_close(RING_BUFFER_USER_HANDLE_T handle);

/**
 * @brief Append one frame (copies payload into ring)
 * @param[in] handle write handle
 * @param[in] addr frame data
 * @param[in] size frame size
 * @param[in] type frame type
 * @param[in] pts timestamp us
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_append_data(RING_BUFFER_USER_HANDLE_T handle, uint8_t *addr, uint32_t size,
                                             MEDIA_FRAME_TYPE_E type, uint64_t pts);

/**
 * @brief Append one frame with ms timestamp
 * @param[in] handle write handle
 * @param[in] addr frame data
 * @param[in] size frame size
 * @param[in] type frame type
 * @param[in] pts timestamp us
 * @param[in] timestamp timestamp ms
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_append_data_with_timestamp(RING_BUFFER_USER_HANDLE_T handle, uint8_t *addr,
                                                           uint32_t size, MEDIA_FRAME_TYPE_E type, uint64_t pts,
                                                           uint64_t timestamp);

/**
 * @brief Get next frame for reader (jumps to latest when delayed)
 * @param[in] handle read handle
 * @param[in] is_retry unused (keep for API compat)
 * @return node pointer valid until next get/append on same stream, or NULL
 * @note Caller must not free raw_data; owned by ring until overwritten
 */
RING_BUFFER_NODE_T *tuya_ipc_ring_buffer_get_frame(RING_BUFFER_USER_HANDLE_T handle, BOOL_T is_retry);

/**
 * @brief Reset reader to newest frame
 * @param[in] handle read handle
 * @return none
 */
void tuya_ipc_ring_buffer_clean_user_state(RING_BUFFER_USER_HANDLE_T handle);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_RING_BUFFER_ */
