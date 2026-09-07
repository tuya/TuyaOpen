/**
 * @file tuya_ipc_demo.c
 * @brief IPC demo media callbacks: live camera over P2P, demo file as fallback
 * @version 2.0
 * @date 2026-08-28
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_ipc_demo.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_mutex.h"
#include "tal_thread.h"
#include "tal_memory.h"
#include "tuya_ipc_p2p.h"
#include "demo_media_event.h"
#include <string.h>

extern uint64_t tuya_p2p_misc_get_current_time_ms(void);

#if OPERATING_SYSTEM == SYSTEM_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEMO_FS_IS_MOUNTED 1 /* a real filesystem is already there */

#else

#include "board_com_api.h"
#include "tkl_fs.h"

#define DEMO_FS_IS_MOUNTED 0 /* the SD card has to be mounted first */
#define DEMO_FS_MOUNT      "/sdcard"

#endif

#if DEMO_ENABLE_AUDIO
#include "demo_audio_port.h"
#include "demo_g711.h"
#define DEMO_HAS_AUDIO 1
#else
#define DEMO_HAS_AUDIO 0
#endif

#if !DEMO_FS_IS_MOUNTED && DEMO_HAS_LOCAL_STORE
#define DEMO_NEEDS_FS_MOUNT 1
#else
#define DEMO_NEEDS_FS_MOUNT 0
#endif

#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
#include "tuya_ring_buffer.h"
#define DEMO_HAS_RING_BUFFER 1
#else
#define DEMO_HAS_RING_BUFFER 0
#endif

#if DEMO_HAS_LOCAL_STORE
#include "local_store.h"
#endif

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
#include "tdl_camera_manage.h"
#define DEMO_HAS_CAMERA 1
#else
#define DEMO_HAS_CAMERA 0
#endif

/* Declared once in av_info, never negotiated per frame. */
#if defined(CAMERA_V4L2_H265) && (CAMERA_V4L2_H265 == 1)
#define DEMO_VIDEO_CODEC TY_AV_CODEC_VIDEO_H265
#else
#define DEMO_VIDEO_CODEC TY_AV_CODEC_VIDEO_H264
#endif

#if DEMO_HAS_LOCAL_STORE && defined(CAMERA_DEMO_SD_LIVE_RECORD) && (CAMERA_DEMO_SD_LIVE_RECORD == 1)
#define DEMO_LIVE_RECORD 1
#ifndef CAMERA_DEMO_SD_RECORD_MAX_SEC
#define CAMERA_DEMO_SD_RECORD_MAX_SEC 120
#endif
#else
#define DEMO_LIVE_RECORD 0
#endif

#if !defined(CAMERA_DEMO_WIDTH) || !defined(CAMERA_DEMO_HEIGHT) || !defined(CAMERA_DEMO_FPS) ||                    \
    !defined(CAMERA_DEMO_KBPS)
#error "the board .config must set CONFIG_CAMERA_DEMO_{WIDTH,HEIGHT,FPS,KBPS} - Kconfig carries no default"
#endif

#define DEMO_CAM_WIDTH  CAMERA_DEMO_WIDTH
#define DEMO_CAM_HEIGHT CAMERA_DEMO_HEIGHT
#define DEMO_CAM_FPS    CAMERA_DEMO_FPS
#define DEMO_CAM_KBPS   CAMERA_DEMO_KBPS

#ifndef CAMERA_DEMO_GOP
#define CAMERA_DEMO_GOP 30
#endif
#define DEMO_CAM_GOP CAMERA_DEMO_GOP

#define DEMO_AV_FPS 25

#ifndef CAMERA_NAME
#define CAMERA_NAME "camera"
#endif
/* Boards use either spelling of the name. */
#define CAMERA_NAME_ALT "camera_dvp"

/* Must match p2p_init()'s media_frame buffer; do not read media_frame->size */
#define DEMO_P2P_FRAME_CAP    (300 * 1024)
#define DEMO_FRAME_BUF_SIZE   (256 * 1024)
#define DEMO_FRAME_LOG_PERIOD 30
#define DEMO_P2P_QUEUE_DEPTH 2

#if OPERATING_SYSTEM == SYSTEM_LINUX
#define DEMO_HAS_FILE_PLAYBACK 1
#define DEMO_FILE_PATH         "demo_video.264"
#elif defined(CAMERA_DEMO_P2P_FILE_H264) && (CAMERA_DEMO_P2P_FILE_H264 == 1)
#define DEMO_HAS_FILE_PLAYBACK 1
extern const uint8_t demo_video_264_start[];
extern const uint8_t demo_video_264_end[];
#else
#define DEMO_HAS_FILE_PLAYBACK 0
#endif

#if DEMO_HAS_FILE_PLAYBACK
#define DEMO_FILE_FPS    30
#define DEMO_FILE_WIDTH  320
#define DEMO_FILE_HEIGHT 240
#define DEMO_FILE_GOP    30
#define DEMO_FILE_KBPS   512
#endif

#if DEMO_HAS_AUDIO

#define DEMO_AUDIO_FRAME_MS      20
#define DEMO_AUDIO_FRAME_BYTES   (8000 * DEMO_AUDIO_FRAME_MS / 1000)
#define DEMO_AUDIO_RING_MS       320
#define DEMO_AUDIO_RING_FRAMES   (DEMO_AUDIO_RING_MS / DEMO_AUDIO_FRAME_MS)
#define DEMO_AUDIO_PCM_MAX       1024
#define DEMO_DOWNLINK_G711_MAX   640
#define DEMO_DL_PCM_MS           300
#define DEMO_DL_PCM_CAP          (8000 * DEMO_DL_PCM_MS / 1000)
#define DEMO_DL_PLAY_SAMPLES     (8000 * DEMO_AUDIO_FRAME_MS / 1000)
#endif

typedef struct {
    uint32_t len;
    BOOL_T   is_key;
    uint64_t ts_ms;
} DEMO_P2P_Q_SLOT_T;

