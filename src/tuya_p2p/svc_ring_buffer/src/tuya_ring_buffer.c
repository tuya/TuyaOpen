/**
 * @file tuya_ring_buffer.c
 * @brief Minimal IPC AV ring buffer implementation
 * @version 1.0
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_ring_buffer.h"
#include "tal_mutex.h"
#include "tal_memory.h"
#include "tal_log.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define RBUF_MAX_SLOTS 8
#define RBUF_DEVICE_MAX 1
#define RBUF_CHANNEL_MAX 1
#define RBUF_STREAM_MAX ((INT_T)E_IPC_STREAM_MAX)

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define RBUF_MALLOC(s) tal_psram_malloc(s)
#define RBUF_FREE(p) tal_psram_free(p)
#else
#define RBUF_MALLOC(s) tal_malloc(s)
#define RBUF_FREE(p) tal_free(p)
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    BOOL_T inited;
    UINT_T bitrate_kb;
    UINT_T fps;
    UINT_T max_seconds;
    UINT_T slot_cnt;
    UINT_T max_frame_size;
    UINT_T write_idx;
    UINT_T seq;
    RING_BUFFER_NODE_T nodes[RBUF_MAX_SLOTS];
    MUTEX_HANDLE lock;
} RBUF_STREAM_T;

typedef struct {
    RBUF_STREAM_T *stream;
    RBUF_OPEN_TYPE_E open_type;
    UINT_T read_seq;
} RBUF_USER_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC RBUF_STREAM_T s_streams[RBUF_DEVICE_MAX][RBUF_CHANNEL_MAX][RBUF_STREAM_MAX];

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Resolve stream object
 * @param[in] device device index
 * @param[in] channel channel index
 * @param[in] stream stream id
 * @return stream ptr or NULL
 */
STATIC RBUF_STREAM_T *__rbuf_get(INT_T device, INT_T channel, IPC_STREAM_E stream)
{
    if (device < 0 || device >= RBUF_DEVICE_MAX) {
        return NULL;
    }
    if (channel < 0 || channel >= RBUF_CHANNEL_MAX) {
        return NULL;
    }
    if ((INT_T)stream < 0 || (INT_T)stream >= RBUF_STREAM_MAX) {
        return NULL;
    }
    return &s_streams[device][channel][stream];
}

/**
 * @brief Initialize one ring buffer for one stream
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] pparam init params
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_init(INT_T device, INT_T channel, IPC_STREAM_E stream,
                                      RING_BUFFER_INIT_PARAM_T *pparam)
{
    RBUF_STREAM_T *st;
    UINT_T secs;
    UINT_T i;
    OPERATE_RET rt;

    if (pparam == NULL) {
        return OPRT_INVALID_PARM;
    }
    st = __rbuf_get(device, channel, stream);
    if (st == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (st->inited) {
        return OPRT_OK;
    }

    memset(st, 0, sizeof(*st));
    secs = pparam->max_buffer_seconds;
    if (secs == 0) {
        secs = 2;
    }
    if (secs > 10) {
        secs = 10;
    }
    st->bitrate_kb = (pparam->bitrate > 0) ? pparam->bitrate : 1024;
    st->fps = (pparam->fps > 0) ? pparam->fps : 25;
    st->max_seconds = secs;
    st->slot_cnt = st->fps * secs;
    if (st->slot_cnt < 2) {
        st->slot_cnt = 2;
    }
    if (st->slot_cnt > RBUF_MAX_SLOTS) {
        st->slot_cnt = RBUF_MAX_SLOTS;
    }
    st->max_frame_size = (st->bitrate_kb * 1024 * 3) / 16;
    if (st->max_frame_size < (64 * 1024)) {
        st->max_frame_size = 64 * 1024;
    }
    if (st->max_frame_size > MAX_MEDIA_FRAME_SIZE) {
        st->max_frame_size = MAX_MEDIA_FRAME_SIZE;
    }

    rt = tal_mutex_create_init(&st->lock);
    if (rt != OPRT_OK) {
        return rt;
    }
    for (i = 0; i < st->slot_cnt; i++) {
        st->nodes[i].raw_data = (UCHAR_T *)RBUF_MALLOC(st->max_frame_size);
        if (st->nodes[i].raw_data == NULL) {
            UINT_T j;
            for (j = 0; j < i; j++) {
                RBUF_FREE(st->nodes[j].raw_data);
                st->nodes[j].raw_data = NULL;
            }
            tal_mutex_release(st->lock);
            st->lock = NULL;
            return OPRT_MALLOC_FAILED;
        }
        st->nodes[i].index = i;
    }
    st->inited = TRUE;
    PR_NOTICE("ring_buffer init d=%d c=%d s=%d slots=%u maxf=%u", device, channel, (INT_T)stream, st->slot_cnt,
              st->max_frame_size);
    return OPRT_OK;
}

/**
 * @brief Uninitialize one ring buffer
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_uninit(INT_T device, INT_T channel, IPC_STREAM_E stream)
{
    RBUF_STREAM_T *st = __rbuf_get(device, channel, stream);
    UINT_T i;

    if (st == NULL || !st->inited) {
        return OPRT_OK;
    }
    tal_mutex_lock(st->lock);
    for (i = 0; i < st->slot_cnt; i++) {
        if (st->nodes[i].raw_data != NULL) {
            RBUF_FREE(st->nodes[i].raw_data);
            st->nodes[i].raw_data = NULL;
        }
    }
    tal_mutex_unlock(st->lock);
    tal_mutex_release(st->lock);
    memset(st, 0, sizeof(*st));
    return OPRT_OK;
}

/**
 * @brief Open read or write handle
 * @param[in] device device number
 * @param[in] channel channel number
 * @param[in] stream stream id
 * @param[in] open_type read/write
 * @return handle or NULL
 */
