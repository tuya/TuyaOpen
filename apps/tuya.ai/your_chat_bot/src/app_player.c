/**
 * @file app_player.c
 * @brief app_player module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_player.h"
#include "app_media_alert.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "tkl_audio.h"

#include <driver/aud_dac_types.h>
#include <driver/aud_dac.h>
#include <driver/dma.h>
#include <driver/audio_ring_buff.h>
#include <modules/mp3dec.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define MP3_STREAM_BUFF_MAX_LEN (1024 * 64 * 2)

#define PLAYING_NO_DATA_TIMEOUT_MS (5 * 1000)

#define MP3_PCM_SIZE_MAX (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN * 2)

#define APP_PLAYER_STAT_CHANGE(new_stat)                                                                               \
    do {                                                                                                               \
        PR_DEBUG("app player stat changed: %d->%d", ctx->stat, new_stat);                                              \
        ctx->stat = new_stat;                                                                                          \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    MUTEX_HANDLE player_mutex;

    TUYA_RINGBUFF_T rb_hdl;
    MUTEX_HANDLE spk_rb_mutex;
    THREAD_HANDLE thrd_hdl;
    APP_PLAYER_STATE stat;
    TIMER_ID tm_id;

    HMP3Decoder mp3_dec;
    MP3FrameInfo mp3_frame_info;
    uint8_t first_frame;

    uint8_t *mp3_raw;
    uint8_t *mp3_raw_head;
    uint32_t mp3_raw_used_len;
    uint8_t *mp3_pcm; // mp3 decode to pcm buffer

    uint8_t is_eof;

    QUEUE_HANDLE stat_queue;
    MUTEX_HANDLE stat_mutex;

    // alert
    MUTEX_HANDLE alert_mutex;

    uint8_t is_playing;
} APP_PLAYER_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static OPERATE_RET app_player_stat_post(APP_PLAYER_STATE stat);
static OPERATE_RET app_play_stat_fetch(APP_PLAYER_STATE *stat, uint32_t timeout);

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_PLAYER_T sg_player;

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __app_player_mp3_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_mutex_lock(sg_player.player_mutex);

    if (sg_player.is_playing) {
        PR_ERR("playing..., stop it first please");
        return OPRT_COM_ERROR;
    }

    if (NULL == sg_player.mp3_dec) {
        sg_player.mp3_dec = MP3InitDecoder();
        TUYA_CHECK_NULL_RETURN(sg_player.mp3_dec, OPRT_COM_ERROR);
        PR_DEBUG("MP3InitDecoder success");
    }

    sg_player.mp3_raw_used_len = 0;

    tal_mutex_unlock(sg_player.player_mutex);

    return rt;
}

static int32_t __app_mp3_find_id3(uint8_t *buf)
{
    uint8_t tag_header[10];
    int tag_size = 0;

    memcpy(tag_header, buf, sizeof(tag_header));

    if (tag_header[0] == 'I' && tag_header[1] == 'D' && tag_header[2] == '3') {
        tag_size = ((tag_header[6] & 0x7F) << 21) | ((tag_header[7] & 0x7F) << 14) | ((tag_header[8] & 0x7F) << 7) |
                   (tag_header[9] & 0x7F);
        PR_DEBUG("ID3 tag_size = %d", tag_size);
        return tag_size + sizeof(tag_header);
    } else {
        // tag_header ignored
        return 0;
    }
}

static OPERATE_RET __app_player_mp3_playing(void)
{
    OPERATE_RET rt = OPRT_OK;
    APP_PLAYER_T *ctx = &sg_player;

    if (NULL == ctx->mp3_dec) {
        PR_ERR("mp3 decoder is NULL");
        return OPRT_COM_ERROR;
    }

    uint32_t rb_used_len = tuya_ring_buff_used_size_get(ctx->rb_hdl);
    if (0 == rb_used_len && 0 == ctx->mp3_raw_used_len) {
        // PR_DEBUG("mp3 data is empty");
        rt = OPRT_RECV_DA_NOT_ENOUGH;
        goto __EXIT;
    }
    // PR_DEBUG("rb_used_len=%d", rb_used_len);

    if (NULL != ctx->mp3_raw_head && ctx->mp3_raw_used_len > 0 && ctx->mp3_raw_head != ctx->mp3_raw) {
        // PR_DEBUG("move data, offset=%d, used_len=%d", ctx->mp3_raw_head - ctx->mp3_raw, ctx->mp3_raw_used_len);
        memmove(ctx->mp3_raw, ctx->mp3_raw_head, ctx->mp3_raw_used_len);
    }
    ctx->mp3_raw_head = ctx->mp3_raw;

    // read new data
    if (rb_used_len > 0 && ctx->mp3_raw_used_len < MAINBUF_SIZE) {
        uint32_t read_len =
            (MAINBUF_SIZE - ctx->mp3_raw_used_len) > rb_used_len ? rb_used_len : (MAINBUF_SIZE - ctx->mp3_raw_used_len);
        // PR_DEBUG("read_len=%d", read_len);
        uint32_t rt_len = tuya_ring_buff_read(ctx->rb_hdl, ctx->mp3_raw + ctx->mp3_raw_used_len, read_len);

        ctx->mp3_raw_used_len += rt_len;
    }

    // find id3 tag
    if (ctx->first_frame && ctx->mp3_raw_used_len > 10) {
        int32_t id3_size = __app_mp3_find_id3(ctx->mp3_raw);
        if (id3_size > 0) {
            ctx->mp3_raw_used_len -= id3_size;
            ctx->mp3_raw_head += id3_size;
        }
        ctx->first_frame = 0;
    }

    // decode mp3 data
    int sync_offset = MP3FindSyncWord(ctx->mp3_raw_head, ctx->mp3_raw_used_len);
    if (sync_offset < 0) {
        PR_ERR("MP3FindSyncWord not find!, sync_offset=%d", sync_offset);
        ctx->mp3_raw_used_len = 0;
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    ctx->mp3_raw_head += sync_offset;
    ctx->mp3_raw_used_len -= sync_offset;
    int rt_dec = MP3Decode(ctx->mp3_dec, &ctx->mp3_raw_head, &ctx->mp3_raw_used_len, (short *)ctx->mp3_pcm, 0);
    if (rt_dec != ERR_MP3_NONE) {
        PR_ERR("MP3Decode failed, code is %d", rt_dec);
        ctx->mp3_raw_used_len -= 80;
        ctx->mp3_raw_head += 80;
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // 3. play pcm data
    // PR_DEBUG("mp3_raw_used_len=%d", ctx->mp3_raw_used_len);
    memset(&ctx->mp3_frame_info, 0, sizeof(MP3FrameInfo));
    MP3GetLastFrameInfo(ctx->mp3_dec, &ctx->mp3_frame_info);

    TKL_AUDIO_FRAME_INFO_T frame;
    frame.pbuf = ctx->mp3_pcm;
    frame.used_size = ctx->mp3_frame_info.outputSamps * 2;
    tkl_ao_put_frame(0, 0, NULL, &frame);

__EXIT:
    return rt;
}

static void __chat_bot_player(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    APP_PLAYER_T *ctx = &sg_player;
    APP_PLAYER_STATE stat = APP_PLAYER_STAT_IDLE;
    uint32_t next_timeout = 5;

    ctx->stat = APP_PLAYER_STAT_IDLE;

    for (;;) {
        rt = app_play_stat_fetch(&stat, next_timeout);
        if (stat != ctx->stat && rt == OPRT_OK) {
            APP_PLAYER_STAT_CHANGE(stat);
        }
        next_timeout = (ctx->stat == APP_PLAYER_STAT_PLAY) ? 5 : 500;

        switch (ctx->stat) {
        case APP_PLAYER_STAT_IDLE: {
        } break;
        case APP_PLAYER_STAT_START: {
            rt = __app_player_mp3_start();
            if (rt != OPRT_OK) {
                app_player_stat_post(APP_PLAYER_STAT_IDLE);
            } else {
                PR_DEBUG("app player start");
                app_player_stat_post(APP_PLAYER_STAT_PLAY);
                ctx->is_playing = 1;
                ctx->first_frame = 1;
            }
        } break;
        case APP_PLAYER_STAT_PLAY: {
            rt = __app_player_mp3_playing();
            if (OPRT_RECV_DA_NOT_ENOUGH == rt) {
                tal_sw_timer_start(ctx->tm_id, PLAYING_NO_DATA_TIMEOUT_MS, TAL_TIMER_ONCE);
            } else if (OPRT_OK == rt) {
                if (tal_sw_timer_is_running(ctx->tm_id)) {
                    tal_sw_timer_stop(ctx->tm_id);
                }
            }
            uint32_t rb_used_len = tuya_ring_buff_used_size_get(ctx->rb_hdl);
            if (rb_used_len == 0 && 0 == ctx->mp3_raw_used_len && ctx->is_eof) {
                PR_DEBUG("app player end");
                app_player_stat_post(APP_PLAYER_STAT_STOP);
            }
        } break;
        case APP_PLAYER_STAT_STOP: {
            if (tal_sw_timer_is_running(ctx->tm_id)) {
                tal_sw_timer_stop(ctx->tm_id);
            }
            app_player_stat_post(APP_PLAYER_STAT_IDLE);
            ctx->is_playing = 0;
            ctx->is_eof = 0;
        } break;
        default:
            break;
        }
    }
}

static void *mp3_private_alloc_psram(size_t size)
{
    return tkl_system_psram_malloc(size);
}

static void mp3_private_free_psram(void *buff)
{
    tkl_system_psram_free(buff);
}

static void *mp3_private_memset_psram(void *s, unsigned char c, size_t n)
{
    return tkl_system_memset(s, c, n);
}

static OPERATE_RET __app_player_mp3_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("app player mp3 init...");

    MP3SetBuffMethodAlwaysFourAlignedAccess(mp3_private_alloc_psram, mp3_private_free_psram, mp3_private_memset_psram);

    sg_player.mp3_raw = (uint8_t *)tkl_system_psram_malloc(MAINBUF_SIZE);
    TUYA_CHECK_NULL_GOTO(sg_player.mp3_raw, __ERR);

    sg_player.mp3_pcm = (uint8_t *)tkl_system_psram_malloc(MP3_PCM_SIZE_MAX);
    TUYA_CHECK_NULL_GOTO(sg_player.mp3_pcm, __ERR);

    return rt;

__ERR:
    if (sg_player.mp3_raw) {
        tkl_system_psram_free(sg_player.mp3_raw);
        sg_player.mp3_raw = NULL;
    }

    if (sg_player.mp3_pcm) {
        tkl_system_psram_free(sg_player.mp3_pcm);
        sg_player.mp3_pcm = NULL;
    }

    return OPRT_COM_ERROR;
}

OPERATE_RET app_player_play_alert(APP_ALERT_TYPE type)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_player.alert_mutex) {
        PR_ERR("player alert mutex is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_player.alert_mutex);

    app_player_start();

    switch (type) {
    case APP_ALERT_TYPE_POWER_ON: {
        rt = app_player_data_write((char *)media_src_power_on, sizeof(media_src_power_on), 1);
    } break;
    case APP_ALERT_TYPE_NOT_ACTIVE: {
        rt = app_player_data_write((char *)media_src_not_active, sizeof(media_src_not_active), 1);
    } break;
    case APP_ALERT_TYPE_NETWORK_CFG: {
        rt = app_player_data_write((char *)media_src_netcfg_mode, sizeof(media_src_netcfg_mode), 1);
    } break;
    case APP_ALERT_TYPE_NETWORK_CONNECTED: {
        rt = app_player_data_write((char *)media_src_network_conencted, sizeof(media_src_network_conencted), 1);
    } break;
    case APP_ALERT_TYPE_NETWORK_FAIL: {
        rt = app_player_data_write((char *)media_src_network_fail, sizeof(media_src_network_fail), 1);
    } break;
    case APP_ALERT_TYPE_NETWORK_DISCONNECT: {
        rt = app_player_data_write((char *)media_src_network_disconnect, sizeof(media_src_network_disconnect), 1);
    } break;
    case APP_ALERT_TYPE_BATTERY_LOW: {
        rt = app_player_data_write((char *)media_src_battery_low, sizeof(media_src_battery_low), 1);
    } break;
    case APP_ALERT_TYPE_PLEASE_AGAIN: {
        rt = app_player_data_write((char *)media_src_please_again, sizeof(media_src_please_again), 1);
    } break;
    default:
        break;
    }

    tal_mutex_unlock(sg_player.alert_mutex);

    return rt;
}

static OPERATE_RET app_player_stat_post(APP_PLAYER_STATE stat)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_player.stat_mutex || NULL == sg_player.stat_queue) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_player.stat_mutex);
    rt = tal_queue_post(sg_player.stat_queue, &stat, 0);
    tal_mutex_unlock(sg_player.stat_mutex);

    return rt;
}

static OPERATE_RET app_play_stat_fetch(APP_PLAYER_STATE *stat, uint32_t timeout)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_player.stat_queue) {
        return OPRT_COM_ERROR;
    }

    return tal_queue_fetch(sg_player.stat_queue, stat, timeout);
}

uint8_t app_player_is_playing(void)
{
    return sg_player.is_playing;
}

OPERATE_RET app_player_start(void)
{
    return app_player_stat_post(APP_PLAYER_STAT_START);
}

OPERATE_RET app_player_data_write(uint8_t *data, uint32_t len, uint8_t is_eof)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_player.rb_hdl) {
        PR_ERR("ring buffer is NULL");
        return OPRT_COM_ERROR;
    }

    if (NULL != data && len > 0) {
        // PR_DEBUG("player write data, len=%d", len);

        uint32_t rb_free_len = tuya_ring_buff_free_size_get(sg_player.rb_hdl);
        while (rb_free_len < len) {
            // PR_DEBUG("---> ring buffer is full, wait...");
            tal_system_sleep(10);
            rb_free_len = tuya_ring_buff_free_size_get(sg_player.rb_hdl);
        }

        tal_mutex_lock(sg_player.player_mutex);
        tuya_ring_buff_write(sg_player.rb_hdl, data, len);
        tal_mutex_unlock(sg_player.player_mutex);
    }

    tal_mutex_lock(sg_player.player_mutex);
    sg_player.is_eof = is_eof;
    tal_mutex_unlock(sg_player.player_mutex);

    return rt;
}

OPERATE_RET app_player_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!sg_player.is_playing) {
        PR_DEBUG("not playing...");
        return OPRT_OK;
    }

    app_player_stat_post(APP_PLAYER_STAT_STOP);

    // wait for stop
    while (sg_player.is_playing) {
        tal_system_sleep(5);
    }

    tkl_ao_clear_buffer(TKL_AUDIO_TYPE_BOARD, 0);

    return rt;
}

static void __app_playing_tm_cb(TIMER_ID timer_id, void *arg)
{
    PR_DEBUG("app player timeout cb, stop playing");
    app_player_stat_post(APP_PLAYER_STAT_STOP);
    return;
}

OPERATE_RET app_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_player, 0, sizeof(APP_PLAYER_T));

    PR_DEBUG("app player init...");

    // create mutex
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.player_mutex), __ERR);

    tal_mutex_lock(sg_player.player_mutex);

    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__app_playing_tm_cb, NULL, &sg_player.tm_id), __ERR);

    // create stat queue
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.stat_mutex), __ERR);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&sg_player.stat_queue, sizeof(APP_PLAYER_STATE), 10), __ERR);

    // alert
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.alert_mutex), __ERR);

    TUYA_CALL_ERR_GOTO(__app_player_mp3_init(), __ERR);
    // ring buffer init
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(MP3_STREAM_BUFF_MAX_LEN, OVERFLOW_PSRAM_STOP_TYPE, &sg_player.rb_hdl),
                       __ERR);
    // mutex init
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.spk_rb_mutex), __ERR);

    // thread init
    TUYA_CALL_ERR_GOTO(
        tkl_thread_create_in_psram(&sg_player.thrd_hdl, "ai_player", 1024 * 8, THREAD_PRIO_2, __chat_bot_player, NULL),
        __ERR);

    tal_mutex_unlock(sg_player.player_mutex);

    PR_DEBUG("app player init success");

    return rt;

__ERR:
    if (sg_player.player_mutex) {
        tal_mutex_unlock(sg_player.player_mutex);
        tal_mutex_release(sg_player.player_mutex);
        sg_player.player_mutex = NULL;
    }

    if (sg_player.stat_mutex) {
        tal_mutex_release(sg_player.stat_mutex);
        sg_player.stat_mutex = NULL;
    }

    if (sg_player.stat_queue) {
        tal_queue_free(sg_player.stat_queue);
        sg_player.stat_queue = NULL;
    }

    if (sg_player.alert_mutex) {
        tal_mutex_release(sg_player.alert_mutex);
        sg_player.alert_mutex = NULL;
    }

    if (sg_player.spk_rb_mutex) {
        tal_mutex_release(sg_player.spk_rb_mutex);
        sg_player.spk_rb_mutex = NULL;
    }

    if (sg_player.rb_hdl) {
        tuya_ring_buff_free(sg_player.rb_hdl);
        sg_player.rb_hdl = NULL;
    }

    return rt;
}
