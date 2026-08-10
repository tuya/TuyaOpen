/**
 * @file tuya_ipc_demo.c
 * @brief Tuya IPC demo media callbacks (file-based on LINUX, stub on RTOS)
 * @version 1.1
 * @date 2026-07-31
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_ipc_demo.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_system.h"
#include <string.h>

#define TKL_VENC_MAIN_FPS 30

#if OPERATING_SYSTEM == SYSTEM_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

STATIC CHAR_T g_demo_path[512] = {0};
STATIC UINT8_T *g_video_buf = NULL;
STATIC INT_T g_file_size = 0;
STATIC BOOL_T g_is_last_frame = FALSE;
STATIC UINT32_T g_frame_len = 0, g_frame_start = 0;
STATIC UINT32_T g_offset = 0;
STATIC UINT32_T g_is_key_frame = 0;
STATIC FILE *g_fp = NULL;

/**
 * @brief Initialize demo video file
 * @return none
 */
VOID_T tuya_ipc_demo_start(VOID_T)
{
    if (getcwd(g_demo_path, sizeof(g_demo_path)) == NULL) {
        PR_ERR("getcwd failed");
        return;
    }
    strncat(g_demo_path, "/demo_video.264", sizeof(g_demo_path) - strlen(g_demo_path) - 1);
    g_fp = fopen(g_demo_path, "rb");
    if (g_fp == NULL) {
        PR_ERR("cannot read demo video file %s", g_demo_path);
        return;
    }

    fseek(g_fp, 0, SEEK_END);
    g_file_size = (INT_T)ftell(g_fp);
    fseek(g_fp, 0, SEEK_SET);

    g_video_buf = (UINT8_T *)malloc((size_t)g_file_size);
    if (g_video_buf == NULL) {
        PR_ERR("malloc video buffer failed");
        fclose(g_fp);
        g_fp = NULL;
        return;
    }

    if (fread(g_video_buf, 1, (size_t)g_file_size, g_fp) != (size_t)g_file_size) {
        PR_ERR("fread demo video incomplete");
    }
}

/**
 * @brief Clean up demo resources
 * @return none
 */
VOID_T tuya_ipc_demo_end(VOID_T)
{
    if (g_video_buf) {
        free(g_video_buf);
        g_video_buf = NULL;
    }
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
    g_file_size = 0;
    g_is_last_frame = FALSE;
    g_frame_len = 0;
    g_frame_start = 0;
    g_offset = 0;
    g_is_key_frame = 0;
}

/**
 * @brief Parse one H.264 AU from demo Annex-B buffer
 * @param[in] video_buf buffer base
 * @param[in] offset absolute offset
 * @param[in] buf_size remaining size
 * @param[out] is_key_frame keyframe flag
 * @param[out] frame_len AU length
 * @param[out] frame_start absolute start
 * @return 0 on success, -1 on failure
 */
STATIC INT_T read_one_frame_from_demo_video_file(UINT8_T *video_buf, UINT32_T offset, UINT32_T buf_size,
                                                 UINT32_T *is_key_frame, UINT32_T *frame_len, UINT32_T *frame_start)
{
    UINT32_T pos = 0;
    INT_T need_calc = 0;
    UINT8_T nal_type = 0;
    INT_T idx = 0;

    if (buf_size <= 5) {
        return -1;
    }

    for (pos = 0; pos <= buf_size - 5; pos++) {
        if (video_buf[pos] == 0x00 && video_buf[pos + 1] == 0x00 && video_buf[pos + 2] == 0x00 &&
            video_buf[pos + 3] == 0x01) {
            nal_type = (UINT8_T)(video_buf[pos + 4] & 0x1f);
            if (nal_type == 0x7) {
                if (need_calc == 1) {
                    *frame_len = pos - (UINT32_T)idx;
                    return 0;
                }
                *is_key_frame = 1;
                *frame_start = offset + pos;
                need_calc = 1;
                idx = (INT_T)pos;
            } else if (nal_type == 0x1) {
                if (need_calc) {
                    *frame_len = pos - (UINT32_T)idx;
                    return 0;
                }
                *frame_start = offset + pos;
                *is_key_frame = 0;
                idx = (INT_T)pos;
                need_calc = 1;
            }
        }
    }

    *frame_len = buf_size;
    return 0;
}

/**
 * @brief Get current time in milliseconds
 * @return timestamp in ms
 */
STATIC UINT32_T get_time_ms(VOID_T)
{
    return (UINT32_T)tal_system_get_millisecond();
}

/**
 * @brief Signal disconnect callback
 * @return 0 on success
 */
INT_T demo_on_signal_disconnect_callback(VOID_T)
{
    tuya_ipc_demo_end();
    return 0;
}

/**
 * @brief Get video frame callback
 * @param[in,out] media_frame media frame
 * @return 0 on success, -1 on failure
 */
INT_T demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame)
{
    INT_T ret = 0;

    if (media_frame == NULL || g_video_buf == NULL || g_file_size <= 0) {
        return -1;
    }

    g_offset = g_frame_start + g_frame_len;
    if (g_offset >= (UINT32_T)g_file_size) {
        g_is_last_frame = FALSE;
        g_frame_len = 0;
        g_frame_start = 0;
        g_offset = 0;
        g_is_key_frame = 0;
        return -1;
    }

    ret = read_one_frame_from_demo_video_file(g_video_buf + g_offset, g_offset, (UINT32_T)g_file_size - g_offset,
                                              &g_is_key_frame, &g_frame_len, &g_frame_start);
    if (ret) {
        return -1;
    }

    memcpy(media_frame->data, g_video_buf + g_offset, g_frame_len);
    media_frame->size = g_frame_len;
    media_frame->pts = get_time_ms();
    media_frame->timestamp = get_time_ms();
    media_frame->type = g_is_key_frame ? eVideoIFrame : eVideoPBFrame;

    tal_system_sleep(1000 / TKL_VENC_MAIN_FPS);
    return 0;
}

/**
 * @brief Get audio frame callback
 * @param[in,out] media_frame media frame
 * @return 0 on success
 * @note Ubuntu file demo has no live mic uplink; local_record uses demo_audio.aac
 */
INT_T demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    (VOID_T)media_frame;
    return 0;
}

/**
 * @brief LIVE video start (file already loaded in tuya_ipc_demo_start)
 * @return 0 on success
 */
INT_T demo_on_live_video_start_callback(VOID_T)
{
    return 0;
}

/**
 * @brief LIVE video stop
 * @return 0 on success
 */
INT_T demo_on_live_video_stop_callback(VOID_T)
{
    return 0;
}

/**
 * @brief LIVE speaker start (no AO on Ubuntu file demo)
 * @return 0 on success
 */
INT_T demo_on_live_audio_start_callback(VOID_T)
{
    return 0;
}

/**
 * @brief LIVE speaker stop
 * @return 0 on success
 */
INT_T demo_on_live_audio_stop_callback(VOID_T)
{
    return 0;
}

