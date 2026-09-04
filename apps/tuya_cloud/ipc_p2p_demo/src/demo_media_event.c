/**
 * @file demo_media_event.c
 * @brief Demo MEDIA_STREAM event_cb: local_store PB query + segment replay
 * @version 2.0
 * @date 2026-09-02
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
#include "tuya_error_code.h"
#include "tuya_ipc_media_stream_event.h"
#include "tuya_ipc_media_stream.h"
#include "tuya_ipc_media_adapter.h"
#include <string.h>
#include <stdio.h>

#if DEMO_HAS_LOCAL_STORE
#include "local_store.h"
#endif

#define DEMO_PB_DAY_SEG_MAX 1024
#define DEMO_PB_SEND_RETRY_MS 20
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
#if defined(CAMERA_DEMO_WIDTH) && defined(CAMERA_DEMO_HEIGHT)
#define DEMO_PB_WIDTH CAMERA_DEMO_WIDTH
#define DEMO_PB_HEIGHT CAMERA_DEMO_HEIGHT
#else
#define DEMO_PB_WIDTH 480
#define DEMO_PB_HEIGHT 480
#endif
#ifdef CAMERA_DEMO_FPS
#define DEMO_PB_FPS CAMERA_DEMO_FPS
#else
#define DEMO_PB_FPS 20
#endif
/* event_cb return: App re-START same seg without mid playTime */
#define DEMO_PB_START_IGNORED 1

#if DEMO_HAS_LOCAL_STORE
static THREAD_HANDLE s_pb_thread = NULL;
static BOOL_T s_pb_running = FALSE;
static BOOL_T s_pb_alive = FALSE;
static BOOL_T s_pb_pause = FALSE;
static uint32_t s_pb_play_ts = 0;
static uint32_t s_pb_speed = 1;
static uint32_t s_pb_seg_start = 0;
static uint32_t s_pb_seg_end = 0;
static BOOL_T s_pb_seek_req = FALSE;
static BOOL_T s_pb_switch_req = FALSE;
static uint32_t s_pb_switch_start = 0;
static uint32_t s_pb_switch_end = 0;
static char s_pb_switch_path[DEMO_PB_PATH_MAX];

static OPERATE_RET __pb_resolve_path(uint32_t play_ts, char *path, uint32_t path_len, uint32_t *seg_start,
                                     uint32_t *seg_end)
{
    LOCAL_STORE_SEG_T seg;
    OPERATE_RET rt;

    if (path == NULL || path_len == 0) {
        return OPRT_INVALID_PARM;
    }
    path[0] = '\0';
    *seg_start = 0;
    *seg_end = 0;
    memset(&seg, 0, sizeof(seg));
    if (play_ts == 0) {
        play_ts = (uint32_t)tal_time_get_posix();
    }
    rt = local_store_find_by_play_ts(play_ts, &seg, path, path_len);
    if (rt != OPRT_OK) {
        PR_WARN("pb find ts=%u failed: %d", play_ts, rt);
        return rt;
    }
    *seg_start = seg.start_ts;
    *seg_end = seg.end_ts;
    PR_NOTICE("pb open segment %s [%u,%u] play_ts=%u", path, seg.start_ts, seg.end_ts, play_ts);
    return OPRT_OK;
}

static BOOL_T __pb_open_next_segment(LOCAL_STORE_READER_T **rd, uint32_t *seg_start, uint32_t *seg_end)
{
    char path[DEMO_PB_PATH_MAX];
    LOCAL_STORE_SEG_T seg;
    LOCAL_STORE_READER_T *next;

    memset(&seg, 0, sizeof(seg));
    if (local_store_find_next_seg(*seg_start, &seg, path, sizeof(path)) != OPRT_OK) {
        return FALSE;
    }
    next = local_store_reader_open(path);
    if (next == NULL) {
        PR_ERR("pb next segment open failed: %s", path);
        return FALSE;
    }
    if (*rd != NULL) {
        local_store_reader_close(*rd);
    }
    *rd = next;
    *seg_start = seg.start_ts;
    *seg_end = seg.end_ts;
    s_pb_seg_start = seg.start_ts;
    s_pb_seg_end = seg.end_ts;
    tuya_ipc_media_p2p_drop_unsent();
    PR_NOTICE("pb continue next segment %s [%u,%u]", path, seg.start_ts, seg.end_ts);
    return TRUE;
}

typedef struct {
    uint32_t base_ms;
    uint64_t first_ts;
    uint32_t speed;
} DEMO_PB_PACE_T;