static MUTEX_HANDLE        s_frame_mutex = NULL;
static uint8_t            *s_q_pool[DEMO_P2P_QUEUE_DEPTH] = {0};
static DEMO_P2P_Q_SLOT_T   s_q_slot[DEMO_P2P_QUEUE_DEPTH];
static uint32_t            s_q_head = 0;
static uint32_t            s_q_tail = 0;
static uint32_t            s_q_count = 0;
static uint32_t            s_frame_slot_cap = 0;
static BOOL_T              s_media_ready = FALSE;
static BOOL_T              s_live_push_enable = FALSE;
static BOOL_T              s_live_want_cam = FALSE;
static MUTEX_HANDLE        s_cam_lock = NULL;
static BOOL_T              s_queue_need_iframe = FALSE;
static BOOL_T              s_cam_running = FALSE;
#if DEMO_HAS_CAMERA
static TDL_CAMERA_HANDLE_T s_cam = NULL;
#endif
static uint64_t            s_frame_idx = 0;

#if DEMO_HAS_RING_BUFFER
static RING_BUFFER_USER_HANDLE_T s_ring_w = NULL;
static RING_BUFFER_USER_HANDLE_T s_ring_r = NULL;
static BOOL_T                    s_ring_ready = FALSE;
#endif

#if DEMO_HAS_FILE_PLAYBACK
static const uint8_t *s_file_h264 = NULL;
static uint32_t       s_file_size = 0;
static uint32_t       s_file_offset = 0;
static uint32_t       s_file_frame_len = 0;
static uint32_t       s_file_frame_start = 0;
static uint32_t       s_file_is_key = 0;
static uint64_t       s_file_pts_idx = 0;
#if OPERATING_SYSTEM == SYSTEM_LINUX
static uint8_t *s_file_buf = NULL; /* owned here; the embedded blob is not */
#endif
#endif

#if DEMO_HAS_AUDIO
typedef struct {
    uint8_t  data[DEMO_AUDIO_FRAME_BYTES];
    uint64_t ts_ms;
} DEMO_UL_FRAME_T;

static MUTEX_HANDLE    s_audio_mutex = NULL;
static DEMO_UL_FRAME_T s_ul_ring[DEMO_AUDIO_RING_FRAMES];
static uint32_t        s_ul_head = 0, s_ul_tail = 0, s_ul_count = 0;
static uint32_t        s_ul_ovf = 0;
static int16_t         s_dl_pcm[DEMO_DL_PCM_CAP];
static uint32_t        s_dl_head = 0, s_dl_tail = 0, s_dl_count = 0;
static uint32_t        s_dl_ovf = 0;
static BOOL_T          s_audio_inited = FALSE;
static BOOL_T          s_mic_running = FALSE;
static volatile BOOL_T s_spk_active = FALSE;
static volatile BOOL_T s_play_run = FALSE;
static volatile BOOL_T s_play_alive = FALSE;
static THREAD_HANDLE   s_play_th = NULL;

static OPERATE_RET __demo_audio_uplink_init(void);
static void        __demo_audio_uplink_deinit(void);
static OPERATE_RET __demo_mic_start(void);
static void        __demo_mic_stop(void);
static void        __demo_play_stop(void);
#endif

static void __demo_p2p_queue_clear(void)
{
    s_q_head = 0;
    s_q_tail = 0;
    s_q_count = 0;
    s_queue_need_iframe = TRUE;
}

static OPERATE_RET __demo_p2p_queue_push(const uint8_t *data, uint32_t len, BOOL_T is_key, uint64_t ts_ms)
{
    uint8_t *dst;

    if (data == NULL || len == 0 || len > s_frame_slot_cap) {
        return OPRT_INVALID_PARM;
    }
    if (s_q_count >= DEMO_P2P_QUEUE_DEPTH) {
        s_q_head = (s_q_head + 1U) % DEMO_P2P_QUEUE_DEPTH;
        s_q_count--;
        s_queue_need_iframe = TRUE;
    }
    if (s_queue_need_iframe && !is_key) {
        return OPRT_OK;
    }
    dst = s_q_pool[s_q_tail];
    if (dst == NULL) {
        return OPRT_COM_ERROR;
    }
    memcpy(dst, data, len);
    s_q_slot[s_q_tail].len = len;
    s_q_slot[s_q_tail].is_key = is_key;
    s_q_slot[s_q_tail].ts_ms = ts_ms;
    s_q_tail = (s_q_tail + 1U) % DEMO_P2P_QUEUE_DEPTH;
    s_q_count++;
    if (is_key) {
        s_queue_need_iframe = FALSE;
    }
    return OPRT_OK;
}

static OPERATE_RET __demo_p2p_queue_pop(MEDIA_FRAME *media_frame)
{
    const DEMO_P2P_Q_SLOT_T *slot;
    const uint8_t           *src;

    if (media_frame == NULL || media_frame->data == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (s_q_count == 0) {
        return OPRT_NOT_FOUND;
    }
    slot = &s_q_slot[s_q_head];
    src = s_q_pool[s_q_head];
    if (src == NULL || slot->len == 0 || slot->len > DEMO_P2P_FRAME_CAP) {
        __demo_p2p_queue_clear();
        return OPRT_COM_ERROR;
    }
    memcpy(media_frame->data, src, slot->len);
    media_frame->size = slot->len;
    media_frame->type = slot->is_key ? eVideoIFrame : eVideoPBFrame;
    media_frame->pts = slot->ts_ms;
    media_frame->timestamp = (uint32_t)slot->ts_ms;
    s_q_head = (s_q_head + 1U) % DEMO_P2P_QUEUE_DEPTH;
    s_q_count--;
    return OPRT_OK;
}

#if DEMO_LIVE_RECORD
static void __demo_rec_start(void)
{
    OPERATE_RET rt = local_store_rec_start("live", CAMERA_DEMO_SD_RECORD_MAX_SEC);

    if (rt != OPRT_OK) {
        PR_ERR("live rec start failed: %d", rt);
    }
}

static void __demo_rec_stop(void)
{
    (void)local_store_rec_stop();
}
#endif /* DEMO_LIVE_RECORD */

#if DEMO_HAS_CAMERA
static BOOL_T __demo_au_has_annexb(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if (data == NULL || len < 4) {
        return FALSE;
    }
    for (i = 0; i + 3 < len; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
            return TRUE;
        }
        if (i + 4 < len && data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            return TRUE;
        }
    }
    return FALSE;
}

