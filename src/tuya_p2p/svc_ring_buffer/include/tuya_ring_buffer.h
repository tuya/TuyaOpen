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

typedef VOID (*FUNC_REQUEST_KEY_FRAME_CB)(INT_T device, INT_T channel, IPC_STREAM_E stream);

typedef struct {
    UINT_T index;
    MEDIA_FRAME_TYPE_E type;
    UCHAR_T *raw_data;
    UINT_T size;
    UINT64_T pts;
    UINT64_T timestamp;
    UINT_T seq_no;
    UCHAR_T *extra_data;
    UINT_T extra_size;
    UINT_T seq_sync;
} RING_BUFFER_NODE_T;

typedef struct {
    UINT_T bitrate;
    UINT_T fps;
    UINT_T max_buffer_seconds;
    FUNC_REQUEST_KEY_FRAME_CB request_key_frame_cb;
} RING_BUFFER_INIT_PARAM_T;

typedef VOID *RING_BUFFER_USER_HANDLE_T;

/**
 * @brief Initialize one ring buffer for one stream
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] pparam init params
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_init(INT_T device, INT_T channel, IPC_STREAM_E stream,
                                      RING_BUFFER_INIT_PARAM_T *pparam);

/**
 * @brief Uninitialize one ring buffer
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_uninit(INT_T device, INT_T channel, IPC_STREAM_E stream);

/**
 * @brief Open read or write handle
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] open_type read/write
 * @return handle or NULL
 */
RING_BUFFER_USER_HANDLE_T tuya_ipc_ring_buffer_open(INT_T device, INT_T channel, IPC_STREAM_E stream,
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
OPERATE_RET tuya_ipc_ring_buffer_append_data(RING_BUFFER_USER_HANDLE_T handle, UCHAR_T *addr, UINT_T size,
                                             MEDIA_FRAME_TYPE_E type, UINT64_T pts);

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
OPERATE_RET tuya_ipc_ring_buffer_append_data_with_timestamp(RING_BUFFER_USER_HANDLE_T handle, UCHAR_T *addr,
                                                           UINT_T size, MEDIA_FRAME_TYPE_E type, UINT64_T pts,
                                                           UINT64_T timestamp);

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
VOID_T tuya_ipc_ring_buffer_clean_user_state(RING_BUFFER_USER_HANDLE_T handle);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_RING_BUFFER_ */