static int32_t __pb_pace(DEMO_PB_PACE_T *pc, uint64_t frame_ts)
{
    uint32_t now = tal_system_get_millisecond();
    uint32_t speed = s_pb_speed ? s_pb_speed : 1;
    uint32_t elapsed;
    int32_t remain;

    if (pc->first_ts == 0 || frame_ts < pc->first_ts || speed != pc->speed) {
        pc->first_ts = frame_ts;
        pc->base_ms = now;
        pc->speed = speed;
        return 0;
    }
    elapsed = (uint32_t)((frame_ts - pc->first_ts) / speed);
    remain = (int32_t)(pc->base_ms + elapsed - now);
    if (remain > 0) {
        tal_system_sleep((uint32_t)remain);
        return remain;
    }
    return remain;
}

static void __pb_apply_seek(LOCAL_STORE_READER_T *rd, uint32_t seg_start, uint32_t seg_end, DEMO_PB_PACE_T *pace,
                            BOOL_T *send_armed)
{
    uint32_t play_ts = s_pb_play_ts;
    uint32_t rel_ms = 0;

    s_pb_seek_req = FALSE;
    if (rd == NULL) {
        return;
    }
    if (seg_start != 0 && play_ts > seg_start) {
        if (play_ts < seg_end) {
            rel_ms = (play_ts - seg_start) * 1000u;
        } else {
            rel_ms = (seg_end > seg_start) ? (seg_end - seg_start) * 1000u : 0;
        }
    }
    (void)local_store_reader_seek(rd, rel_ms);
    memset(pace, 0, sizeof(*pace));
    *send_armed = FALSE;
    tuya_ipc_media_p2p_drop_unsent();
    PR_NOTICE("pb seek in-place play_ts=%u rel_ms=%u seg=[%u,%u]", play_ts, rel_ms, seg_start, seg_end);
}

static BOOL_T __pb_apply_switch(LOCAL_STORE_READER_T **rd, uint32_t *seg_start, uint32_t *seg_end, char *path,
                                DEMO_PB_PACE_T *pace, BOOL_T *send_armed, BOOL_T *need_vss)
{
    LOCAL_STORE_READER_T *next;
    uint32_t play_ts = s_pb_play_ts;
    uint32_t ns = s_pb_switch_start;
    uint32_t ne = s_pb_switch_end;
    uint32_t rel_ms = 0;

    s_pb_switch_req = FALSE;
    next = local_store_reader_open(s_pb_switch_path);
    if (next == NULL) {
        PR_ERR("pb switch open failed: %s", s_pb_switch_path);
        return FALSE;
    }
    if (*rd != NULL) {
        local_store_reader_close(*rd);
    }
    *rd = next;
    *seg_start = ns;
    *seg_end = ne;
    s_pb_seg_start = ns;
    s_pb_seg_end = ne;
    memcpy(path, s_pb_switch_path, DEMO_PB_PATH_MAX);
    path[DEMO_PB_PATH_MAX - 1] = '\0';
    if (ns != 0 && play_ts > ns && play_ts < ne) {
        rel_ms = (play_ts - ns) * 1000u;
        (void)local_store_reader_seek(*rd, rel_ms);
    }
    memset(pace, 0, sizeof(*pace));
    *send_armed = FALSE;
    *need_vss = TRUE;
    tuya_ipc_media_p2p_drop_unsent();
    PR_NOTICE("pb switch in-place path=%s play_ts=%u rel_ms=%u seg=[%u,%u]", path, play_ts, rel_ms, ns, ne);
    return TRUE;
}