static OPERATE_RET __demo_encoded_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    BOOL_T   is_key;
    uint64_t ts_ms;

    (void)hdl;
    if (frame == NULL || frame->data == NULL || frame->data_len == 0) {
        return OPRT_OK;
    }
    if (s_frame_mutex == NULL || s_frame_slot_cap == 0) {
        return OPRT_OK;
    }
    if (!s_live_push_enable) {
        return OPRT_OK;
    }
    if (frame->data_len > s_frame_slot_cap) {
        PR_WARN("encoded frame too large: %u > %u", (uint32_t)frame->data_len, (uint32_t)s_frame_slot_cap);
        return OPRT_OK;
    }
    if (!__demo_au_has_annexb((const uint8_t *)frame->data, frame->data_len)) {
        static uint32_t s_bad_au_cnt = 0;
        if ((s_bad_au_cnt++ % 30) == 0) {
            PR_NOTICE("drop AU without Annex-B start code len=%u cnt=%u", (uint32_t)frame->data_len, s_bad_au_cnt);
        }
        return OPRT_OK;
    }
    if (!frame->is_complete) {
        static uint32_t s_incomplete_cnt = 0;
        if ((s_incomplete_cnt++ % 30) == 0) {
            PR_DEBUG("drop incomplete AU len=%u total=%u cnt=%u", (uint32_t)frame->data_len,
                     (uint32_t)frame->total_frame_len, s_incomplete_cnt);
        }
        return OPRT_OK;
    }
    if (frame->total_frame_len > 0 && frame->data_len != frame->total_frame_len) {
        static uint32_t s_len_mismatch_cnt = 0;
        if ((s_len_mismatch_cnt++ % 10) == 0) {
            PR_DEBUG("drop len mismatch got=%u expect=%u cnt=%u", (uint32_t)frame->data_len,
                     (uint32_t)frame->total_frame_len, s_len_mismatch_cnt);
        }
        return OPRT_OK;
    }

    is_key = frame->is_i_frame ? TRUE : FALSE;
    ts_ms = tuya_p2p_misc_get_current_time_ms();
    s_frame_idx++;

    if (s_live_push_enable) {
        tal_mutex_lock(s_frame_mutex);
        (void)__demo_p2p_queue_push((const uint8_t *)frame->data, frame->data_len, is_key, ts_ms);
        tal_mutex_unlock(s_frame_mutex);
    }

#if DEMO_HAS_RING_BUFFER
    if (s_live_push_enable && s_ring_ready && s_ring_w != NULL) {
        (void)tuya_ipc_ring_buffer_append_data_with_timestamp(s_ring_w, (uint8_t *)frame->data, frame->data_len,
                                                             is_key ? E_VIDEO_I_FRAME : E_VIDEO_PB_FRAME,
                                                             ts_ms * 1000ULL, ts_ms);
    }
#endif

#if DEMO_LIVE_RECORD
    (void)local_store_rec_write((const uint8_t *)frame->data, frame->data_len, ts_ms, is_key);
#endif

    if (is_key || (s_frame_idx % DEMO_FRAME_LOG_PERIOD) == 0) {
#if DEMO_LIVE_RECORD
        LOCAL_STORE_REC_STAT_T rs;

        local_store_rec_get_stat(&rs);
        PR_NOTICE("enc frames=%llu len=%u i=%u q=%u rec in=%u drop=%u ring=%uK wmax=%ums",
                  (unsigned long long)s_frame_idx, (uint32_t)frame->data_len, (uint32_t)(is_key ? 1 : 0),
                  (uint32_t)s_q_count, rs.frames_in, rs.frames_dropped, rs.ring_used / 1024u, rs.write_max_ms);
#else
        PR_NOTICE("enc frames=%llu len=%u i=%u q=%u", (unsigned long long)s_frame_idx, (uint32_t)frame->data_len,
                  (uint32_t)(is_key ? 1 : 0), (uint32_t)s_q_count);
#endif
    }
    return OPRT_OK;
}

static TDL_CAMERA_HANDLE_T __demo_camera_find(void)
{
    TDL_CAMERA_HANDLE_T hdl = tdl_camera_find_dev((char *)CAMERA_NAME);

    if (hdl == NULL) {
        hdl = tdl_camera_find_dev((char *)CAMERA_NAME_ALT);
    }
    return hdl;
}

static BOOL_T __demo_camera_present(void)
{
    return (__demo_camera_find() != NULL) ? TRUE : FALSE;
}