/**
 * @brief Recv APP audio (ignored on Ubuntu file demo)
 * @param[in] media_frame unused
 * @return 0 on success
 */
INT_T demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    (VOID_T)media_frame;
    return 0;
}

#else /* !SYSTEM_LINUX — T5: embedded demo H264 or live GC2145 */

#if (defined(CAMERA_DEMO_P2P_FILE_H264) && (CAMERA_DEMO_P2P_FILE_H264 == 1))

#include "tuya_ipc_p2p.h"

/* demo_video.264 in repo: 320x240 Annex-B, ~30fps */
#define DEMO_FILE_FPS        30
#define DEMO_FILE_WIDTH      320
#define DEMO_FILE_HEIGHT     240
#define DEMO_FILE_GOP        30
#define DEMO_FILE_BITRATE_KB 512
/* Must match p2p_init() media_frame buffer (300 * 1024); do not use media_frame->size as cap */
#define DEMO_P2P_VIDEO_BUF_SIZE (300 * 1024)

extern const UINT8_T demo_video_264_start[];
extern const UINT8_T demo_video_264_end[];

STATIC CONST UINT8_T *s_file_h264 = NULL;
STATIC UINT32_T s_file_h264_size = 0;
STATIC BOOL_T s_media_ready = FALSE;
STATIC UINT32_T s_file_offset = 0;
STATIC UINT32_T s_file_frame_len = 0;
STATIC UINT32_T s_file_frame_start = 0;
STATIC UINT32_T s_file_is_key = 0;
STATIC UINT64_T s_file_pts_idx = 0;

/**
 * @brief Parse one H.264 AU from demo Annex-B buffer (same layout as LINUX path)
 * @param[in] video_buf buffer base
 * @param[in] offset absolute offset
 * @param[in] buf_size bytes from offset to end
 * @param[out] is_key_frame keyframe flag
 * @param[out] frame_len AU length
 * @param[out] frame_start absolute start offset
 * @return 0 on success, -1 on failure
 */
STATIC INT_T __demo_read_one_au(CONST UINT8_T *video_buf, UINT32_T offset, UINT32_T buf_size, UINT32_T *is_key_frame,
                                UINT32_T *frame_len, UINT32_T *frame_start)
{
    UINT32_T pos = 0;
    INT_T need_calc = 0;
    UINT8_T nal_type = 0;
    INT_T idx = 0;

    if (buf_size <= 5) {
        return -1;
    }
    for (pos = 0; pos <= buf_size - 5; pos++) {
        if (video_buf[pos] == 0x00 && video_buf[pos + 1] == 0x00 && video_buf[pos + 2] == 0x00 &&
            video_buf[pos + 3] == 0x01) {
            nal_type = (UINT8_T)(video_buf[pos + 4] & 0x1f);
            if (nal_type == 0x7) {
                if (need_calc == 1) {
                    *frame_len = pos - (UINT32_T)idx;
                    return 0;
                }
                *is_key_frame = 1;
                *frame_start = offset + pos;
                need_calc = 1;
                idx = (INT_T)pos;
            } else if (nal_type == 0x1) {
                if (need_calc) {
                    *frame_len = pos - (UINT32_T)idx;
                    return 0;
                }
                *frame_start = offset + pos;
                *is_key_frame = 0;
                idx = (INT_T)pos;
                need_calc = 1;
            }
        }
    }
    *frame_len = buf_size;
    return 0;
}

/**
 * @brief Fill P2P av_info to match embedded demo bitstream
 * @return none
 */
STATIC VOID_T __demo_init_p2p_av_info_file(VOID_T)
{
    TRANS_IPC_AV_INFO_T av_info;
    OPERATE_RET rt;

    memset(&av_info, 0, sizeof(av_info));
    av_info.video_codec[eIpcStreamVideoMain] = TY_AV_CODEC_VIDEO_H264;
    av_info.fps[eIpcStreamVideoMain] = DEMO_FILE_FPS;
    av_info.gop[eIpcStreamVideoMain] = DEMO_FILE_GOP;
    av_info.bitrate[eIpcStreamVideoMain] = DEMO_FILE_BITRATE_KB;
    av_info.width[eIpcStreamVideoMain] = DEMO_FILE_WIDTH;
    av_info.height[eIpcStreamVideoMain] = DEMO_FILE_HEIGHT;
    av_info.video_codec[eIpcStreamVideoSub] = TY_AV_CODEC_VIDEO_H264;
    av_info.fps[eIpcStreamVideoSub] = DEMO_FILE_FPS;
    av_info.gop[eIpcStreamVideoSub] = DEMO_FILE_GOP;
    av_info.bitrate[eIpcStreamVideoSub] = DEMO_FILE_BITRATE_KB;
    av_info.width[eIpcStreamVideoSub] = DEMO_FILE_WIDTH;
    av_info.height[eIpcStreamVideoSub] = DEMO_FILE_HEIGHT;
    av_info.audio_codec = TY_AV_CODEC_AUDIO_PCM;
    av_info.audio_sample = TY_AUDIO_SAMPLE_8K;
    av_info.audio_databits = TY_AUDIO_DATABITS_16;
    av_info.audio_channel = TY_AUDIO_CHANNEL_MONO;

    rt = tuya_ipc_init_trans_av_info(&av_info);
    if (rt != OPRT_OK) {
        PR_ERR("tuya_ipc_init_trans_av_info failed: %d", rt);
    }
}

/**
 * @brief Load embedded demo_video.264 for P2P
 * @return none
 */
VOID_T tuya_ipc_demo_start(VOID_T)
{
    if (s_media_ready) {
        return;
    }
    s_file_h264 = demo_video_264_start;
    s_file_h264_size = (UINT32_T)(demo_video_264_end - demo_video_264_start);
    if (s_file_h264 == NULL || s_file_h264_size < 128) {
        PR_ERR("embedded demo_video.264 invalid size=%u", (UINT_T)s_file_h264_size);
        return;
    }
    s_file_offset = 0;
    s_file_frame_len = 0;
    s_file_frame_start = 0;
    s_file_is_key = 0;
    s_file_pts_idx = 0;

    __demo_init_p2p_av_info_file();
    s_media_ready = TRUE;
    PR_NOTICE("P2P file H264 ready: demo_video.264 %u bytes %ux%u@%u", (UINT_T)s_file_h264_size,
              (UINT_T)DEMO_FILE_WIDTH, (UINT_T)DEMO_FILE_HEIGHT, (UINT_T)DEMO_FILE_FPS);
}

/**
 * @brief Clean up demo resources
 * @return none
 */
VOID_T tuya_ipc_demo_end(VOID_T)
{
    s_media_ready = FALSE;
    s_file_h264 = NULL;
    s_file_h264_size = 0;
}

/**
 * @brief Signal disconnect callback
 * @return 0 on success
 */
INT_T demo_on_signal_disconnect_callback(VOID_T)
{
    return 0;
}