static void __pb_send_thread(void *arg)
{
    LOCAL_STORE_READER_T *rd = NULL;
    LOCAL_STORE_FRAME_HDR_T hdr;
    const uint8_t *payload;
    DEMO_PB_PACE_T pace;
    uint32_t frame_cnt = 0;
    uint32_t fail_cnt = 0;
    uint32_t seg_start = 0;
    uint32_t seg_end = 0;
    uint32_t play_ts = s_pb_play_ts;
    char path[DEMO_PB_PATH_MAX];
    BOOL_T send_armed = FALSE;
    BOOL_T need_vss = TRUE;
    int32_t remain;
    OPERATE_RET rt;

    (void)arg;
    s_pb_alive = TRUE;
    memset(&pace, 0, sizeof(pace));

    /* Free UDP/P2P TX while replaying (mic uplink competes with PB) */
    demo_mic_uplink_pause();

    if (__pb_resolve_path(play_ts, path, sizeof(path), &seg_start, &seg_end) != OPRT_OK) {
        goto __pb_exit;
    }
    s_pb_seg_start = seg_start;
    s_pb_seg_end = seg_end;

    rd = local_store_reader_open(path);
    if (rd == NULL) {
        PR_ERR("pb open failed: %s", path);
        goto __pb_exit;
    }
    if (seg_start != 0 && play_ts > seg_start && play_ts < seg_end) {
        (void)local_store_reader_seek(rd, (play_ts - seg_start) * 1000u);
    }
    PR_NOTICE("pb stream start path=%s play_ts=%u seg=[%u,%u] speed=%u", path, play_ts, seg_start, seg_end,
              s_pb_speed);

    while (s_pb_running) {
        MEDIA_VIDEO_FRAME_T vf;

        if (s_pb_switch_req) {
            if (!__pb_apply_switch(&rd, &seg_start, &seg_end, path, &pace, &send_armed, &need_vss)) {
                PR_ERR("pb switch failed, keep current seg=[%u,%u]", seg_start, seg_end);
            }
            continue;
        }
        if (s_pb_seek_req) {
            __pb_apply_seek(rd, seg_start, seg_end, &pace, &send_armed);
            continue;
        }
        if (s_pb_pause) {
            tal_system_sleep(40);
            continue;
        }
        rt = local_store_reader_next(rd, &hdr, &payload);
        if (rt != OPRT_OK) {
            if (rt != OPRT_NOT_FOUND) {
                PR_WARN("pb segment %s unreadable at frame %u: %d", path, frame_cnt, rt);
            }
            if (__pb_open_next_segment(&rd, &seg_start, &seg_end)) {
                memset(&pace, 0, sizeof(pace));
                send_armed = FALSE;
                continue;
            }
            PR_NOTICE("pb EOF frames=%u", frame_cnt);
            (void)tuya_ipc_media_playback_send_finish(0);
            break;
        }
        if (hdr.type == LOCAL_STORE_FRAME_AUDIO) {
            continue;
        }
        if (!send_armed) {
            if (hdr.type != LOCAL_STORE_FRAME_VIDEO_I) {
                continue;
            }
            send_armed = TRUE;
            PR_NOTICE("pb armed key size=%u ts=%llu", hdr.size, (unsigned long long)hdr.timestamp);
        }

        remain = __pb_pace(&pace, hdr.timestamp);
        if (!s_pb_running) {
            break;
        }
        if (s_pb_seek_req || s_pb_switch_req) {
            continue;
        }
        if (remain < -(int32_t)DEMO_PB_PACE_RESYNC_MS) {
            if (hdr.type != LOCAL_STORE_FRAME_VIDEO_I) {
                continue;
            }
            memset(&pace, 0, sizeof(pace));
            (void)__pb_pace(&pace, hdr.timestamp);
            PR_NOTICE("pb pace skip to I ts=%llu remain=%d", (unsigned long long)hdr.timestamp, remain);
        }

        memset(&vf, 0, sizeof(vf));
        vf.video_codec = TUYA_CODEC_VIDEO_H264;
        vf.video_frame_type =
            (hdr.type == LOCAL_STORE_FRAME_VIDEO_I) ? TUYA_VIDEO_FRAME_IFRAME : TUYA_VIDEO_FRAME_PBFRAME;
        vf.width = DEMO_PB_WIDTH;
        vf.height = DEMO_PB_HEIGHT;
        vf.fps = DEMO_PB_FPS;
        vf.p_video_buf = (uint8_t *)payload;
        vf.buf_len = hdr.size;
        vf.timestamp = hdr.timestamp;
        vf.pts = hdr.timestamp * 1000ULL;

        /* -23: drop P (align LIVE), retry only I */
        rt = OPRT_COM_ERROR;
        for (;;) {
            if (!s_pb_running || s_pb_seek_req || s_pb_switch_req) {
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
                if (hdr.type != LOCAL_STORE_FRAME_VIDEO_I) {
                    break;
                }
                fail_cnt++;
                if ((fail_cnt % 50) == 1) {
                    PR_WARN("pb send wait buffer rt=%d cnt=%u fail=%u", rt, frame_cnt, fail_cnt);
                }
                tal_system_sleep(DEMO_PB_SEND_RETRY_MS);
                continue;
            }
            if (rt == OPRT_NOT_EXIST || rt == OPRT_SOCK_CONN_ERR) {
                PR_NOTICE("pb send stop, playback not active rt=%d cnt=%u", rt, frame_cnt);
                s_pb_running = FALSE;
                break;
            }
            PR_WARN("pb send fail rt=%d cnt=%u, skip frame", rt, frame_cnt);
            break;
        }
        if (!s_pb_running) {
            break;
        }
        if (s_pb_seek_req || s_pb_switch_req) {
            continue;
        }
        if (rt == OPRT_OK) {
            if (need_vss && hdr.type == LOCAL_STORE_FRAME_VIDEO_I) {
                tuya_ipc_media_p2p_video_send_start();
                need_vss = FALSE;
            }
            if (frame_cnt == 0 || (frame_cnt % 30) == 0) {
                PR_NOTICE("pb send ok cnt=%u key=%u size=%u ts=%llu", frame_cnt,
                          (hdr.type == LOCAL_STORE_FRAME_VIDEO_I) ? 1u : 0u, hdr.size,
                          (unsigned long long)hdr.timestamp);
            }
            frame_cnt++;
        }
    }

__pb_exit:
    if (rd != NULL) {
        local_store_reader_close(rd);
    }
    s_pb_running = FALSE;
    s_pb_thread = NULL;
    s_pb_alive = FALSE;
    PR_NOTICE("pb send thread exit frames=%u", frame_cnt);
}