static OPERATE_RET __demo_camera_open(void)
{
    TDL_CAMERA_CFG_T      cfg;
    TDL_CAMERA_DEV_INFO_T info;
    OPERATE_RET           rt;

    if (s_cam_running) {
        return OPRT_OK;
    }

    s_cam = __demo_camera_find();
    if (s_cam == NULL) {
        PR_WARN("camera '%s' not registered", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    memset(&info, 0, sizeof(info));
    if (tdl_camera_dev_get_info(s_cam, &info) == OPRT_OK && info.supported_fmts != 0 &&
        !(info.supported_fmts & TDL_CAMERA_FMT_H264)) {
        PR_WARN("camera '%s' has no encoded output (supported_fmts=0x%x)", CAMERA_NAME, info.supported_fmts);
        s_cam = NULL;
        return OPRT_NOT_SUPPORTED;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.width = DEMO_CAM_WIDTH;
    cfg.height = DEMO_CAM_HEIGHT;
    cfg.fps = DEMO_CAM_FPS;
    /* TDL_CAMERA_FMT_E has no H.265 member; the board encoder decides the codec. */
    cfg.out_fmt = TDL_CAMERA_FMT_H264;
    cfg.get_encoded_frame_cb = __demo_encoded_frame_cb;

    rt = tdl_camera_dev_open(s_cam, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("tdl_camera_dev_open failed: %d", rt);
        s_cam = NULL;
        return rt;
    }

    s_cam_running = TRUE;
    PR_NOTICE("live camera started: %ux%u@%u %s", (uint32_t)DEMO_CAM_WIDTH, (uint32_t)DEMO_CAM_HEIGHT,
              (uint32_t)DEMO_CAM_FPS, DEMO_VIDEO_CODEC == TY_AV_CODEC_VIDEO_H265 ? "H265" : "H264");
    return OPRT_OK;
}

static void __demo_camera_close(void)
{
    if (s_cam != NULL && s_cam_running) {
        (void)tdl_camera_dev_close(s_cam);
        s_cam_running = FALSE;
        PR_NOTICE("live camera stopped");
    }
    s_cam = NULL;
}

static OPERATE_RET __demo_camera_request_i_frame(void)
{
    (void)s_cam;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET __demo_camera_set_bitrate(uint32_t kbps)
{
    (void)kbps;
    return OPRT_NOT_SUPPORTED;
}

#else /* !DEMO_HAS_CAMERA - the demo bitstream is the only source */

static BOOL_T      __demo_camera_present(void)              { return FALSE; }
static OPERATE_RET __demo_camera_open(void)                 { return OPRT_NOT_SUPPORTED; }
static void        __demo_camera_close(void)                { }
static OPERATE_RET __demo_camera_request_i_frame(void)      { return OPRT_NOT_SUPPORTED; }
static OPERATE_RET __demo_camera_set_bitrate(uint32_t kbps) { (void)kbps; return OPRT_NOT_SUPPORTED; }

#endif /* DEMO_HAS_CAMERA */

static void __demo_camera_sync(void)
{
    BOOL_T wanted;

    if (s_cam_lock == NULL) {
        return; /* before tuya_ipc_demo_start(); nothing owns a sensor yet */
    }

    tal_mutex_lock(s_cam_lock);
    wanted = s_live_want_cam;

    if (wanted && !s_cam_running) {
        if (__demo_camera_open() != OPRT_OK) {
            PR_ERR("camera open failed, nothing will be encoded");
        }
    } else if (!wanted && s_cam_running) {
        __demo_camera_close();
    }
    tal_mutex_unlock(s_cam_lock);
}

#if DEMO_HAS_FILE_PLAYBACK
static int __demo_read_one_au(const uint8_t *video_buf, uint32_t offset, uint32_t buf_size, uint32_t *is_key_frame,
                              uint32_t *frame_len, uint32_t *frame_start)
{
    uint32_t pos = 0;
    int      need_calc = 0;
    uint8_t  nal_type = 0;
    int      idx = 0;

    if (buf_size <= 5) {
        return -1;
    }
    for (pos = 0; pos <= buf_size - 5; pos++) {
        if (video_buf[pos] == 0x00 && video_buf[pos + 1] == 0x00 && video_buf[pos + 2] == 0x00 &&
            video_buf[pos + 3] == 0x01) {
            nal_type = (uint8_t)(video_buf[pos + 4] & 0x1f);
            if (nal_type == 0x7) {
                if (need_calc == 1) {
                    *frame_len = pos - (uint32_t)idx;
                    return 0;
                }
                *is_key_frame = 1;
                *frame_start = offset + pos;
                need_calc = 1;
                idx = (int)pos;
            } else if (nal_type == 0x1) {
                if (need_calc) {
                    *frame_len = pos - (uint32_t)idx;
                    return 0;
                }
                *frame_start = offset + pos;
                *is_key_frame = 0;
                idx = (int)pos;
                need_calc = 1;
            }
        }
    }
    *frame_len = buf_size;
    return 0;
}

static OPERATE_RET __demo_file_load(void)
{
#if OPERATING_SYSTEM == SYSTEM_LINUX
    char  path[512] = {0};
    FILE *fp = NULL;
    long  size;

    if (getcwd(path, sizeof(path)) == NULL) {
        PR_ERR("getcwd failed");
        return OPRT_COM_ERROR;
    }
    strncat(path, "/" DEMO_FILE_PATH, sizeof(path) - strlen(path) - 1);
    fp = fopen(path, "rb");
    if (fp == NULL) {
        PR_WARN("no demo video file at %s", path);
        return OPRT_NOT_FOUND;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 128) {
        PR_ERR("demo video file too small: %ld", size);
        fclose(fp);
        return OPRT_COM_ERROR;
    }
    s_file_buf = (uint8_t *)malloc((size_t)size);
    if (s_file_buf == NULL) {
        PR_ERR("malloc %ld for demo video failed", size);
        fclose(fp);
        return OPRT_MALLOC_FAILED;
    }
    if (fread(s_file_buf, 1, (size_t)size, fp) != (size_t)size) {
        PR_ERR("fread demo video incomplete");
        free(s_file_buf);
        s_file_buf = NULL;
        fclose(fp);
        return OPRT_COM_ERROR;
    }
    fclose(fp);
    s_file_h264 = s_file_buf;
    s_file_size = (uint32_t)size;
#else
    s_file_h264 = demo_video_264_start;
    s_file_size = (uint32_t)(demo_video_264_end - demo_video_264_start);
    if (s_file_h264 == NULL || s_file_size < 128) {
        PR_ERR("embedded demo_video.264 invalid size=%u", (uint32_t)s_file_size);
        s_file_h264 = NULL;
        s_file_size = 0;
        return OPRT_COM_ERROR;
    }
#endif
    s_file_offset = 0;
    s_file_frame_len = 0;
    s_file_frame_start = 0;
    s_file_is_key = 0;
    s_file_pts_idx = 0;
    PR_NOTICE("file playback ready: %u bytes %ux%u@%u", (uint32_t)s_file_size, (uint32_t)DEMO_FILE_WIDTH,
              (uint32_t)DEMO_FILE_HEIGHT, (uint32_t)DEMO_FILE_FPS);
    return OPRT_OK;
}

static void __demo_file_unload(void)
{
#if OPERATING_SYSTEM == SYSTEM_LINUX
    if (s_file_buf != NULL) {
        free(s_file_buf);
        s_file_buf = NULL;
    }
#endif
    s_file_h264 = NULL;
    s_file_size = 0;
}

static int __demo_file_get_frame(MEDIA_FRAME *media_frame)
{
    uint64_t pts_ms;
    int      ret;

    if (s_file_h264 == NULL || s_file_size == 0) {
        return -1;
    }

    s_file_offset = s_file_frame_start + s_file_frame_len;
    if (s_file_offset >= s_file_size) {
        s_file_offset = 0;
        s_file_frame_len = 0;
        s_file_frame_start = 0;
        s_file_is_key = 0;
        s_file_pts_idx = 0;
    }

    ret = __demo_read_one_au(s_file_h264 + s_file_offset, s_file_offset, s_file_size - s_file_offset, &s_file_is_key,
                             &s_file_frame_len, &s_file_frame_start);
    if (ret != 0 || s_file_frame_len == 0) {
        return -1;
    }
    if (s_file_frame_len > DEMO_P2P_FRAME_CAP) {
        PR_WARN("demo AU too large len=%u cap=%u", (uint32_t)s_file_frame_len, (uint32_t)DEMO_P2P_FRAME_CAP);
        return -1;
    }

    memcpy(media_frame->data, s_file_h264 + s_file_frame_start, s_file_frame_len);
    media_frame->size = s_file_frame_len;
    media_frame->type = s_file_is_key ? eVideoIFrame : eVideoPBFrame;
    pts_ms = (s_file_pts_idx * 1000ULL) / DEMO_FILE_FPS;
    s_file_pts_idx++;
    media_frame->timestamp = (uint32_t)pts_ms;
    media_frame->pts = pts_ms * 1000ULL;

    tal_system_sleep(1000 / DEMO_FILE_FPS);
    return 0;
}
#endif /* DEMO_HAS_FILE_PLAYBACK */

static void __demo_init_p2p_av_info(BOOL_T from_camera)
{
    TRANS_IPC_AV_INFO_T av_info;
    OPERATE_RET         rt;
    uint32_t            w, h, fps, gop, kbps;
    int                 i;

    if (from_camera) {
        w = DEMO_CAM_WIDTH;
        h = DEMO_CAM_HEIGHT;
        fps = DEMO_AV_FPS;
        gop = DEMO_CAM_GOP;
        kbps = DEMO_CAM_KBPS;
    } else {
#if DEMO_HAS_FILE_PLAYBACK
        w = DEMO_FILE_WIDTH;
        h = DEMO_FILE_HEIGHT;
        fps = DEMO_FILE_FPS;
        gop = DEMO_FILE_GOP;
        kbps = DEMO_FILE_KBPS;
#else
        return;
#endif
    }

    memset(&av_info, 0, sizeof(av_info));
    for (i = 0; i < 2; i++) {
        int s = (i == 0) ? eIpcStreamVideoMain : eIpcStreamVideoSub;

        av_info.video_codec[s] = DEMO_VIDEO_CODEC;
        av_info.fps[s] = fps;
        av_info.gop[s] = gop;
        av_info.bitrate[s] = kbps;
        av_info.width[s] = w;
        av_info.height[s] = h;
    }
#if DEMO_HAS_AUDIO
    av_info.audio_codec = TY_AV_CODEC_AUDIO_G711U;
    av_info.audio_sample = TY_AUDIO_SAMPLE_8K;
    av_info.audio_databits = TY_AUDIO_DATABITS_16;
    av_info.audio_channel = TY_AUDIO_CHANNEL_MONO;
#endif

    rt = tuya_ipc_init_trans_av_info(&av_info);
    if (rt != OPRT_OK) {
        PR_ERR("tuya_ipc_init_trans_av_info failed: %d", rt);
    }
}

void tuya_ipc_demo_start(void)
{
    OPERATE_RET rt;
    BOOL_T      have_camera;
    uint32_t    i;

    if (s_media_ready) {
        return;
    }

    rt = tal_mutex_create_init(&s_frame_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("frame mutex create failed: %d", rt);
        return;
    }
    rt = tal_mutex_create_init(&s_cam_lock);
    if (rt != OPRT_OK) {
        PR_ERR("camera mutex create failed: %d", rt);
        return;
    }

    s_frame_idx = 0;
    s_live_push_enable = FALSE;
    s_live_want_cam = FALSE;
    __demo_p2p_queue_clear();

    for (i = 0; i < DEMO_P2P_QUEUE_DEPTH; i++) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
        s_q_pool[i] = (uint8_t *)tal_psram_malloc(DEMO_FRAME_BUF_SIZE);
#else
        s_q_pool[i] = (uint8_t *)tal_malloc(DEMO_FRAME_BUF_SIZE);
#endif
        if (s_q_pool[i] == NULL) {
            PR_ERR("alloc p2p queue slot %u failed", i);
            return;
        }
    }
    s_frame_slot_cap = DEMO_FRAME_BUF_SIZE;

    have_camera = __demo_camera_present();

#if DEMO_HAS_FILE_PLAYBACK
    if (__demo_file_load() != OPRT_OK && !have_camera) {
        PR_ERR("neither a camera nor a demo bitstream is available");
    }
#else
    if (!have_camera) {
        PR_ERR("no camera registered and this build has no file playback");
    }
#endif

    __demo_init_p2p_av_info(have_camera);

#if DEMO_HAS_RING_BUFFER
    {
        RING_BUFFER_INIT_PARAM_T rp = {0};

        rp.bitrate = DEMO_CAM_KBPS;
        rp.fps = DEMO_AV_FPS;
        rp.max_buffer_seconds = 2;
        if (tuya_ipc_ring_buffer_init(0, 0, E_IPC_STREAM_VIDEO_MAIN, &rp) == OPRT_OK) {
            s_ring_w = tuya_ipc_ring_buffer_open(0, 0, E_IPC_STREAM_VIDEO_MAIN, E_RBUF_WRITE);
            s_ring_r = tuya_ipc_ring_buffer_open(0, 0, E_IPC_STREAM_VIDEO_MAIN, E_RBUF_READ);
            s_ring_ready = (s_ring_w != NULL && s_ring_r != NULL) ? TRUE : FALSE;
            PR_NOTICE("ring_buffer live video %s", s_ring_ready ? "ready" : "open fail");
        }
    }
#endif

#if DEMO_HAS_AUDIO
    if (__demo_audio_uplink_init() != OPRT_OK) {
        PR_ERR("audio uplink init failed");
    }
#endif

#if DEMO_NEEDS_FS_MOUNT
    if (tkl_fs_mount(DEMO_FS_MOUNT, DEV_SDCARD) != OPRT_OK) {
        PR_ERR("mount %s failed (FAT card? SDIO=P2/P3/...)", DEMO_FS_MOUNT);
    } else {
        PR_NOTICE("mount ok: %s", DEMO_FS_MOUNT);
    }
#endif
#if DEMO_HAS_LOCAL_STORE
    if (local_store_init() != OPRT_OK) {
        PR_ERR("local_store init failed");
    }
#endif

    s_media_ready = TRUE;
    demo_media_event_register();

    PR_NOTICE("tuya_ipc_demo: ready (source=%s, sensor %ux%u@%u, av fps %u, live_rec=%d)",
              have_camera ? "camera" : "file", (uint32_t)DEMO_CAM_WIDTH, (uint32_t)DEMO_CAM_HEIGHT,
              (uint32_t)DEMO_CAM_FPS, (uint32_t)DEMO_AV_FPS, (int)DEMO_LIVE_RECORD);
}

void tuya_ipc_demo_end(void)
{
    uint32_t i;

    s_live_push_enable = FALSE;
    s_live_want_cam = FALSE;
#if DEMO_HAS_AUDIO
    __demo_mic_stop();
#endif
    if (s_cam_lock != NULL) {
        tal_mutex_lock(s_cam_lock);
        __demo_camera_close();
        tal_mutex_unlock(s_cam_lock);
    } else {
        __demo_camera_close();
    }
    s_media_ready = FALSE;

    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }
    for (i = 0; i < DEMO_P2P_QUEUE_DEPTH; i++) {
        if (s_q_pool[i] != NULL) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
            tal_psram_free(s_q_pool[i]);
#else
            tal_free(s_q_pool[i]);
#endif
            s_q_pool[i] = NULL;
        }
    }
    s_frame_slot_cap = 0;
    if (s_frame_mutex != NULL) {
        tal_mutex_release(s_frame_mutex);
        s_frame_mutex = NULL;
    }
    if (s_cam_lock != NULL) {
        tal_mutex_release(s_cam_lock);
        s_cam_lock = NULL;
    }

#if DEMO_HAS_FILE_PLAYBACK
    __demo_file_unload();
#endif
#if DEMO_HAS_AUDIO
    __demo_audio_uplink_deinit();
#endif
}

int demo_on_live_video_start_callback(void)
{
    if (!s_media_ready) {
        return -1;
    }

    s_live_want_cam = TRUE;
    __demo_camera_sync();
    if (!s_cam_running) {
#if DEMO_HAS_FILE_PLAYBACK
        PR_WARN("LIVE start: no camera, falling back to file playback");
#else
        PR_ERR("LIVE start: camera open failed");
        s_live_want_cam = FALSE;
        return -1;
#endif
    }

#if DEMO_LIVE_RECORD
    if (!local_store_rec_is_open()) {
        __demo_rec_start();
    }
#endif

    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }

#if DEMO_HAS_AUDIO
    if (!s_mic_running && __demo_mic_start() != OPRT_OK) {
        PR_ERR("LIVE start: mic start failed");
    }
#endif

    s_live_push_enable = TRUE;
    PR_NOTICE("LIVE video start: push enabled");
    return 0;
}