/**
 * @brief Get video frame from embedded demo file for P2P
 * @param[in,out] media_frame media frame
 * @return 0 on success, -1 on failure
 */
INT_T demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame)
{
    INT_T ret;
    UINT32_T rel_off;
    UINT64_T pts_ms;
    UINT32_T buf_cap = DEMO_P2P_VIDEO_BUF_SIZE;

    if (media_frame == NULL || media_frame->data == NULL || !s_media_ready || s_file_h264 == NULL) {
        return -1;
    }

    s_file_offset = s_file_frame_start + s_file_frame_len;
    if (s_file_offset >= s_file_h264_size) {
        s_file_frame_len = 0;
        s_file_frame_start = 0;
        s_file_offset = 0;
        s_file_is_key = 0;
        s_file_pts_idx = 0;
    }

    rel_off = s_file_offset;
    if (rel_off >= s_file_h264_size) {
        return -1;
    }

    ret = __demo_read_one_au(s_file_h264 + rel_off, s_file_offset, s_file_h264_size - rel_off, &s_file_is_key,
                             &s_file_frame_len, &s_file_frame_start);
    if (ret != 0 || s_file_frame_len == 0) {
        return -1;
    }
    if (s_file_frame_len > buf_cap) {
        PR_WARN("demo AU too large len=%u cap=%u", (UINT_T)s_file_frame_len, (UINT_T)buf_cap);
        return -1;
    }

    memcpy(media_frame->data, s_file_h264 + s_file_frame_start, s_file_frame_len);
    media_frame->size = s_file_frame_len;
    media_frame->type = s_file_is_key ? eVideoIFrame : eVideoPBFrame;
    pts_ms = (s_file_pts_idx * 1000ULL) / DEMO_FILE_FPS;
    s_file_pts_idx++;
    media_frame->timestamp = (UINT32_T)pts_ms;
    media_frame->pts = pts_ms * 1000ULL;

    tal_system_sleep(1000 / DEMO_FILE_FPS);
    return 0;
}

/**
 * @brief Get audio frame callback
 * @param[in,out] media_frame media frame
 * @return 0 on success
 */
INT_T demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    (VOID_T)media_frame;
    return 0;
}

#else /* live GC2145 + DVP */

#include <string.h>
#include "tal_mutex.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "board_com_api.h"
#include "tkl_fs.h"
#include "tdl_camera_manage.h"
#include "tuya_ipc_p2p.h"
#include "tkl_audio.h"
#include "tkl_gpio.h"
#include "tkl_dvp.h"
#include "resample_fixed.h"
#include "modules/g711.h"
#include "demo_media_event.h"
#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
#include "tuya_ring_buffer.h"
#endif
#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
#include "local_store.h"
#endif

extern uint64_t tuya_p2p_misc_get_current_time_ms(void);

#ifndef CAMERA_NAME
#define CAMERA_NAME "camera"
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define DEMO_SD_MOUNT       "/sdcard"
/* TuyaOS T5AI_BOARD DVP opens at 20fps; P2P media_info still advertises 25 like wukong */
#define DEMO_CAM_FPS        20
#define DEMO_AV_FPS         25
#define DEMO_CAM_WIDTH      480
#define DEMO_CAM_HEIGHT     480
#define DEMO_CAM_GOP        25
#define DEMO_CAM_BITRATE_KB 1024
#define DEMO_FRAME_BUF_SIZE (256 * 1024)
/* SDIO default group P2/P3/... (CMD=P3); camera I2C P13/P15 — no pin conflict */
#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
#define DEMO_ENABLE_LOCAL_SD 1
#else
#define DEMO_ENABLE_LOCAL_SD 0
#endif
#if defined(CAMERA_DEMO_SD_LIVE_RECORD) && (CAMERA_DEMO_SD_LIVE_RECORD == 1)
#ifndef CAMERA_DEMO_SD_RECORD_MAX_SEC
#define CAMERA_DEMO_SD_RECORD_MAX_SEC 120
#endif
#define DEMO_SD_LIVE_RECORD 1
#else
#define DEMO_SD_LIVE_RECORD 0
#endif
#define DEMO_FRAME_LOG_PERIOD 30
/* Shallow live queue: keep latest (drop oldest). OS ring consumer jumps to latest when delayed */
#define DEMO_P2P_QUEUE_DEPTH 2

/* ----- Audio uplink (mic -> G.711U -> P2P), align TuyaOS wukong -----
 * mic captures 16k/16bit/mono PCM via tkl_ai put_cb; resample to 8k;
 * G.711 mu-law encode; P2P pulls one frame via OnGetAudioFrameCallback.
 */
#define DEMO_MIC_SAMPLE_RATE  TKL_AUDIO_SAMPLE_16K
#define DEMO_MIC_DATABITS     TKL_AUDIO_DATABITS_16
#define DEMO_MIC_CHANNEL      TKL_AUDIO_CHANNEL_MONO
#define DEMO_MIC_CARD         TKL_AUDIO_TYPE_BOARD
/* Align boards/T5AI/TUYA_T5AI_BOARD: BOARD_SPEAKER_EN_PIN=GPIO28, mute-level LOW (high-enable) */
#define DEMO_SPK_GPIO           TUYA_GPIO_NUM_28
#define DEMO_SPK_GPIO_POLARITY  0
#define DEMO_SPK_VOLUME         80
#define DEMO_AUDIO_AV_FPS     25   /* P2P audio fps (align OS media_info.audio_fps=25) */
/* G.711 8k @ 25fps -> 40ms/frame = 320 samples = 320 bytes */
#define DEMO_AUDIO_FRAME_BYTES (8000 / DEMO_AUDIO_AV_FPS)
#define DEMO_AUDIO_RING_FRAMES  8
#define DEMO_AUDIO_RING_CAP     (DEMO_AUDIO_FRAME_BYTES * DEMO_AUDIO_RING_FRAMES)
#define DEMO_AUDIO_PCM_MAX      640

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    UINT32_T len;
    BOOL_T is_key;
    UINT64_T ts_ms;
} DEMO_P2P_Q_SLOT_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC MUTEX_HANDLE s_frame_mutex = NULL;
STATIC UINT8_T *s_q_pool[DEMO_P2P_QUEUE_DEPTH] = {0};
STATIC DEMO_P2P_Q_SLOT_T s_q_slot[DEMO_P2P_QUEUE_DEPTH];
STATIC UINT32_T s_q_head = 0;
STATIC UINT32_T s_q_tail = 0;
STATIC UINT32_T s_q_count = 0;
STATIC UINT32_T s_frame_slot_cap = 0;
STATIC BOOL_T s_media_ready = FALSE;
STATIC BOOL_T s_live_push_enable = FALSE;
STATIC BOOL_T s_queue_need_iframe = FALSE;
STATIC TDL_CAMERA_HANDLE_T s_cam = NULL;
STATIC UINT64_T s_frame_idx = 0;

