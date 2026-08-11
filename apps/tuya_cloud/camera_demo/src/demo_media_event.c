/**
 * @file demo_media_event.c
 * @brief Demo MEDIA_STREAM event_cb: local_store PB query + Annex-B frame send
 * @version 1.4
 * @date 2026-08-07
 * @copyright Copyright (c) Tuya Inc.
 */
#include "demo_media_event.h"
#include "tuya_ipc_demo.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_time_service.h"
#include "tkl_fs.h"
#include "tuya_error_code.h"
#include "tuya_ipc_media_stream_event.h"
#include "tuya_ipc_media_stream.h"
#include "tuya_ipc_media_adapter.h"
#include <string.h>
#include <stdio.h>

#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
#include "local_store.h"
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define DEMO_PB_DAY_SEG_MAX 64
#define DEMO_PB_FPS 20
#define DEMO_PB_FRAME_SLEEP_MS (1000 / DEMO_PB_FPS)
#define DEMO_PB_SEND_RETRY_MS 20
/* Fall further behind than this and the anchor is re-based instead of bursting */
#define DEMO_PB_PACE_RESYNC_MS 1000
#define DEMO_PB_STOP_WAIT_MS 2000
#define DEMO_PB_SEED_LEAF "pb_demo.h264"
#if OPERATING_SYSTEM == SYSTEM_LINUX
#define DEMO_PB_SEED_SRC "./demo_video.264"
#else
#define DEMO_PB_SEED_SRC "/sdcard/pb_seed.h264"
#endif
#define DEMO_PB_SEED_DURATION_SEC 120
#define DEMO_PB_PATH_MAX 256
/* Stream from SD like OS: sliding window, never load whole file into RAM */
#define DEMO_PB_SLIDE_BUF (128 * 1024)
#define DEMO_PB_FILL_CHUNK (32 * 1024)
#define DEMO_PB_WIDTH 480
#define DEMO_PB_HEIGHT 480
#define DEMO_PB_NEED_MORE (-2)

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static THREAD_HANDLE s_pb_thread = NULL;
static BOOL_T s_pb_running = FALSE;
static BOOL_T s_pb_alive = FALSE;
static BOOL_T s_pb_pause = FALSE;
static uint32_t s_pb_play_ts = 0;
static uint32_t s_pb_speed = 1; /* 1=1x, App SET_SPEED */
static uint32_t s_pb_seg_start = 0;
static uint32_t s_pb_seg_end = 0;
/* event_cb return: App re-START same seg without mid playTime */
#define DEMO_PB_START_IGNORED 1

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Find next Annex-B NAL start code
 * @param[in] p buffer
 * @param[in] len length
 * @param[in] from search offset
 * @param[out] sc_len 3 or 4
 * @return offset of start code, or -1
 */