int demo_on_live_video_stop_callback(void)
{
    s_live_want_cam = FALSE;
    s_live_push_enable = FALSE;
    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }
    __demo_camera_sync();
#if DEMO_LIVE_RECORD
    __demo_rec_stop();
#endif
#if DEMO_HAS_AUDIO
    demo_mic_uplink_pause();
#endif
    PR_NOTICE("LIVE video stop: camera %s", s_cam_running ? "kept for cloud recording" : "closed");
    return 0;
}

int demo_on_signal_disconnect_callback(void)
{
    demo_media_pb_stop();
    (void)demo_on_live_video_stop_callback();
#if DEMO_HAS_AUDIO
    __demo_mic_stop();
#endif
    return 0;
}

int demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame)
{
    OPERATE_RET rt;

    if (media_frame == NULL || media_frame->data == NULL || !s_media_ready) {
        tal_system_sleep(10);
        return -1;
    }

#if DEMO_HAS_FILE_PLAYBACK
    if (!s_cam_running) {
        return __demo_file_get_frame(media_frame);
    }
#endif

#if DEMO_HAS_RING_BUFFER
    if (s_ring_ready && s_ring_r != NULL) {
        RING_BUFFER_NODE_T *node = tuya_ipc_ring_buffer_get_frame(s_ring_r, FALSE);

        if (node != NULL && node->raw_data != NULL && node->size > 0 && node->size <= DEMO_P2P_FRAME_CAP) {
            memcpy(media_frame->data, node->raw_data, node->size);
            media_frame->size = node->size;
            media_frame->type = (node->type == E_VIDEO_I_FRAME) ? eVideoIFrame : eVideoPBFrame;
            media_frame->pts = node->timestamp;
            media_frame->timestamp = (uint32_t)node->timestamp;
            return 0;
        }
        tal_system_sleep(10);
        return -1;
    }
#endif

    tal_mutex_lock(s_frame_mutex);
    rt = __demo_p2p_queue_pop(media_frame);
    tal_mutex_unlock(s_frame_mutex);
    if (rt != OPRT_OK) {
        tal_system_sleep(10);
        return -1;
    }
    return 0;
}