/* Audio uplink state (mic -> G.711U -> P2P) */
STATIC MUTEX_HANDLE s_audio_mutex = NULL;
STATIC UINT8_T s_audio_ring[DEMO_AUDIO_RING_CAP];
STATIC UINT32_T s_audio_head = 0, s_audio_tail = 0, s_audio_count = 0;
STATIC BOOL_T s_audio_inited = FALSE;
STATIC BOOL_T s_mic_running = FALSE;
/* Forward decls: defined after video helpers, used by start/stop below */
STATIC OPERATE_RET __demo_audio_uplink_init(VOID_T);
STATIC VOID_T __demo_audio_uplink_deinit(VOID_T);
STATIC OPERATE_RET __demo_mic_start(VOID_T);
STATIC VOID_T __demo_mic_stop(VOID_T);
#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
STATIC RING_BUFFER_USER_HANDLE_T s_ring_w = NULL;
STATIC RING_BUFFER_USER_HANDLE_T s_ring_r = NULL;
STATIC BOOL_T s_ring_ready = FALSE;
#endif
#if DEMO_SD_LIVE_RECORD
STATIC BOOL_T s_rec_wait_iframe = FALSE;
STATIC UINT_T s_rec_wr_fail = 0;
#endif

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Drop all queued P2P video frames
 * @return none
 */
STATIC VOID_T __demo_p2p_queue_clear(VOID_T)
{
    s_q_head = 0;
    s_q_tail = 0;
    s_q_count = 0;
    s_queue_need_iframe = TRUE;
}

#if DEMO_ENABLE_LOCAL_SD
/**
 * @brief Mount SD card for local_store PB (default SDIO group P2/P3/...)
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __demo_sd_mount(VOID_T)
{
    OPERATE_RET rt;

    rt = (OPERATE_RET)tkl_fs_mount(DEMO_SD_MOUNT, DEV_SDCARD);
    if (rt != OPRT_OK) {
        PR_ERR("sd mount %s failed: %d (FAT card? SDIO=P2/P3/...)", DEMO_SD_MOUNT, (INT_T)rt);
        return rt;
    }
    PR_NOTICE("sd mount ok: %s", DEMO_SD_MOUNT);
    return OPRT_OK;
}
#endif

#if DEMO_SD_LIVE_RECORD
/**
 * @brief Start (or restart) a live SD recording segment; wait for next I-frame
 * @return none
 */
STATIC VOID_T __demo_sd_rec_start(VOID_T)
{
    OPERATE_RET rt = local_store_rec_start("live");
    if (rt != OPRT_OK) {
        PR_ERR("sd live rec start failed: %d", rt);
        s_rec_wait_iframe = FALSE;
        return;
    }
    s_rec_wait_iframe = TRUE;
}

/**
 * @brief Stop live SD recording and write day index
 * @return none
 */
STATIC VOID_T __demo_sd_rec_stop(VOID_T)
{
    s_rec_wait_iframe = FALSE;
    (VOID)local_store_rec_stop();
}

/**
 * @brief Append one live AU to SD; roll segment after MAX_SEC on I-frame
 * @param[in] data Annex-B bytes
 * @param[in] len length
 * @param[in] is_key I-frame flag
 * @return none
 */
STATIC VOID_T __demo_sd_rec_on_frame(CONST UINT8_T *data, UINT32_T len, BOOL_T is_key)
{
    if (!local_store_rec_is_open()) {
        return;
    }
    if (s_rec_wait_iframe) {
        if (!is_key) {
            return;
        }
        s_rec_wait_iframe = FALSE;
    }
    if (is_key && local_store_rec_elapsed_sec() >= (UINT32_T)CAMERA_DEMO_SD_RECORD_MAX_SEC) {
        __demo_sd_rec_stop();
        __demo_sd_rec_start();
        if (!local_store_rec_is_open() || s_rec_wait_iframe) {
            /* New segment still waiting I; current frame is I — clear wait and write */
            if (local_store_rec_is_open()) {
                s_rec_wait_iframe = FALSE;
            } else {
                return;
            }
        }
    }
    if (local_store_rec_write(data, len) != OPRT_OK) {
        s_rec_wr_fail++;
        if ((s_rec_wr_fail % 30) == 1) {
            PR_WARN("sd live rec write fail cnt=%u", s_rec_wr_fail);
        }
        /* Stop on sustained ENOSPC/IO errors to avoid log/sendto storms */
        if (s_rec_wr_fail >= 5) {
            PR_ERR("sd live rec abort after %u write fails", s_rec_wr_fail);
            s_rec_wr_fail = 0;
            __demo_sd_rec_stop();
        }
    } else {
        s_rec_wr_fail = 0;
    }
}
#endif

/**
 * @brief Push one encoded frame into live FIFO; drop oldest when full (keep latest)
 * @param[in] data frame bytes
 * @param[in] len byte length
 * @param[in] is_key TRUE for I frame
 * @param[in] ts_ms capture time ms (monotonic, TuyaOS put_frame time)
 * @return OPRT_OK on success
 * @note Align OS low-latency path: no deep backlog; after drop wait next I-frame
 */