static void __pb_stop(void)
{
    uint32_t waited = 0;

    s_pb_running = FALSE;
    s_pb_pause = FALSE;
    s_pb_seek_req = FALSE;
    s_pb_switch_req = FALSE;
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
    tuya_ipc_media_p2p_drop_unsent();
}

void demo_media_pb_stop(void)
{
    __pb_stop();
}

static void __pb_start(uint32_t play_ts)
{
    THREAD_CFG_T cfg;

    if (s_pb_running || s_pb_alive) {
        PR_WARN("pb start skipped, thread still alive ts=%u", play_ts);
        return;
    }
    /* LIVE stop already rebuilt KCP; mid-stream must not clear_send */
    tuya_ipc_media_p2p_drop_unsent();
    s_pb_play_ts = play_ts;
    s_pb_seek_req = FALSE;
    s_pb_switch_req = FALSE;
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

static int __pb_query_day(uint32_t year, uint32_t month, uint32_t day, LOCAL_STORE_SEG_T **out, uint32_t *count)
{
    LOCAL_STORE_SEG_T *segs;
    OPERATE_RET rt;

    *out = NULL;
    *count = 0;
    segs = (LOCAL_STORE_SEG_T *)Malloc(sizeof(LOCAL_STORE_SEG_T) * DEMO_PB_DAY_SEG_MAX);
    if (segs == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    *count = DEMO_PB_DAY_SEG_MAX;
    rt = local_store_query_day(year, month, day, segs, count);
    if (rt != OPRT_OK) {
        Free(segs);
        *count = 0;
        return (int)rt;
    }
    *out = segs;
    return OPRT_OK;
}

#endif /* DEMO_HAS_LOCAL_STORE */

#if !DEMO_HAS_LOCAL_STORE
void demo_media_pb_stop(void)
{
}
#endif

static int __demo_media_stream_event_cb(const int device, const int channel,
                                          const MEDIA_STREAM_EVENT_E event, void *args)
{
    (void)device;
    (void)channel;

    switch (event) {
    case MEDIA_STREAM_LIVE_VIDEO_START:
#if DEMO_HAS_LOCAL_STORE
        PR_NOTICE("pb stop: live start");
        __pb_stop();
#endif
        break;
    case MEDIA_STREAM_LIVE_VIDEO_STOP:
    case MEDIA_STREAM_SPEAKER_START:
    case MEDIA_STREAM_SPEAKER_STOP:
        break;

#if DEMO_HAS_LOCAL_STORE
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
        LOCAL_STORE_SEG_T *segs = NULL;
        uint32_t count = 0;
        uint32_t i;
        uint32_t arr_bytes;
        PLAY_BACK_ALARM_INFO_ARR *arr;
        int rt;

        if (day == NULL) {
            return OPRT_INVALID_PARM;
        }
        day->alarm_arr = NULL;
        rt = __pb_query_day(day->year, day->month, day->day, &segs, &count);
        if (rt != OPRT_OK) {
            return rt;
        }
        arr_bytes = (uint32_t)(sizeof(PLAY_BACK_ALARM_INFO_ARR) + count * sizeof(PLAY_BACK_ALARM_FRAGMENT));
        arr = (PLAY_BACK_ALARM_INFO_ARR *)Malloc(arr_bytes);
        if (arr == NULL) {
            Free(segs);
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
        Free(segs);
        day->alarm_arr = arr;
        PR_NOTICE("pb day %u-%02u-%02u segs=%u", day->year, day->month, day->day, count);
        break;
    }
    case MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_WITH_ENCRYPT: {
        C2C_TRANS_QUERY_PB_DAY_WITH_ENCRYPT_RESP *day = (C2C_TRANS_QUERY_PB_DAY_WITH_ENCRYPT_RESP *)args;
        LOCAL_STORE_SEG_T *segs = NULL;
        uint32_t count = 0;
        uint32_t i;
        uint32_t arr_bytes;
        PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR *arr;
        int rt;

        if (day == NULL) {
            return OPRT_INVALID_PARM;
        }
        day->alarm_arr = NULL;
        rt = __pb_query_day(day->year, day->month, day->day, &segs, &count);
        if (rt != OPRT_OK) {
            return rt;
        }
        arr_bytes = (uint32_t)(sizeof(PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR) +
                               count * sizeof(PLAY_BACK_FILE_INFOS_WITH_ENCRYPT));
        arr = (PLAY_BACK_ALARM_INFO_WITH_ENCRYPT_ARR *)Malloc(arr_bytes);
        if (arr == NULL) {
            Free(segs);
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
        Free(segs);
        day->alarm_arr = arr;
        PR_NOTICE("pb day(encrypt) %u-%02u-%02u segs=%u", day->year, day->month, day->day, count);
        break;
    }
    case MEDIA_STREAM_PLAYBACK_START_TS: {
        uint32_t play_ts = 0;
        uint32_t new_start = 0;
        uint32_t new_end = 0;
        char path[DEMO_PB_PATH_MAX];
        BOOL_T mid_seek = FALSE;
        BOOL_T same_seg = FALSE;

        if (args != NULL) {
            C2C_TRANS_CTRL_PB_START *pb = (C2C_TRANS_CTRL_PB_START *)args;

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
        if (same_seg && !mid_seek) {
            PR_NOTICE("pb START ignore duplicate same seg=[%u,%u] play_ts=%u (keep playing)", new_start, new_end,
                      play_ts);
            return DEMO_PB_START_IGNORED;
        }
        if (same_seg && mid_seek && s_pb_alive) {
            s_pb_play_ts = play_ts;
            s_pb_seek_req = TRUE;
            PR_NOTICE("pb START same-seg seek play_ts=%u", play_ts);
            break;
        }
        if (!same_seg && s_pb_alive && path[0] != '\0') {
            s_pb_play_ts = play_ts;
            s_pb_switch_start = new_start;
            s_pb_switch_end = new_end;
            memcpy(s_pb_switch_path, path, sizeof(s_pb_switch_path));
            s_pb_switch_path[sizeof(s_pb_switch_path) - 1] = '\0';
            s_pb_switch_req = TRUE;
            PR_NOTICE("pb START switch-seg play_ts=%u seg=[%u,%u] path=%s", play_ts, new_start, new_end,
                      s_pb_switch_path);
            break;
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
        PR_NOTICE("pb SET_SPEED=%u", s_pb_speed);
        break;
    }
#else
    case MEDIA_STREAM_PLAYBACK_START_TS:
    case MEDIA_STREAM_PLAYBACK_STOP:
        (void)tuya_ipc_media_playback_send_finish(0);
        break;
#endif
    default:
        break;
    }
    return 0;
}

void demo_media_event_register(void)
{
    OPERATE_RET rt = tuya_ipc_media_stream_register_event_cb(__demo_media_stream_event_cb);
    if (rt != OPRT_OK) {
        PR_ERR("register media event_cb failed: %d", rt);
        return;
    }
    PR_NOTICE("demo media stream event_cb registered");

#if DEMO_HAS_LOCAL_STORE && defined(CAMERA_DEMO_PB_SEED) && (CAMERA_DEMO_PB_SEED == 1)
    rt = local_store_seed_h264(DEMO_PB_SEED_SRC, DEMO_PB_SEED_LEAF, DEMO_PB_SEED_DURATION_SEC);
    if (rt != OPRT_OK) {
        PR_WARN("pb seed %s failed: %d (copy Annex-B H264 to that path)", DEMO_PB_SEED_SRC, rt);
    }
#endif
}