int demo_on_request_i_frame_callback(void)
{
    return (__demo_camera_request_i_frame() == OPRT_OK) ? 0 : -1;
}

int demo_on_set_video_bitrate_callback(uint32_t kbps)
{
    return (__demo_camera_set_bitrate(kbps) == OPRT_OK) ? 0 : -1;
}

#if DEMO_HAS_AUDIO

#define DEMO_DUPLEX_FAR_THRESH 400
#define DEMO_DUPLEX_HANG_MS    600
#define DEMO_DUPLEX_ATTEN_Q15  1024 /* 0.031 -> -30 dB */

#define DEMO_NOISE_PK_TH   800
#define DEMO_NOISE_HANG_MS 500

static int32_t s_noise_hang_ms = 0;
static volatile BOOL_T  s_far_active = FALSE;
static volatile int32_t s_far_hold_ms = 0;

static int32_t __demo_frame_peak(const int16_t *pcm, size_t n)
{
    int32_t pk = 0;
    size_t  i;

    for (i = 0; i < n; i++) {
        int32_t v = pcm[i] < 0 ? -(int32_t)pcm[i] : (int32_t)pcm[i];
        if (v > pk) {
            pk = v;
        }
    }
    return pk;
}

static uint32_t __demo_dl_queued_ms(void)
{
    uint32_t n;

    tal_mutex_lock(s_audio_mutex);
    n = s_dl_count;
    tal_mutex_unlock(s_audio_mutex);
    return n * 1000u / 8000u;
}