STATIC OPERATE_RET __demo_p2p_queue_push(CONST UINT8_T *data, UINT32_T len, BOOL_T is_key, UINT64_T ts_ms)
{
    UINT8_T *dst;

    if (data == NULL || len == 0 || len > s_frame_slot_cap) {
        return OPRT_INVALID_PARM;
    }
    if (s_q_count >= DEMO_P2P_QUEUE_DEPTH) {
        /* Drop oldest to keep freshest frames (OS ring jumps to latest when delayed) */
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

/**
 * @brief Pop oldest frame for P2P media thread (FIFO order)
 * @param[out] media_frame output media frame
 * @return OPRT_OK if a frame was returned, OPRT_NOT_FOUND if queue empty
 */
STATIC OPERATE_RET __demo_p2p_queue_pop(MEDIA_FRAME *media_frame)
{
    CONST DEMO_P2P_Q_SLOT_T *slot;
    CONST UINT8_T *src;

    if (media_frame == NULL || media_frame->data == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (s_q_count == 0) {
        return OPRT_NOT_FOUND;
    }
    slot = &s_q_slot[s_q_head];
    src = s_q_pool[s_q_head];
    if (src == NULL || slot->len == 0 || slot->len > (300 * 1024)) {
        __demo_p2p_queue_clear();
        return OPRT_COM_ERROR;
    }
    memcpy(media_frame->data, src, slot->len);
    media_frame->size = slot->len;
    media_frame->type = slot->is_key ? eVideoIFrame : eVideoPBFrame;
    /* Match TuyaOS __p2p_h264_cb: same monotonic ms for pts and timestamp */
    media_frame->pts = slot->ts_ms;
    media_frame->timestamp = (UINT32_T)slot->ts_ms;
    s_q_head = (s_q_head + 1U) % DEMO_P2P_QUEUE_DEPTH;
    s_q_count--;
    return OPRT_OK;
}

/**
 * @brief Check Annex-B start code present (reject DMA-truncated garbage before P2P)
 * @param[in] data H264 frame
 * @param[in] len byte length
 * @return TRUE if at least one 00 00 01 / 00 00 00 01 prefix exists
 */
STATIC BOOL_T __demo_h264_au_has_annexb(CONST UINT8_T *data, UINT32_T len)
{
    UINT32_T i;

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

/**
 * @brief Encoded H264 frame callback from tdl_camera
 * @param[in] hdl camera handle
 * @param[in] frame encoded frame
 * @return OPRT_OK
 */
STATIC OPERATE_RET __demo_encoded_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    BOOL_T is_key;

    (VOID_T)hdl;
    if (frame == NULL || frame->data == NULL || frame->data_len == 0) {
        return OPRT_OK;
    }
    if (s_frame_mutex == NULL || s_frame_slot_cap == 0 || !s_live_push_enable) {
        /* Align TuyaOS __p2p_h264_cb: no client / not LIVE -> do not feed P2P */
        return OPRT_OK;
    }
    if (frame->data_len > s_frame_slot_cap) {
        PR_WARN("h264 frame too large: %u > %u", (UINT_T)frame->data_len, (UINT_T)s_frame_slot_cap);
        return OPRT_OK;
    }
    if (!__demo_h264_au_has_annexb((CONST UINT8_T *)frame->data, frame->data_len)) {
        STATIC UINT_T s_bad_au_cnt = 0;
        if ((s_bad_au_cnt++ % 30) == 0) {
            PR_NOTICE("drop AU without Annex-B start code len=%u cnt=%u", (UINT_T)frame->data_len,
                    s_bad_au_cnt);
        }
        return OPRT_OK;
    }
    if (!frame->is_complete) {
        STATIC UINT_T s_incomplete_cnt = 0;
        if ((s_incomplete_cnt++ % 30) == 0) {
            PR_DEBUG("drop incomplete AU len=%u total=%u cnt=%u", (UINT_T)frame->data_len,
                    (UINT_T)frame->total_frame_len, s_incomplete_cnt);
        }
        return OPRT_OK;
    }
    if (frame->total_frame_len > 0 && frame->data_len != frame->total_frame_len) {
        STATIC UINT_T s_len_mismatch_cnt = 0;
        if ((s_len_mismatch_cnt++ % 10) == 0) {
            PR_DEBUG("drop len mismatch got=%u expect=%u cnt=%u", (UINT_T)frame->data_len,
                    (UINT_T)frame->total_frame_len, s_len_mismatch_cnt);
        }
        return OPRT_OK;
    }

    /* Same as TuyaOS ai_video wukong __p2p_h264_cb: trust DVP is_i_frame for I vs P/B */
    is_key = frame->is_i_frame ? TRUE : FALSE;
    s_frame_idx++;

    tal_mutex_lock(s_frame_mutex);
    (VOID_T) __demo_p2p_queue_push((CONST UINT8_T *)frame->data, frame->data_len, is_key,
                                   tuya_p2p_misc_get_current_time_ms());
    tal_mutex_unlock(s_frame_mutex);

#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
    if (s_ring_ready && s_ring_w != NULL) {
        UINT64_T ts = tuya_p2p_misc_get_current_time_ms();
        (VOID_T) tuya_ipc_ring_buffer_append_data_with_timestamp(
            s_ring_w, (UCHAR_T *)frame->data, frame->data_len,
            is_key ? E_VIDEO_I_FRAME : E_VIDEO_PB_FRAME, ts * 1000ULL, ts);
    }
#endif

#if DEMO_SD_LIVE_RECORD
    __demo_sd_rec_on_frame((CONST UINT8_T *)frame->data, frame->data_len, is_key);
#endif

    if (is_key || (s_frame_idx % DEMO_FRAME_LOG_PERIOD) == 0) {
        PR_NOTICE("h264 enc frames=%llu len=%u i=%u q=%u", (unsigned long long)s_frame_idx, (UINT_T)frame->data_len,
                  (UINT_T)(is_key ? 1 : 0), (UINT_T)s_q_count);
    }

    return OPRT_OK;
}

/**
 * @brief Fill P2P av_info for App QUERY_VIDEO_STREAM_PARAMS / I-frame extension
 * @return none
 * @note Call after TUYA_APP_Start()/p2p_init(). Main=HIGH, Sub=STANDARD.
 */
STATIC VOID_T __demo_init_p2p_av_info(VOID_T)
{
    TRANS_IPC_AV_INFO_T av_info;
    OPERATE_RET rt;

    memset(&av_info, 0, sizeof(av_info));
    /* HIGH clarity -> main stream; fps matches TuyaOS media_info (25), sensor is DEMO_CAM_FPS */
    av_info.video_codec[eIpcStreamVideoMain] = TY_AV_CODEC_VIDEO_H264;
    av_info.fps[eIpcStreamVideoMain] = DEMO_AV_FPS;
    av_info.gop[eIpcStreamVideoMain] = DEMO_CAM_GOP;
    av_info.bitrate[eIpcStreamVideoMain] = DEMO_CAM_BITRATE_KB;
    av_info.width[eIpcStreamVideoMain] = DEMO_CAM_WIDTH;
    av_info.height[eIpcStreamVideoMain] = DEMO_CAM_HEIGHT;
    /* STANDARD clarity -> sub stream (same single sensor stream for now) */
    av_info.video_codec[eIpcStreamVideoSub] = TY_AV_CODEC_VIDEO_H264;
    av_info.fps[eIpcStreamVideoSub] = DEMO_AV_FPS;
    av_info.gop[eIpcStreamVideoSub] = DEMO_CAM_GOP;
    av_info.bitrate[eIpcStreamVideoSub] = DEMO_CAM_BITRATE_KB;
    av_info.width[eIpcStreamVideoSub] = DEMO_CAM_WIDTH;
    av_info.height[eIpcStreamVideoSub] = DEMO_CAM_HEIGHT;
    /* Audio uplink: G.711 mu-law 8k mono (align TuyaOS wukong media_info) */
    av_info.audio_codec = TY_AV_CODEC_AUDIO_G711U;
    av_info.audio_sample = TY_AUDIO_SAMPLE_8K;
    av_info.audio_databits = TY_AUDIO_DATABITS_16;
    av_info.audio_channel = TY_AUDIO_CHANNEL_MONO;

    rt = tuya_ipc_init_trans_av_info(&av_info);
    if (rt != OPRT_OK) {
        PR_ERR("tuya_ipc_init_trans_av_info failed: %d", rt);
    }
}

/**
 * @brief Open GC2145 via tdl_camera and enable H264 output
 * @return OPRT_OK on success
 * @note Leave h264_cfg disabled: tkl_dvp only applies jpeg_cfg; chip uses
 *       CONFIG_H264_QUALITY_LEVEL=MIDDLE (same as TuyaOS wukong).
 */
STATIC OPERATE_RET __demo_open_camera(VOID_T)
{
    TDL_CAMERA_CFG_T cfg;

    s_cam = tdl_camera_find_dev((CHAR_T *)CAMERA_NAME);
    if (s_cam == NULL) {
        PR_ERR("camera dev '%s' not found (enable TUYA_T5AI_BOARD_CAMERA?)", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.fps = DEMO_CAM_FPS;
    cfg.width = DEMO_CAM_WIDTH;
    cfg.height = DEMO_CAM_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_H264;
    cfg.get_encoded_frame_cb = __demo_encoded_frame_cb;
    /* Do not set h264_cfg.enable — matches OS board (JPEG quality only; H264=chip default) */

    return tdl_camera_dev_open(s_cam, &cfg);
}

/**
 * @brief Close H264 camera if open
 * @return none
 */
STATIC VOID_T __demo_close_camera(VOID_T)
{
    if (s_cam != NULL) {
        (VOID_T) tdl_camera_dev_close(s_cam);
        /* tdl/tdd camera close is NOT_SUPPORTED, so DVP DMA (chan 8) leaks and
         * reopen fails with "malloc dma fail / chan has been allocated".
         * Release DVP directly here to fix reopen after APP reconnect. */
        (VOID_T) tkl_dvp_deinit();
        s_cam = NULL;
    }
}

/**
 * @brief Initialize T5 camera buffers + P2P av_info (H264 opens on LIVE start, like OS)
 * @return none
 */
VOID_T tuya_ipc_demo_start(VOID_T)
{
    OPERATE_RET rt;

    if (s_media_ready) {
        return;
    }

    rt = tal_mutex_create_init(&s_frame_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("frame mutex create failed: %d", rt);
        return;
    }

    s_frame_idx = 0;
    s_live_push_enable = FALSE;
    __demo_p2p_queue_clear();

    for (UINT32_T i = 0; i < DEMO_P2P_QUEUE_DEPTH; i++) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
        s_q_pool[i] = (UINT8_T *)tal_psram_malloc(DEMO_FRAME_BUF_SIZE);
#else
        s_q_pool[i] = (UINT8_T *)tal_malloc(DEMO_FRAME_BUF_SIZE);
#endif
        if (s_q_pool[i] == NULL) {
            PR_ERR("alloc p2p queue slot %u failed", (UINT_T)i);
            return;
        }
    }
    s_frame_slot_cap = DEMO_FRAME_BUF_SIZE;

    /* p2p_init already done in TUYA_APP_Start(); publish real stream params for App */
    __demo_init_p2p_av_info();

#if DEMO_ENABLE_LOCAL_SD
    (VOID)__demo_sd_mount();
#endif
    demo_media_event_register();

#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
    {
        RING_BUFFER_INIT_PARAM_T rp = {0};
        rp.bitrate = DEMO_CAM_BITRATE_KB;
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

    rt = __demo_audio_uplink_init();
    if (rt != OPRT_OK) {
        PR_ERR("audio uplink init failed: %d", rt);
    }

#if DEMO_ENABLE_LOCAL_SD
    PR_NOTICE("local_store SD path: mount=%s root=/sdcard/media live_rec=%d", DEMO_SD_MOUNT,
              (INT_T)DEMO_SD_LIVE_RECORD);
#endif

    s_media_ready = TRUE;
    PR_NOTICE("tuya_ipc_demo: ready (H264 starts on LIVE, sensor %ux%u@%u, av fps %u, align OS no localStorage)",
              (UINT_T)DEMO_CAM_WIDTH, (UINT_T)DEMO_CAM_HEIGHT, (UINT_T)DEMO_CAM_FPS, (UINT_T)DEMO_AV_FPS);
}

/**
 * @brief Clean up demo resources
 * @return none
 */
VOID_T tuya_ipc_demo_end(VOID_T)
{
    s_live_push_enable = FALSE;
    __demo_mic_stop();
    __demo_close_camera();
    s_media_ready = FALSE;
    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }
    for (UINT32_T i = 0; i < DEMO_P2P_QUEUE_DEPTH; i++) {
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
    __demo_audio_uplink_deinit();
}

/**
 * @brief Align TuyaOS LIVE_VIDEO_START: open H264 and begin feeding P2P
 * @return 0 on success
 */
INT_T demo_on_live_video_start_callback(VOID_T)
{
    OPERATE_RET rt;

    if (!s_media_ready) {
        return -1;
    }
    if (s_cam == NULL) {
        rt = __demo_open_camera();
        if (rt != OPRT_OK) {
            PR_ERR("LIVE start: tdl_camera open failed: %d", rt);
            return -1;
        }
        PR_NOTICE("LIVE start: GC2145 H264 opened %ux%u@%u", (UINT_T)DEMO_CAM_WIDTH, (UINT_T)DEMO_CAM_HEIGHT,
                  (UINT_T)DEMO_CAM_FPS);
    }
#if DEMO_SD_LIVE_RECORD
    if (!local_store_rec_is_open()) {
        __demo_sd_rec_start();
    }
#endif
    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }
    if (!s_mic_running) {
        rt = __demo_mic_start();
        if (rt != OPRT_OK) {
            PR_ERR("LIVE start: mic start failed: %d", rt);
        }
    }
    s_live_push_enable = TRUE;
    PR_NOTICE("LIVE video start: push enabled");
    return 0;
}

/**
 * @brief Align TuyaOS LIVE_VIDEO_STOP: stop feed and close H264
 * @return 0 on success
 * @note Close camera first (stop encode CB), then close SD rec to avoid
 *       fwrite-after-fclose. Pause mic uplink so leftover LIVE TX can drain/clear.
 */
INT_T demo_on_live_video_stop_callback(VOID_T)
{
    s_live_push_enable = FALSE;
    if (s_frame_mutex != NULL) {
        tal_mutex_lock(s_frame_mutex);
        __demo_p2p_queue_clear();
        tal_mutex_unlock(s_frame_mutex);
    }
    __demo_close_camera();
#if DEMO_SD_LIVE_RECORD
    __demo_sd_rec_stop();
#endif
    demo_mic_uplink_pause();
    PR_NOTICE("LIVE video stop: H264 closed");
    return 0;
}

/**
 * @brief Signal disconnect callback
 * @return 0 on success
 */
INT_T demo_on_signal_disconnect_callback(VOID_T)
{
    (VOID_T) demo_on_live_video_stop_callback();
    /* Session end: release mic (OS keeps always-on AI path; demo has no such path) */
    __demo_mic_stop();
    return 0;
}

/**
 * @brief Get video frame callback for P2P
 * @param[in,out] media_frame media frame (data buffer owned by P2P session)
 * @return 0 on success, -1 if no frame yet
 */
INT_T demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame)
{
    OPERATE_RET rt;

    if (media_frame == NULL || media_frame->data == NULL || !s_media_ready) {
        tal_system_sleep(10);
        return -1;
    }

#if defined(ENABLE_IPC_RING_BUFFER) && (ENABLE_IPC_RING_BUFFER == 1)
    if (s_ring_ready && s_ring_r != NULL) {
        RING_BUFFER_NODE_T *node = tuya_ipc_ring_buffer_get_frame(s_ring_r, FALSE);
        if (node != NULL && node->raw_data != NULL && node->size > 0 && node->size <= (300 * 1024)) {
            memcpy(media_frame->data, node->raw_data, node->size);
            media_frame->size = node->size;
            media_frame->type = (node->type == E_VIDEO_I_FRAME) ? eVideoIFrame : eVideoPBFrame;
            media_frame->pts = node->timestamp;
            media_frame->timestamp = (UINT32_T)node->timestamp;
            return OPRT_OK;
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
    return OPRT_OK;
}

/* =========================================================================
 * Audio uplink: mic(16k PCM) -> resample 8k -> G.711U -> P2P pull
 * Align TuyaOS wukong: __audio_frame_put + tuya_ipc_app_audio_frame_put
 * ========================================================================= */

/**
 * @brief G.711 mu-law encode (wrap vendor linear2ulaw)
 */
STATIC VOID_T __demo_g711u_encode(const int16_t *pcm, size_t n, UINT8_T *out)
{
    size_t i;
    for (i = 0; i < n; i++) {
        out[i] = (UINT8_T)linear2ulaw((int)pcm[i]);
    }
}

/**
 * @brief Push G.711 bytes into ring; drop oldest when full (keep latest audio)
 */
STATIC VOID_T __demo_audio_ring_push(CONST UINT8_T *data, size_t len)
{
    size_t i;
    tal_mutex_lock(s_audio_mutex);
    for (i = 0; i < len; i++) {
        if (s_audio_count >= DEMO_AUDIO_RING_CAP) {
            s_audio_head = (s_audio_head + 1U) % DEMO_AUDIO_RING_CAP;
            s_audio_count--;
        }
        s_audio_ring[s_audio_tail] = data[i];
        s_audio_tail = (s_audio_tail + 1U) % DEMO_AUDIO_RING_CAP;
        s_audio_count++;
    }
    tal_mutex_unlock(s_audio_mutex);
}

/**
 * @brief mic frame callback: PCM 16k -> resample 8k -> G.711U -> ring
 */
STATIC int __demo_mic_frame_put_cb(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    static int16_t s_pcm8k[DEMO_AUDIO_PCM_MAX];
    static UINT8_T s_g711[DEMO_AUDIO_PCM_MAX];
    size_t in_frames, out_frames = 0;
    int ret;

    if (!s_mic_running || pframe == NULL || pframe->pbuf == NULL || pframe->used_size == 0) {
        return 0;
    }
    in_frames = pframe->used_size / 2U; /* 16bit mono -> samples */
    {
        STATIC UINT_T s_put_cnt = 0;
        if ((s_put_cnt++ % 100) == 0) {
        }
    }
    if (in_frames == 0 || in_frames > DEMO_AUDIO_PCM_MAX) {
        return 0;
    }
    ret = resample_to_8k_fixed((const int16_t *)pframe->pbuf, in_frames, 16000, 1, s_pcm8k, &out_frames);
    if (ret != 0 || out_frames == 0) {
        return 0;
    }
    __demo_g711u_encode(s_pcm8k, out_frames, s_g711);
    __demo_audio_ring_push(s_g711, out_frames);
    return 0;
}

/**
 * @brief Init and start mic capture (called on LIVE start)
 * @note align OS wukong_audio_input_board: tkl_ai_init + tkl_ai_start
 */
STATIC OPERATE_RET __demo_mic_start(VOID_T)
{
    TKL_AUDIO_CONFIG_T cfg;
    OPERATE_RET rt;

    memset(&cfg, 0, sizeof(cfg));
    /* Align OS wukong: enable=1 (vendor AFE/AEC). The AFE DMA path does NOT
     * starve the log UART, unlike the enable=0 raw-capture path whose DMA
     * grabbed the UART and stopped all logs after GC2145 set_ppi. enable=1
     * also makes tkl_audio set chl_num=2 (MIC1+MIC2) which AEC HARDWARE reqs. */
    cfg.enable = 1;
    cfg.card = DEMO_MIC_CARD;
    cfg.ai_chn = TKL_AI_0;
    cfg.sample = DEMO_MIC_SAMPLE_RATE;
    cfg.spk_sample = DEMO_MIC_SAMPLE_RATE;
    cfg.datebits = DEMO_MIC_DATABITS;
    cfg.channel = DEMO_MIC_CHANNEL;
    cfg.codectype = TKL_CODEC_AUDIO_PCM;
    cfg.spk_gpio = DEMO_SPK_GPIO;
    cfg.spk_gpio_polarity = DEMO_SPK_GPIO_POLARITY;
    cfg.spk_volume = DEMO_SPK_VOLUME;
    cfg.put_cb = __demo_mic_frame_put_cb;

    rt = tkl_ai_init(&cfg, 1);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_ai_init failed: %d", rt);
        return rt;
    }
    rt = tkl_ai_start(cfg.card, TKL_AI_0);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_ai_start failed: %d", rt);
        (VOID_T) tkl_ai_uninit();
        return rt;
    }
    /* Speex+RNN VAD overflows vendor audio_element stack (kf_work Hardfault on PB entry).
     * Keep BK aec_proc; do not hook tkl_ai_set_vad_aec_algorithm. */
    s_mic_running = TRUE;
    PR_NOTICE("mic started: 16k/16bit/mono PCM -> G.711U 8k (vendor AEC, no Speex/RNN)");
    return OPRT_OK;
}

/**
 * @brief Stop mic capture
 */
STATIC VOID_T __demo_mic_stop(VOID_T)
{
    if (!s_mic_running) {
        return;
    }
    s_mic_running = FALSE;
    (VOID_T) tkl_ai_stop(DEMO_MIC_CARD, TKL_AI_0);
    (VOID_T) tkl_ai_uninit();
    PR_NOTICE("mic stopped");
}

/**
 * @brief Pause mic uplink (for PB send path — free P2P/UDP buffer)
 * @return none
 */
VOID_T demo_mic_uplink_pause(VOID_T)
{
    __demo_mic_stop();
}

/**
 * @brief Init audio uplink ring + mutex (called from tuya_ipc_demo_start)
 */
STATIC OPERATE_RET __demo_audio_uplink_init(VOID_T)
{
    OPERATE_RET rt;
    if (s_audio_inited) {
        return OPRT_OK;
    }
    rt = tal_mutex_create_init(&s_audio_mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    s_audio_head = s_audio_tail = s_audio_count = 0;
    s_audio_inited = TRUE;
    return OPRT_OK;
}

/**
 * @brief Deinit audio uplink
 */
STATIC VOID_T __demo_audio_uplink_deinit(VOID_T)
{
    if (s_audio_mutex != NULL) {
        tal_mutex_lock(s_audio_mutex);
        s_audio_head = s_audio_tail = s_audio_count = 0;
        tal_mutex_unlock(s_audio_mutex);
        tal_mutex_release(s_audio_mutex);
        s_audio_mutex = NULL;
    }
    s_audio_inited = FALSE;
}

/**
 * @brief Get audio frame callback for P2P (pull one G.711U frame)
 * @param[in,out] media_frame media frame
 * @return 0 on success, -1 if no full frame yet
 */
INT_T demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    UINT32_T i;
    UINT64_T now_ms;

    if (media_frame == NULL || media_frame->data == NULL || !s_audio_inited) {
        return -1;
    }
    tal_mutex_lock(s_audio_mutex);
    if (s_audio_count < DEMO_AUDIO_FRAME_BYTES) {
        tal_mutex_unlock(s_audio_mutex);
        return -1;
    }
    for (i = 0; i < DEMO_AUDIO_FRAME_BYTES; i++) {
        ((UINT8_T *)media_frame->data)[i] = s_audio_ring[s_audio_head];
        s_audio_head = (s_audio_head + 1U) % DEMO_AUDIO_RING_CAP;
        s_audio_count--;
    }
    tal_mutex_unlock(s_audio_mutex);

    media_frame->size = DEMO_AUDIO_FRAME_BYTES;
    media_frame->type = eAudioFrame;
    now_ms = tuya_p2p_misc_get_current_time_ms();
    media_frame->pts = now_ms;
    media_frame->timestamp = (UINT32_T)now_ms;
    {
        STATIC UINT_T s_pull_cnt = 0;
        if ((s_pull_cnt++ % 100) == 0) {
        }
    }
    return OPRT_OK;
}

/* =========================================================================
 * Audio downlink: APP -> device speaker (align TuyaOS __tuya_ipc_app_rev_audio_cb)
 * G.711 mu-law decode -> resample 8k->16k -> tkl_ao_put_frame
 * ========================================================================= */
#define DEMO_DOWNLINK_G711_MAX   640
#define DEMO_DOWNLINK_PCM16K_MAX 1280

STATIC BOOL_T s_spk_active = FALSE;

/**
 * @brief Open PA + unmute DAC for downlink intercom
 * @return none
 * @note tkl_ao_set_vol drives bk_aud_dac_unmute; PA via GPIO (do not use
 *       tkl_ai_set_vol(0) on mute — that zeros mic ADC gain and kills uplink)
 */
STATIC VOID_T __demo_spk_unmute(VOID_T)
{
    OPERATE_RET ao_vol;
    TUYA_GPIO_LEVEL_E pa_on;

    ao_vol = tkl_ao_set_vol(DEMO_MIC_CARD, TKL_AO_0, NULL, DEMO_SPK_VOLUME);
    /* polarity = mute level; unmute = opposite */
    pa_on = (DEMO_SPK_GPIO_POLARITY == 0) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;
    (VOID_T) tkl_gpio_write(DEMO_SPK_GPIO, pa_on);
    PR_DEBUG("unmute ao_vol=%d gpio=%d pa_on=%d", ao_vol, DEMO_SPK_GPIO, (INT_T)pa_on);
}

/**
 * @brief Mute DAC + close PA after intercom (keep mic gain intact)
 * @return none
 */
STATIC VOID_T __demo_spk_mute(VOID_T)
{
    (VOID_T) tkl_ao_set_vol(DEMO_MIC_CARD, TKL_AO_0, NULL, 0);
    (VOID_T) tkl_gpio_write(DEMO_SPK_GPIO, (TUYA_GPIO_LEVEL_E)DEMO_SPK_GPIO_POLARITY);
}

INT_T demo_on_live_audio_start_callback(VOID_T)
{
    s_spk_active = TRUE;
    __demo_spk_unmute();
    PR_NOTICE("LIVE audio(speaker) start: downlink intercom on");
    return 0;
}

INT_T demo_on_live_audio_stop_callback(VOID_T)
{
    s_spk_active = FALSE;
    __demo_spk_mute();
    PR_NOTICE("LIVE audio(speaker) stop: downlink intercom off");
    return 0;
}

INT_T demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame)
{
    static int16_t s_pcm8k[DEMO_DOWNLINK_G711_MAX];
    static int16_t s_pcm16k[DEMO_DOWNLINK_PCM16K_MAX];
    static UINT32_T s_dl_cnt = 0;
    UINT32_T i, n;
    size_t out_frames = 0;
    int ret;
    OPERATE_RET ao_ret;
    INT32_T peak = 0;

    if (media_frame == NULL || media_frame->data == NULL || media_frame->size == 0 || !s_spk_active) {
        return 0;
    }
    n = media_frame->size;
    if (n > DEMO_DOWNLINK_G711_MAX) {
        n = DEMO_DOWNLINK_G711_MAX;
    }
    /* 1. G.711 mu-law decode -> 8k PCM */
    for (i = 0; i < n; i++) {
        s_pcm8k[i] = (int16_t)ulaw2linear((int)((CONST UINT8_T *)media_frame->data)[i]);
        if (s_pcm8k[i] > peak) {
            peak = s_pcm8k[i];
        } else if ((-s_pcm8k[i]) > peak) {
            peak = -s_pcm8k[i];
        }
    }
    /* 2. resample 8k -> 16k (spk path is 16k, align OS) */
    ret = resample_to_16k_fixed(s_pcm8k, (size_t)n, 8000, 1, s_pcm16k, &out_frames);
    if (ret != 0 || out_frames == 0) {
        PR_ERR("resample fail ret=%d in=%u out=%u", ret, n, (UINT32_T)out_frames);
        return 0;
    }
    /* 3. play to speaker (spk was initialized during tkl_ai_init, SPK_TYPE_ONBOARD) */
    TKL_AUDIO_FRAME_INFO_T frame;
    memset(&frame, 0, sizeof(frame));
    frame.pbuf = (CHAR_T *)s_pcm16k;
    frame.used_size = (uint32_t)(out_frames * 2);
    ao_ret = tkl_ao_put_frame(0, 0, NULL, &frame);
    s_dl_cnt++;
    if (s_dl_cnt <= 3 || (s_dl_cnt % 50) == 1 || ao_ret != OPRT_OK || peak < 200) {
        CONST UINT8_T *raw = (CONST UINT8_T *)media_frame->data;
        UINT32_T ff_cnt = 0;
        UINT32_T j;
        for (j = 0; j < n; j++) {
            if (raw[j] == 0xFFu) {
                ff_cnt++;
            }
        }
        /* mu-law silence is 0xFF; dump head to distinguish App silence vs wrong payload */
    }
    return 0;
}

#endif /* CONFIG_CAMERA_DEMO_P2P_FILE_H264 */

#endif /* OPERATING_SYSTEM == SYSTEM_LINUX */