RING_BUFFER_USER_HANDLE_T tuya_ipc_ring_buffer_open(INT_T device, INT_T channel, IPC_STREAM_E stream,
                                                    RBUF_OPEN_TYPE_E open_type)
{
    RBUF_STREAM_T *st = __rbuf_get(device, channel, stream);
    RBUF_USER_T *user;

    if (st == NULL || !st->inited) {
        return NULL;
    }
    user = (RBUF_USER_T *)tal_malloc(sizeof(RBUF_USER_T));
    if (user == NULL) {
        return NULL;
    }
    memset(user, 0, sizeof(*user));
    user->stream = st;
    user->open_type = open_type;
    user->read_seq = 0;
    return (RING_BUFFER_USER_HANDLE_T)user;
}

/**
 * @brief Close handle
 * @param[in] handle user handle
 * @return OPRT_OK on success
 */
OPERATE_RET tuya_ipc_ring_buffer_close(RING_BUFFER_USER_HANDLE_T handle)
{
    if (handle == NULL) {
        return OPRT_INVALID_PARM;
    }
    tal_free(handle);
    return OPRT_OK;
}

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
                                                           UINT64_T timestamp)
{
    RBUF_USER_T *user = (RBUF_USER_T *)handle;
    RBUF_STREAM_T *st;
    RING_BUFFER_NODE_T *node;

    if (user == NULL || user->open_type != E_RBUF_WRITE || addr == NULL || size == 0) {
        return OPRT_INVALID_PARM;
    }
    st = user->stream;
    if (st == NULL || !st->inited) {
        return OPRT_COM_ERROR;
    }
    if (size > st->max_frame_size) {
        PR_ERR("ring append too large %u > %u", size, st->max_frame_size);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(st->lock);
    node = &st->nodes[st->write_idx % st->slot_cnt];
    memcpy(node->raw_data, addr, size);
    node->size = size;
    node->type = type;
    node->pts = pts;
    node->timestamp = timestamp;
    node->extra_data = NULL;
    node->extra_size = 0;
    st->seq++;
    if (st->seq == 0) {
        st->seq = 1;
    }
    node->seq_no = st->seq;
    node->seq_sync = st->seq;
    st->write_idx = (st->write_idx + 1) % st->slot_cnt;
    tal_mutex_unlock(st->lock);
    return OPRT_OK;
}

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
                                             MEDIA_FRAME_TYPE_E type, UINT64_T pts)
{
    UINT64_T ts_ms = pts / 1000ULL;
    return tuya_ipc_ring_buffer_append_data_with_timestamp(handle, addr, size, type, pts, ts_ms);
}

/**
 * @brief Get next frame for reader
 * @param[in] handle read handle
 * @param[in] is_retry unused
 * @return node or NULL
 */
RING_BUFFER_NODE_T *tuya_ipc_ring_buffer_get_frame(RING_BUFFER_USER_HANDLE_T handle, BOOL_T is_retry)
{
    RBUF_USER_T *user = (RBUF_USER_T *)handle;
    RBUF_STREAM_T *st;
    RING_BUFFER_NODE_T *best = NULL;
    UINT_T i;
    UINT_T newest_seq = 0;

    (VOID)is_retry;
    if (user == NULL || user->open_type != E_RBUF_READ) {
        return NULL;
    }
    st = user->stream;
    if (st == NULL || !st->inited) {
        return NULL;
    }

    tal_mutex_lock(st->lock);
    for (i = 0; i < st->slot_cnt; i++) {
        RING_BUFFER_NODE_T *n = &st->nodes[i];
        if (n->seq_no == 0 || n->size == 0) {
            continue;
        }
        if (n->seq_no > newest_seq) {
            newest_seq = n->seq_no;
        }
        if (n->seq_no > user->read_seq) {
            if (best == NULL || n->seq_no < best->seq_no) {
                best = n;
            }
        }
    }
    if (best != NULL && newest_seq > 0 && (newest_seq - best->seq_no) > (st->slot_cnt / 2)) {
        for (i = 0; i < st->slot_cnt; i++) {
            if (st->nodes[i].seq_no == newest_seq) {
                best = &st->nodes[i];
                break;
            }
        }
    }
    if (best != NULL) {
        user->read_seq = best->seq_no;
    }
    tal_mutex_unlock(st->lock);
    return best;
}

/**
 * @brief Reset reader to newest frame
 * @param[in] handle read handle
 * @return none
 */
VOID_T tuya_ipc_ring_buffer_clean_user_state(RING_BUFFER_USER_HANDLE_T handle)
{
    RBUF_USER_T *user = (RBUF_USER_T *)handle;
    if (user == NULL) {
        return;
    }
    user->read_seq = 0;
}