static void __demo_ul_clear(void)
{
    if (s_audio_mutex == NULL) {
        return;
    }
    tal_mutex_lock(s_audio_mutex);
    s_ul_head = s_ul_tail = s_ul_count = 0;
    tal_mutex_unlock(s_audio_mutex);
}

static void __demo_dl_clear(void)
{
    if (s_audio_mutex == NULL) {
        return;
    }
    tal_mutex_lock(s_audio_mutex);
    s_dl_head = s_dl_tail = s_dl_count = 0;
    tal_mutex_unlock(s_audio_mutex);
}

static void __demo_ul_push(const uint8_t *data, uint64_t ts_ms)
{
    tal_mutex_lock(s_audio_mutex);
    if (s_ul_count >= DEMO_AUDIO_RING_FRAMES) {
        s_ul_head = (s_ul_head + 1U) % DEMO_AUDIO_RING_FRAMES;
        s_ul_count--;
        s_ul_ovf++;
        PR_WARN("uplink ring overflow count=%u", s_ul_ovf);
    }
    memcpy(s_ul_ring[s_ul_tail].data, data, DEMO_AUDIO_FRAME_BYTES);
    s_ul_ring[s_ul_tail].ts_ms = ts_ms;
    s_ul_tail = (s_ul_tail + 1U) % DEMO_AUDIO_RING_FRAMES;
    s_ul_count++;
    tal_mutex_unlock(s_audio_mutex);
}

static void __demo_dl_push(const int16_t *pcm, uint32_t n)
{
    uint32_t i;
    BOOL_T   dropped = FALSE;

    tal_mutex_lock(s_audio_mutex);
    for (i = 0; i < n; i++) {
        if (s_dl_count >= DEMO_DL_PCM_CAP) {
            s_dl_head = (s_dl_head + 1U) % DEMO_DL_PCM_CAP;
            s_dl_count--;
            dropped = TRUE;
        }
        s_dl_pcm[s_dl_tail] = pcm[i];
        s_dl_tail = (s_dl_tail + 1U) % DEMO_DL_PCM_CAP;
        s_dl_count++;
    }
    if (dropped) {
        s_dl_ovf++;
    }
    tal_mutex_unlock(s_audio_mutex);
}

static uint32_t __demo_dl_pull(int16_t *pcm, uint32_t n)
{
    uint32_t i, got = 0;

    tal_mutex_lock(s_audio_mutex);
    for (i = 0; i < n && s_dl_count > 0; i++) {
        pcm[i] = s_dl_pcm[s_dl_head];
        s_dl_head = (s_dl_head + 1U) % DEMO_DL_PCM_CAP;
        s_dl_count--;
        got++;
    }
    tal_mutex_unlock(s_audio_mutex);
    return got;
}

static void __demo_play_thread(void *arg)
{
    int16_t pcm[DEMO_DL_PLAY_SAMPLES];

    (void)arg;
    s_play_alive = TRUE;
    while (s_play_run) {
        uint32_t got = __demo_dl_pull(pcm, DEMO_DL_PLAY_SAMPLES);

        if (got == 0) {
            tal_system_sleep(DEMO_AUDIO_FRAME_MS);
            continue;
        }
        if (got < DEMO_DL_PLAY_SAMPLES) {
            memset(pcm + got, 0, (DEMO_DL_PLAY_SAMPLES - got) * sizeof(int16_t));
        }
        (void)demo_audio_port_play(pcm, DEMO_DL_PLAY_SAMPLES);
    }
    s_play_alive = FALSE;
}