static int __pb_find_start(const uint8_t *p, uint32_t len, uint32_t from, uint32_t *sc_len)
{
    uint32_t i;

    if (p == NULL || sc_len == NULL || len < 4) {
        return -1;
    }
    for (i = from; i + 3 < len; i++) {
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) {
            *sc_len = 3;
            return (int)i;
        }
        if (i + 4 < len && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1) {
            *sc_len = 4;
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Parse one access unit from a sliding buffer
 * @param[in] video_buf buffer base
 * @param[in] offset current offset
 * @param[in] buf_size valid bytes in buffer
 * @param[in] at_eof TRUE if no more file data behind buf
 * @param[out] is_key_frame I-frame flag
 * @param[out] frame_len AU length
 * @param[out] frame_start AU start offset
 * @return 0 ok, -1 no start code / end, DEMO_PB_NEED_MORE need refill
 */
static int __pb_read_one_frame(const uint8_t *video_buf, uint32_t offset, uint32_t buf_size, BOOL_T at_eof,
                                 uint32_t *is_key_frame, uint32_t *frame_len, uint32_t *frame_start)
{
    int sc0, sc1;
    uint32_t sc_len0 = 0, sc_len1 = 0;
    uint32_t nal_off;
    uint8_t nal_type;
    BOOL_T saw_vcl = FALSE;

    if (video_buf == NULL || offset >= buf_size || is_key_frame == NULL || frame_len == NULL || frame_start == NULL) {
        return -1;
    }
    *is_key_frame = 0;
    sc0 = __pb_find_start(video_buf, buf_size, offset, &sc_len0);
    if (sc0 < 0) {
        return at_eof ? -1 : DEMO_PB_NEED_MORE;
    }
    *frame_start = (uint32_t)sc0;
    nal_off = (uint32_t)sc0 + sc_len0;
    if (nal_off >= buf_size) {
        return at_eof ? -1 : DEMO_PB_NEED_MORE;
    }
    nal_type = (uint8_t)(video_buf[nal_off] & 0x1f);
    if (nal_type == 5 || nal_type == 7) {
        *is_key_frame = 1;
    }
    if (nal_type >= 1 && nal_type <= 5) {
        saw_vcl = TRUE;
    }

    for (;;) {
        sc1 = __pb_find_start(video_buf, buf_size, nal_off + 1, &sc_len1);
        if (sc1 < 0) {
            if (!at_eof) {
                return DEMO_PB_NEED_MORE;
            }
            *frame_len = buf_size - (uint32_t)sc0;
            return (*frame_len > 0) ? 0 : -1;
        }
        nal_off = (uint32_t)sc1 + sc_len1;
        if (nal_off >= buf_size) {
            if (!at_eof) {
                return DEMO_PB_NEED_MORE;
            }
            *frame_len = buf_size - (uint32_t)sc0;
            return 0;
        }
        nal_type = (uint8_t)(video_buf[nal_off] & 0x1f);
        if (nal_type == 7 || nal_type == 8) {
            if (saw_vcl) {
                *frame_len = (uint32_t)sc1 - (uint32_t)sc0;
                return 0;
            }
            if (nal_type == 7) {
                *is_key_frame = 1;
            }
            continue;
        }
        if (nal_type >= 1 && nal_type <= 5) {
            if (saw_vcl) {
                *frame_len = (uint32_t)sc1 - (uint32_t)sc0;
                return 0;
            }
            saw_vcl = TRUE;
            if (nal_type == 5) {
                *is_key_frame = 1;
            }
            continue;
        }
        if (saw_vcl) {
            *frame_len = (uint32_t)sc1 - (uint32_t)sc0;
            return 0;
        }
    }
}

/**
 * @brief Compact used bytes and append more from file into sliding buffer
 * @param[in] fp open file
 * @param[in,out] buf slide buffer
 * @param[in] cap buffer capacity
 * @param[in,out] valid valid byte count
 * @param[in,out] used consumed offset
 * @param[out] at_eof set TRUE when file read returns 0
 * @return OPRT_OK on success
 */
static OPERATE_RET __pb_slide_fill(TUYA_FILE fp, uint8_t *buf, uint32_t cap, uint32_t *valid, uint32_t *used,
                                   BOOL_T *at_eof)
{
    uint32_t keep;
    int n;

    if (fp == NULL || buf == NULL || valid == NULL || used == NULL || at_eof == NULL || cap == 0) {
        return OPRT_INVALID_PARM;
    }
    if (*used > *valid) {
        *used = *valid;
    }
    keep = *valid - *used;
    if (keep > 0 && *used > 0) {
        memmove(buf, buf + *used, keep);
    }
    *valid = keep;
    *used = 0;

    while (*valid + DEMO_PB_FILL_CHUNK <= cap && !*at_eof) {
        n = tkl_fread(buf + *valid, (int)DEMO_PB_FILL_CHUNK, fp);
        if (n <= 0) {
            *at_eof = TRUE;
            break;
        }
        *valid += (uint32_t)n;
    }
    /* If still room and not eof, try one smaller fill for last bytes */
    if (!*at_eof && *valid < cap) {
        n = tkl_fread(buf + *valid, (int)(cap - *valid), fp);
        if (n <= 0) {
            *at_eof = TRUE;
        } else {
            *valid += (uint32_t)n;
        }
    }
    return OPRT_OK;
}

/**
 * @brief Open PB source path for play_ts (local_store segment or seed fallback)
 * @param[in] play_ts App play time
 * @param[out] path path buffer
 * @param[in] path_len path buffer size
 * @param[out] seg_start segment start epoch (0 if unknown)
 * @param[out] seg_end segment end epoch (0 if unknown)
 * @return OPRT_OK on success
 */
static OPERATE_RET __pb_resolve_path(uint32_t play_ts, char *path, uint32_t path_len, uint32_t *seg_start,
                                     uint32_t *seg_end)
{
    if (path == NULL || path_len == 0) {
        return OPRT_INVALID_PARM;
    }
    path[0] = '\0';
    if (seg_start != NULL) {
        *seg_start = 0;
    }
    if (seg_end != NULL) {
        *seg_end = 0;
    }

#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
    {
        LOCAL_STORE_SEG_T seg;
        OPERATE_RET rt;

        memset(&seg, 0, sizeof(seg));
        if (play_ts == 0) {
            play_ts = (uint32_t)tal_time_get_posix();
        }
        rt = local_store_find_by_play_ts(play_ts, &seg, path, path_len);
        if (rt == OPRT_OK) {
            if (seg_start != NULL) {
                *seg_start = seg.start_ts;
            }
            if (seg_end != NULL) {
                *seg_end = seg.end_ts;
            }
            PR_NOTICE("pb open segment %s [%u,%u] play_ts=%u", path, seg.start_ts, seg.end_ts, play_ts);
            return OPRT_OK;
        }
        PR_WARN("pb find ts=%u failed: %d, fallback %s", play_ts, rt, DEMO_PB_SEED_SRC);
    }
#else
    (void)play_ts;
#endif
    snprintf(path, path_len, "%s", DEMO_PB_SEED_SRC);
    return OPRT_OK;
}

/**
 * @brief Compute frame sleep for current PB speed
 * @return sleep ms (>=5)
 */
static uint32_t __pb_frame_period_ms(void)
{
    uint32_t speed = s_pb_speed;
    uint32_t ms;

    if (speed == 0) {
        speed = 1;
    }
    ms = DEMO_PB_FRAME_SLEEP_MS / speed;
    if (ms < 5) {
        ms = 5;
    }
    return ms;
}

/**
 * @brief Sleep until the next frame is due, measured from the stream anchor
 *
 * Sleeping a fixed period per frame adds the SD read and P2P send cost to
 * every interval, so the stream drifts slower than realtime (measured ~0.85x
 * at 20 fps) until the App starves and drops the session. Pace against a
 * deadline instead and only sleep the remainder.
 *
 * @param[in,out] anchor_ms stream anchor, re-based when far behind
 * @param[in] frames_done frames already handed to the send API
 */
static void __pb_pace_to_frame(uint32_t *anchor_ms, uint32_t frames_done)
{
    uint32_t elapsed = frames_done * __pb_frame_period_ms();
    uint32_t now = tal_system_get_millisecond();
    int32_t remain = (int32_t)(*anchor_ms + elapsed - now);

    if (remain > 0) {
        tal_system_sleep((uint32_t)remain);
        return;
    }
    /*
     * A long stall (SD retry, TX buffer full) must not turn into a catch-up
     * burst that floods the App, so write the lost time off and pace from here.
     */
    if (remain < -(int32_t)DEMO_PB_PACE_RESYNC_MS) {
        *anchor_ms = now - elapsed;
    }
}

/**
 * @brief Playback send thread: stream Annex-B from SD (tkl_fread), frame by frame
 * @param[in] arg unused
 * @return none
 * @note Align OS local-storage PB: do not load entire file into RAM.
 *       On OPRT_RESOURCE_NOT_READY (-23) sleep and retry same frame (P2P buffer full).
 *       Seek: skip (play_ts-seg_start)*fps frames then wait next I-frame before send.
 */
/**
 * @brief Open the recording that follows the segment just finished
 * @param[in,out] fp current file, closed and replaced when a next segment exists
 * @param[in,out] seg_start start of the current segment, updated on success
 * @param[in,out] seg_end end of the current segment, updated on success
 * @return TRUE when the next segment is open, FALSE when the day has no more
 * @note The App timeline is continuous across recordings, so ending the send
 *       thread at every file boundary blanks the picture once a segment runs
 *       out. Roll into the next one instead and only finish at the last.
 */
static BOOL_T __pb_open_next_segment(TUYA_FILE *fp, uint32_t *seg_start, uint32_t *seg_end)
{
    char path[DEMO_PB_PATH_MAX];
    uint32_t next_start = 0;
    uint32_t next_end = 0;
    TUYA_FILE next_fp;

    if (fp == NULL || seg_start == NULL || seg_end == NULL || *seg_end == 0) {
        return FALSE;
    }

    /*
     * Look the successor up in the day index rather than resolving seg_end + 1:
     * local_store_find_by_play_ts() returns the *nearest* segment, so a gap
     * between recordings makes the one that just ended win and playback stops.
     *
     * The day list is a few KB, far too much for the send thread's stack, so
     * take it from the heap: this runs once per segment boundary, never in the
     * per-frame path.
     */
    {
        LOCAL_STORE_SEG_T *segs;
        uint32_t count = DEMO_PB_DAY_SEG_MAX;
        POSIX_TM_S tm;
        uint32_t i;

        memset(&tm, 0, sizeof(tm));
        if (tal_time_get_local_time_custom((TIME_T)*seg_start, &tm) != OPRT_OK) {
            return FALSE;
        }
        segs = (LOCAL_STORE_SEG_T *)tal_malloc(sizeof(LOCAL_STORE_SEG_T) * DEMO_PB_DAY_SEG_MAX);
        if (segs == NULL) {
            PR_ERR("pb next segment: day list alloc failed");
            return FALSE;
        }
        if (local_store_query_day((uint32_t)(tm.tm_year + 1900), (uint32_t)(tm.tm_mon + 1), (uint32_t)tm.tm_mday,
                                  segs, &count) != OPRT_OK) {
            tal_free(segs);
            return FALSE;
        }
        for (i = 0; i < count; i++) {
            if (segs[i].start_ts > *seg_start && (next_start == 0 || segs[i].start_ts < next_start)) {
                next_start = segs[i].start_ts;
            }
        }
        tal_free(segs);
        if (next_start == 0) {
            return FALSE; /* last recording of the day */
        }
    }

    memset(path, 0, sizeof(path));
    if (__pb_resolve_path(next_start, path, sizeof(path), &next_start, &next_end) != OPRT_OK) {
        return FALSE;
    }
    if (next_start == 0 || next_start <= *seg_start) {
        return FALSE;
    }

    next_fp = tkl_fopen(path, "rb");
    if (next_fp == NULL) {
        PR_ERR("pb next segment open failed: %s", path);
        return FALSE;
    }

    if (*fp != NULL) {
        tkl_fclose(*fp);
    }
    *fp = next_fp;
    *seg_start = next_start;
    *seg_end = next_end;
    s_pb_seg_start = next_start;
    s_pb_seg_end = next_end;
    PR_NOTICE("pb continue next segment %s [%u,%u]", path, next_start, next_end);

    return TRUE;
}

static void __pb_send_thread(void *arg)
{
    uint8_t *slide = NULL;
    uint32_t valid = 0;
    uint32_t used = 0;
    uint32_t frame_cnt = 0;
    uint32_t fail_cnt = 0;
    uint32_t file_frame_idx = 0;
    uint32_t skip_frames = 0;
    uint32_t seg_start = 0;
    uint32_t seg_end = 0;
    uint64_t base_ms;
    uint32_t pace_base_ms;
    char path[DEMO_PB_PATH_MAX];
    TUYA_FILE fp = NULL;
    OPERATE_RET rt;
    uint32_t play_ts = s_pb_play_ts;
    BOOL_T at_eof = FALSE;
    BOOL_T need_fill = TRUE;
    BOOL_T send_armed = FALSE;

    (void)arg;
    s_pb_alive = TRUE;
    memset(path, 0, sizeof(path));

    /* Free UDP/P2P TX while replaying (mic uplink competes with PB) */
    demo_mic_uplink_pause();

    rt = __pb_resolve_path(play_ts, path, sizeof(path), &seg_start, &seg_end);
    if (rt != OPRT_OK) {
        PR_ERR("pb resolve path failed: %d", rt);
        goto __pb_exit;
    }
    s_pb_seg_start = seg_start;
    s_pb_seg_end = seg_end;

    fp = tkl_fopen(path, "rb");
    if (fp == NULL) {
        PR_ERR("pb fopen failed: %s", path);
        goto __pb_exit;
    }

    slide = (uint8_t *)Malloc(DEMO_PB_SLIDE_BUF);
    if (slide == NULL) {
        PR_ERR("pb slide buf malloc %u failed", (uint32_t)DEMO_PB_SLIDE_BUF);
        goto __pb_exit;
    }

    /* Intra-file seek only when App asks a mid-segment time.
     * playTime at/after end (or at start) => play from beginning of file. */
    if (seg_start != 0 && seg_end > seg_start && play_ts > seg_start && play_ts < seg_end) {
        skip_frames = (play_ts - seg_start) * DEMO_PB_FPS;
    }
    base_ms = tal_system_get_millisecond();
    PR_NOTICE("pb stream start path=%s slide=%u seek_skip=%u play_ts=%u seg=[%u,%u] speed=%u", path,
              (uint32_t)DEMO_PB_SLIDE_BUF, skip_frames, play_ts, seg_start, seg_end, s_pb_speed);

    /* Anchored on the first frame actually sent, so the seek scan ahead of it
     * does not count as playback time. */
    pace_base_ms = 0;

    while (s_pb_running) {
        uint32_t is_key = 0;
        uint32_t frame_len = 0;
        uint32_t frame_start = 0;
        MEDIA_VIDEO_FRAME_T vf;
        int pr;

        if (s_pb_pause) {
            tal_system_sleep(40);
            continue;
        }

        if (need_fill || (valid - used) < DEMO_PB_FILL_CHUNK) {
            (void)__pb_slide_fill(fp, slide, DEMO_PB_SLIDE_BUF, &valid, &used, &at_eof);
            need_fill = FALSE;
        }

        pr = __pb_read_one_frame(slide, used, valid, at_eof, &is_key, &frame_len, &frame_start);
        if (pr == DEMO_PB_NEED_MORE) {
            if (at_eof) {
                if (__pb_open_next_segment(&fp, &seg_start, &seg_end)) {
                    valid = 0;
                    used = 0;
                    file_frame_idx = 0;
                    skip_frames = 0;
                    at_eof = FALSE;
                    need_fill = TRUE;
                    send_armed = FALSE;
                    continue;
                }
                PR_NOTICE("pb EOF frames=%u", frame_cnt);
                (void)tuya_ipc_media_playback_send_finish(0);
                break;
            }
            if (used == 0 && valid == DEMO_PB_SLIDE_BUF) {
                PR_ERR("pb AU exceeds slide buf %u, abort", (uint32_t)DEMO_PB_SLIDE_BUF);
                break;
            }
            need_fill = TRUE;
            continue;
        }
        if (pr != 0) {
            if (__pb_open_next_segment(&fp, &seg_start, &seg_end)) {
                valid = 0;
                used = 0;
                file_frame_idx = 0;
                skip_frames = 0;
                at_eof = FALSE;
                need_fill = TRUE;
                send_armed = FALSE;
                continue;
            }
            PR_NOTICE("pb EOF frames=%u", frame_cnt);
            (void)tuya_ipc_media_playback_send_finish(0);
            break;
        }

        file_frame_idx++;
        used = frame_start + frame_len;

        /* Intra-file seek: drop frames before target, then arm on next I-frame */
        if (file_frame_idx <= skip_frames) {
            continue;
        }
        if (!send_armed) {
            if (!is_key) {
                continue;
            }
            send_armed = TRUE;
            PR_NOTICE("pb seek armed file_idx=%u key size=%u", file_frame_idx, frame_len);
        }

        memset(&vf, 0, sizeof(vf));
        vf.video_codec = TUYA_CODEC_VIDEO_H264;
        vf.video_frame_type = is_key ? TUYA_VIDEO_FRAME_IFRAME : TUYA_VIDEO_FRAME_PBFRAME;
        vf.width = DEMO_PB_WIDTH;
        vf.height = DEMO_PB_HEIGHT;
        vf.fps = DEMO_PB_FPS;
        vf.p_video_buf = slide + frame_start;
        vf.buf_len = frame_len;
        /* Wall-clock-ish ms so App timeline can map progress within the day */
        if (seg_start != 0) {
            vf.timestamp = ((uint64_t)seg_start * 1000ULL) + (((uint64_t)file_frame_idx * 1000ULL) / DEMO_PB_FPS);
        } else {
            vf.timestamp = base_ms + (((uint64_t)frame_cnt * 1000ULL) / DEMO_PB_FPS);
        }
        vf.pts = vf.timestamp * 1000ULL;

        /* P2P contract: -23 means TX buffer full — retry same frame */
        rt = OPRT_COM_ERROR;
        for (;;) {
            if (!s_pb_running) {
                break;
            }
            if (s_pb_pause) {
                tal_system_sleep(40);
                continue;
            }
            rt = tuya_ipc_media_playback_send_video_frame(0, &vf);
            if (rt == OPRT_OK) {
                fail_cnt = 0;
                break;
            }
            if (rt == OPRT_RESOURCE_NOT_READY) {
                fail_cnt++;
                if ((fail_cnt % 50) == 1) {
                    PR_WARN("pb send wait buffer rt=%d cnt=%u fail=%u", rt, frame_cnt, fail_cnt);
                }
                tal_system_sleep(DEMO_PB_SEND_RETRY_MS);
                continue;
            }
            PR_WARN("pb send fail rt=%d cnt=%u, skip frame", rt, frame_cnt);
            break;
        }
        if (!s_pb_running) {
            break;
        }
        if (rt == OPRT_OK && (frame_cnt == 0 || (frame_cnt % 30) == 0)) {
            PR_NOTICE("pb send ok cnt=%u key=%u size=%u ts=%llu file_idx=%u", frame_cnt, is_key, frame_len,
                      (unsigned long long)vf.timestamp, file_frame_idx);
        }

        frame_cnt++;
        if (pace_base_ms == 0) {
            pace_base_ms = tal_system_get_millisecond();
        }
        __pb_pace_to_frame(&pace_base_ms, frame_cnt);
    }

__pb_exit:
    if (slide != NULL) {
        Free(slide);
    }
    if (fp != NULL) {
        tkl_fclose(fp);
    }
    s_pb_running = FALSE;
    s_pb_thread = NULL;
    s_pb_alive = FALSE;
    PR_NOTICE("pb send thread exit frames=%u", frame_cnt);
}

/**
 * @brief Stop PB send thread and wait for exit
 * @return none
 */
static void __pb_stop(void)
{
    uint32_t waited = 0;

    s_pb_running = FALSE;
    s_pb_pause = FALSE;
    while (s_pb_alive && waited < DEMO_PB_STOP_WAIT_MS) {
        tal_system_sleep(20);
        waited += 20;
    }
    if (s_pb_thread != NULL) {
        THREAD_HANDLE h = s_pb_thread;
        s_pb_thread = NULL;
        tal_thread_delete(h);
    }
    s_pb_alive = FALSE;
    s_pb_seg_start = 0;
    s_pb_seg_end = 0;
}

/**
 * @brief Start PB send thread
 * @param[in] play_ts App requested play time (epoch sec), 0=now
 * @return none
 */
static void __pb_start(uint32_t play_ts)
{
    THREAD_CFG_T cfg;

    if (s_pb_running || s_pb_alive) {
        PR_NOTICE("pb restart: stop previous then start ts=%u", play_ts);
        __pb_stop();
    }
    /* Flush LIVE/PB TX residue only on real (re)start */
    tuya_ipc_media_p2p_clear_send();
    s_pb_play_ts = play_ts;
    s_pb_running = TRUE;
    s_pb_pause = FALSE;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stackDepth = 12288;
    cfg.priority = THREAD_PRIO_3;
    cfg.thrdname = "p2p_pb_send";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cfg.psram_mode = 1;
#endif
    if (tal_thread_create_and_start(&s_pb_thread, NULL, NULL, __pb_send_thread, NULL, &cfg) != OPRT_OK) {
        PR_ERR("pb thread create failed");
        s_pb_running = FALSE;
        s_pb_alive = FALSE;
        s_pb_thread = NULL;
    }
}

/**
 * @brief Handle P2P media stream events
 * @param[in] device device index
 * @param[in] channel channel index
 * @param[in] event event id
 * @param[in] args event payload
 * @return 0 on success
 */
static int __demo_media_stream_event_cb(const int device, const int channel,
                                          const MEDIA_STREAM_EVENT_E event, void *args)
{
    (void)device;
    (void)channel;

    switch (event) {
    case MEDIA_STREAM_LIVE_VIDEO_START:
    case MEDIA_STREAM_LIVE_VIDEO_STOP:
    case MEDIA_STREAM_SPEAKER_START:
    case MEDIA_STREAM_SPEAKER_STOP:
        break;

#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
    case MEDIA_STREAM_PLAYBACK_QUERY_MONTH_SIMPLIFY: {
        C2C_TRANS_QUERY_PB_MONTH_RESP *month = (C2C_TRANS_QUERY_PB_MONTH_RESP *)args;
        uint32_t bitmap = 0;
        OPERATE_RET rt;

        if (month == NULL) {
            return OPRT_INVALID_PARM;
        }
        rt = local_store_query_month(month->year, month->month, &bitmap);
        if (rt != OPRT_OK) {
            month->day = 0;
            return (int)rt;
        }
        month->day = bitmap;
        PR_NOTICE("pb month %u-%02u bitmap=0x%x", month->year, month->month, bitmap);
        break;
    }
    case MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS: {
        C2C_TRANS_QUERY_PB_DAY_RESP *day = (C2C_TRANS_QUERY_PB_DAY_RESP *)args;
        LOCAL_STORE_SEG_T segs[DEMO_PB_DAY_SEG_MAX];
        uint32_t count = DEMO_PB_DAY_SEG_MAX;
        OPERATE_RET rt;
        uint32_t i;
        uint32_t arr_bytes;
        PLAY_BACK_ALARM_INFO_ARR *arr;

        if (day == NULL) {
            return OPRT_INVALID_PARM;
        }
        day->alarm_arr = NULL;
        rt = local_store_query_day(day->year, day->month, day->day, segs, &count);
        if (rt != OPRT_OK) {
            return (int)rt;
        }
        arr_bytes = (uint32_t)(sizeof(PLAY_BACK_ALARM_INFO_ARR) + count * sizeof(PLAY_BACK_ALARM_FRAGMENT));
        arr = (PLAY_BACK_ALARM_INFO_ARR *)Malloc(arr_bytes);
        if (arr == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(arr, 0, arr_bytes);
        arr->file_count = count;
        for (i = 0; i < count; i++) {
            arr->file_arr[i].video_type = 0;
            arr->file_arr[i].type = segs[i].type;
            arr->file_arr[i].time_sect.start_timestamp = segs[i].start_ts;
            arr->file_arr[i].time_sect.end_timestamp = segs[i].end_ts;
        }
        day->alarm_arr = arr;
        PR_NOTICE("pb day %u-%02u-%02u segs=%u", day->year, day->month, day->day, count);
        break;
    }
    case MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_WITH_ENCRYPT: {
        C2C_TRANS_QUERY_PB_DAY_WITH_ENCRYPT_RESP *day = (C2C_TRANS_QUERY_PB_DAY_WITH_ENCRYPT_RESP *)args;
        LOCAL_STORE_SEG_T segs[DEMO_PB_DAY_SEG_MAX];
        uint32_t count = DEMO_PB_DAY_SEG_MAX;
        OPERATE_RET rt;
        uint32_t i;
        uint32_t arr_bytes;
        PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR *arr;

        if (day == NULL) {
            return OPRT_INVALID_PARM;
        }
        day->alarm_arr = NULL;
        rt = local_store_query_day(day->year, day->month, day->day, segs, &count);
        if (rt != OPRT_OK) {
            return (int)rt;
        }
        arr_bytes = (uint32_t)(sizeof(PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR) +
                               count * sizeof(PLAY_BACK_FILE_INFOS_WITH_ENCRYPT));
        arr = (PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR *)Malloc(arr_bytes);
        if (arr == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(arr, 0, arr_bytes);
        arr->file_count = count;
        for (i = 0; i < count; i++) {
            arr->file_arr[i].video_type = 0;
            arr->file_arr[i].type = segs[i].type;
            arr->file_arr[i].time_sect.start_timestamp = segs[i].start_ts;
            arr->file_arr[i].time_sect.end_timestamp = segs[i].end_ts;
            arr->file_arr[i].encrypt = 0;
        }
        day->alarm_arr = arr;
        PR_NOTICE("pb day(encrypt) %u-%02u-%02u segs=%u", day->year, day->month, day->day, count);
        break;
    }
#endif

    case MEDIA_STREAM_PLAYBACK_START_TS: {
        uint32_t play_ts = 0;
        uint32_t new_start = 0;
        uint32_t new_end = 0;
        char path[DEMO_PB_PATH_MAX];
        BOOL_T mid_seek = FALSE;
        BOOL_T same_seg = FALSE;

        if (args != NULL) {
            C2C_TRANS_CTRL_PB_START *pb = (C2C_TRANS_CTRL_PB_START *)args;

            /* The P2P layer owns the wire layout and hands over a filled-in
             * struct, so playTime is the position the user scrubbed to. Fall
             * back to the section start only when the App leaves it unset. */
            play_ts = pb->playTime;
            if (play_ts == 0) {
                play_ts = pb->time_sect.start_timestamp;
            }
            PR_NOTICE("pb START sect=[%u,%u] play_ts=%u", pb->time_sect.start_timestamp,
                      pb->time_sect.end_timestamp, play_ts);
        } else {
            PR_NOTICE("pb START (no args)");
        }

        memset(path, 0, sizeof(path));
        (void)__pb_resolve_path(play_ts, path, sizeof(path), &new_start, &new_end);
        mid_seek = (new_start != 0 && new_end > new_start && play_ts > new_start && play_ts < new_end);
        same_seg = ((s_pb_running || s_pb_alive) && s_pb_seg_start != 0 && new_start == s_pb_seg_start &&
                    new_end == s_pb_seg_end);
        /* App scrub with playTime=seg.end and no valid field5: keep playing */
        if (same_seg && !mid_seek) {
            PR_NOTICE("pb START ignore duplicate same seg=[%u,%u] play_ts=%u (keep playing)", new_start, new_end,
                      play_ts);
            return DEMO_PB_START_IGNORED;
        }

        __pb_start(play_ts);
        break;
    }
    case MEDIA_STREAM_PLAYBACK_PAUSE:
        s_pb_pause = TRUE;
        PR_NOTICE("pb PAUSE");
        break;
    case MEDIA_STREAM_PLAYBACK_RESUME:
        s_pb_pause = FALSE;
        PR_NOTICE("pb RESUME");
        break;
    case MEDIA_STREAM_PLAYBACK_STOP:
        PR_NOTICE("pb STOP");
        __pb_stop();
        (void)tuya_ipc_media_playback_send_finish(0);
        break;
    case MEDIA_STREAM_PLAYBACK_MUTE:
    case MEDIA_STREAM_PLAYBACK_UNMUTE:
        PR_NOTICE("pb ctrl event=%d", (int)event);
        break;
    case MEDIA_STREAM_PLAYBACK_SET_SPEED: {
        uint32_t speed = 1;
        if (args != NULL) {
            C2C_TRANS_CTRL_PB_SET_SPEED *sp = (C2C_TRANS_CTRL_PB_SET_SPEED *)args;
            speed = sp->speed;
        }
        if (speed == 0) {
            speed = 1;
        }
        if (speed > 16) {
            speed = 16;
        }
        s_pb_speed = speed;
        PR_NOTICE("pb SET_SPEED raw/clamped=%u period=%ums", s_pb_speed, __pb_frame_period_ms());
        break;
    }
    default:
        break;
    }
    return 0;
}

/**
 * @brief Register demo media stream event callback and seed PB demo file
 * @return none
 */
void demo_media_event_register(void)
{
    OPERATE_RET rt = tuya_ipc_media_stream_register_event_cb(__demo_media_stream_event_cb);
    if (rt != OPRT_OK) {
        PR_ERR("register media event_cb failed: %d", rt);
        return;
    }
    PR_NOTICE("demo media stream event_cb registered");

#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1) && \
    defined(CAMERA_DEMO_PB_SEED) && (CAMERA_DEMO_PB_SEED == 1)
    rt = local_store_seed_h264(DEMO_PB_SEED_SRC, DEMO_PB_SEED_LEAF, DEMO_PB_SEED_DURATION_SEC);
    if (rt != OPRT_OK) {
        PR_WARN("pb seed %s failed: %d (copy Annex-B H264 to that path)", DEMO_PB_SEED_SRC, rt);
    }
#endif
}