static OPERATE_RET __demo_play_start(void)
{
    THREAD_CFG_T cfg;

    if (s_play_th != NULL) {
        return OPRT_OK;
    }
    s_play_run = TRUE;
    s_play_alive = FALSE;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stackDepth = 4096;
    cfg.priority = THREAD_PRIO_3;
    cfg.thrdname = "dl_play";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cfg.psram_mode = 1;
#endif
    if (tal_thread_create_and_start(&s_play_th, NULL, NULL, __demo_play_thread, NULL, &cfg) != OPRT_OK) {
        s_play_run = FALSE;
        s_play_th = NULL;
        PR_ERR("downlink play thread create failed");
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

static void __demo_play_stop(void)
{
    THREAD_HANDLE h;
    uint32_t      wait;

    s_play_run = FALSE;
    for (wait = 0; s_play_alive && wait < 50; wait++) {
        tal_system_sleep(10);
    }
    h = s_play_th;
    s_play_th = NULL;
    if (h != NULL) {
        tal_thread_delete(h);
    }
    __demo_dl_clear();
}

static void __demo_mic_frame_cb(const int16_t *pcm, uint32_t samples)
{
    static int16_t pcm8k[DEMO_AUDIO_PCM_MAX];
    uint8_t        g711[DEMO_AUDIO_FRAME_BYTES];
    uint32_t in_n, off;
    int32_t  frame_ms;
    uint64_t ts0;

    if (!s_mic_running || pcm == NULL || samples == 0) {
        return;
    }
    if (s_spk_active) {
        return;
    }
    in_n = samples;
    if (in_n > DEMO_AUDIO_PCM_MAX) {
        PR_WARN("mic frame %u samples exceeds %u, dropped", in_n, (uint32_t)DEMO_AUDIO_PCM_MAX);
        return;
    }
    memcpy(pcm8k, pcm, in_n * sizeof(int16_t));
    frame_ms = (int32_t)(in_n * 1000u / DEMO_AUDIO_PORT_RATE);
    ts0 = tuya_p2p_misc_get_current_time_ms();
    if (ts0 > (uint64_t)frame_ms) {
        ts0 -= (uint64_t)frame_ms;
    }

    if (__demo_dl_queued_ms() > 0) {
        s_far_hold_ms = DEMO_DUPLEX_HANG_MS;
        s_far_active = TRUE;
    } else if (s_far_hold_ms > 0) {
        s_far_hold_ms -= frame_ms;
        if (s_far_hold_ms <= 0) {
            s_far_hold_ms = 0;
            s_far_active = FALSE;
        }
    }
    {
        int32_t  mic_pk = __demo_frame_peak(pcm8k, in_n);
        uint32_t k;

        if (s_far_active) {
            for (k = 0; k < in_n; k++) {
                pcm8k[k] = (int16_t)(((int32_t)pcm8k[k] * DEMO_DUPLEX_ATTEN_Q15) >> 15);
            }
        }
        if (mic_pk >= DEMO_NOISE_PK_TH) {
            s_noise_hang_ms = DEMO_NOISE_HANG_MS;
        } else if (s_noise_hang_ms > 0) {
            s_noise_hang_ms -= frame_ms;
            if (s_noise_hang_ms < 0) {
                s_noise_hang_ms = 0;
            }
        }
        if (s_noise_hang_ms <= 0 && mic_pk < DEMO_NOISE_PK_TH) {
            return;
        }
    }

    for (off = 0; off + DEMO_AUDIO_FRAME_BYTES <= in_n; off += DEMO_AUDIO_FRAME_BYTES) {
        uint32_t i;

        for (i = 0; i < DEMO_AUDIO_FRAME_BYTES; i++) {
            g711[i] = demo_g711u_encode_sample((int)pcm8k[off + i]);
        }
        __demo_ul_push(g711, ts0 + (uint64_t)(off * 1000u / DEMO_AUDIO_PORT_RATE));
    }
}

static OPERATE_RET __demo_mic_start(void)
{
    OPERATE_RET rt = demo_audio_port_open(__demo_mic_frame_cb);

    if (rt != OPRT_OK) {
        return rt;
    }
    s_noise_hang_ms = 0;
    s_ul_ovf = 0;
    s_dl_ovf = 0;
    s_mic_running = TRUE;
    PR_NOTICE("mic started: %uHz 20ms G.711U dtx pk<%d SRAM=%u", (uint32_t)DEMO_AUDIO_PORT_RATE, DEMO_NOISE_PK_TH,
              (unsigned)tal_system_get_free_heap_size());
    return OPRT_OK;
}

static void __demo_mic_stop(void)
{
    if (!s_mic_running) {
        __demo_play_stop();
        return;
    }
    s_mic_running = FALSE;
    s_spk_active = FALSE;
    __demo_play_stop();
    demo_audio_port_close();
    PR_NOTICE("mic stopped SRAM=%u ul_ovf=%u dl_ovf=%u", (unsigned)tal_system_get_free_heap_size(), s_ul_ovf,
              s_dl_ovf);
}

static OPERATE_RET __demo_audio_uplink_init(void)
{
    OPERATE_RET rt;

    if (s_audio_inited) {
        return OPRT_OK;
    }
    rt = tal_mutex_create_init(&s_audio_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    s_ul_head = s_ul_tail = s_ul_count = 0;
    s_dl_head = s_dl_tail = s_dl_count = 0;
    s_audio_inited = TRUE;
    return OPRT_OK;
}

static void __demo_audio_uplink_deinit(void)
{
    __demo_play_stop();
    if (s_audio_mutex != NULL) {
        tal_mutex_lock(s_audio_mutex);
        s_ul_head = s_ul_tail = s_ul_count = 0;
        s_dl_head = s_dl_tail = s_dl_count = 0;
        tal_mutex_unlock(s_audio_mutex);
        tal_mutex_release(s_audio_mutex);
        s_audio_mutex = NULL;
    }
    s_audio_inited = FALSE;
}

void demo_mic_uplink_pause(void)
{
    __demo_mic_stop();
}

int demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    uint64_t ts_ms;

    if (media_frame == NULL || media_frame->data == NULL || !s_audio_inited) {
        return -1;
    }
    tal_mutex_lock(s_audio_mutex);
    if (s_ul_count == 0) {
        tal_mutex_unlock(s_audio_mutex);
        return -1;
    }
    memcpy(media_frame->data, s_ul_ring[s_ul_head].data, DEMO_AUDIO_FRAME_BYTES);
    ts_ms = s_ul_ring[s_ul_head].ts_ms;
    s_ul_head = (s_ul_head + 1U) % DEMO_AUDIO_RING_FRAMES;
    s_ul_count--;
    tal_mutex_unlock(s_audio_mutex);

    media_frame->size = DEMO_AUDIO_FRAME_BYTES;
    media_frame->type = eAudioFrame;
    media_frame->timestamp = ts_ms;
    media_frame->pts = ts_ms * 1000ULL;
    return 0;
}

int demo_on_live_audio_start_callback(void)
{
    s_spk_active = TRUE;
    s_noise_hang_ms = 0;
    s_far_active = FALSE;
    s_far_hold_ms = 0;
    __demo_ul_clear();
    __demo_dl_clear();
    if (__demo_play_start() != OPRT_OK) {
        return -1;
    }
    PR_NOTICE("LIVE audio(speaker) start: uplink held");
    return 0;
}

int demo_on_live_audio_stop_callback(void)
{
    __demo_play_stop();
    s_spk_active = FALSE;
    s_noise_hang_ms = 0;
    s_far_active = FALSE;
    s_far_hold_ms = 0;
    __demo_ul_clear();
    PR_NOTICE("LIVE audio(speaker) stop: uplink resume");
    return 0;
}

int demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    static int16_t pcm8k[DEMO_DOWNLINK_G711_MAX];
    uint32_t       i, n;

    if (media_frame == NULL || media_frame->data == NULL || media_frame->size == 0 || !s_spk_active) {
        return 0;
    }
    n = media_frame->size;
    if (n > DEMO_DOWNLINK_G711_MAX) {
        n = DEMO_DOWNLINK_G711_MAX;
    }
    for (i = 0; i < n; i++) {
        pcm8k[i] = (int16_t)demo_g711u_decode_sample(((const uint8_t *)media_frame->data)[i]);
    }
    if (__demo_frame_peak(pcm8k, n) > DEMO_DUPLEX_FAR_THRESH) {
        s_far_hold_ms = DEMO_DUPLEX_HANG_MS;
        s_far_active = TRUE;
    }
    __demo_dl_push(pcm8k, n);
    return 0;
}

#else /* !DEMO_HAS_AUDIO */

void demo_mic_uplink_pause(void)
{
}

int demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    (void)media_frame;
    return -1;
}

int demo_on_live_audio_start_callback(void)
{
    return 0;
}

int demo_on_live_audio_stop_callback(void)
{
    return 0;
}

int demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    (void)media_frame;
    return 0;
}

#endif /* DEMO_HAS_AUDIO */
