/**
 * @file app_recorder.c
 * @brief app_recorder module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_recorder.h"
#include "app_chat_bot.h"
#include "app_vad.h"
#include "app_ai.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "tkl_thread.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define RECORDER_RB_TIME_MS (10 * 1000)

#define RECORDER_UPLOAD_TIME_MS (100)

#define APP_VAD_CHECK_TM_MS (300 + 100) // vad active duration is 300ms, add 100ms to upload

#define APP_RECORDER_WAIT_ASR_TM_MS (10 * 1000)

#define APP_VOICE_FRAME_LEN_GET(tm_ms) ((tm_ms) / RECORDER_FRAME_TM_MS * RECORDER_FRAME_SIZE)

#define APP_RECORDER_STAT_CHANGE(new_stat)                                                                             \
    do {                                                                                                               \
        PR_DEBUG("app recorder stat changed: %d->%d", ctx->state, new_stat);                                           \
        ctx->state = new_stat;                                                                                         \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_RINGBUFF_T rb_hdl;
    MUTEX_HANDLE rb_mutex;
    THREAD_HANDLE thrd_hdl;
    TUYA_AUDIO_VOICE_STATE state;
    TIMER_ID asr_timer_id;

    // stat queue
    QUEUE_HANDLE stat_queue;
    MUTEX_HANDLE stat_mutex;

    // upload
    uint8_t *upload_buffer;
} APP_RECORDER_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static OPERATE_RET app_recorder_stat_fetch(TUYA_AUDIO_VOICE_STATE *stat, uint32_t timeout);

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_RECORDER_T sg_recorder;

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __app_recorder_start_data_proc(void)
{
    OPERATE_RET rt = OPRT_OK;
    APP_RECORDER_T *ctx = &sg_recorder;

    APP_WORK_MODE_E work_mode = app_chat_bot_get_work_mode();
    if (APP_CHAT_BOT_WORK_MODE_HOLD == work_mode) {
        // one shot mode, remove the first 100ms of data (sound of button press)
        uint32_t discard_size = APP_VOICE_FRAME_LEN_GET(100);
        if (tuya_ring_buff_used_size_get(ctx->rb_hdl) > discard_size) {
            PR_DEBUG("discard %d bytes", discard_size);
            tuya_ring_buff_discard(ctx->rb_hdl, discard_size);
        } else {
            // not enough data
            return OPRT_COM_ERROR;
        }
    }

    return rt;
}

static void __app_recorder_wait_asr_tm_cb(TIMER_ID timer_id, void *arg)
{
    PR_ERR("wait asr timeout");
    app_recorder_stat_post(VOICE_STATE_IN_IDLE);
    return;
}

static OPERATE_RET __app_recorder_vad_proc(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t chat_bot_enable = app_chat_bot_is_enable();

    // vad check
    ty_vad_flag_e vad_state = app_vad_get_flag();
    // PR_DEBUG("vad state: %d", vad_state);
    uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_recorder.rb_hdl);

    if (TY_VAD_FLAG_VAD_START != vad_state) {
        // Retain only the last APP_VAD_CHECK_TM_MS of data
        // PR_DEBUG("rb_used_size: %d, need data: %d", rb_used_size, APP_VOICE_FRAME_LEN_GET(APP_VAD_CHECK_TM_MS));
        if (rb_used_size > APP_VOICE_FRAME_LEN_GET(APP_VAD_CHECK_TM_MS)) {
            uint32_t discard_size = rb_used_size - APP_VOICE_FRAME_LEN_GET(APP_VAD_CHECK_TM_MS);
            tuya_ring_buff_discard(sg_recorder.rb_hdl, discard_size);
        }
    }

    if (chat_bot_enable == 0) {
        // chat bot is disable
        return OPRT_OK;
    }

    if (TY_VAD_FLAG_VAD_START == vad_state && sg_recorder.state == VOICE_STATE_IN_IDLE) {
        // vad first
        if (rb_used_size > APP_VOICE_FRAME_LEN_GET(APP_VAD_CHECK_TM_MS)) {
            PR_DEBUG("vad start");
            app_recorder_stat_post(VOICE_STATE_IN_START);
        }
    } else if (TY_VAD_FLAG_VAD_END == vad_state && sg_recorder.state == VOICE_STATE_IN_VOICE) {
        // vad end
        PR_DEBUG("vad end");
        app_recorder_stat_post(VOICE_STATE_IN_STOP);
    }

    return rt;
}

static void __app_recorder_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_AUDIO_VOICE_STATE stat = VOICE_STATE_IN_IDLE;
    APP_RECORDER_T *ctx = &sg_recorder;
    uint32_t next_timeout = 100;

    ty_vad_flag_e last_vad_state = TY_VAD_FLAG_VAD_NONE;
    uint8_t vad_first = 0;

    ctx->state = VOICE_STATE_IN_IDLE;

    for (;;) {
        rt = app_recorder_stat_fetch(&stat, next_timeout);
        if (ctx->state != stat && OPRT_OK == rt) {
            APP_RECORDER_STAT_CHANGE(stat);
        }
        next_timeout = (ctx->state == VOICE_STATE_IN_VOICE) ? 30 : 100;

        APP_WORK_MODE_E work_mode = app_chat_bot_get_work_mode();
        if (APP_CHAT_BOT_WORK_MODE_HOLD != work_mode) {
            // vad process
            __app_recorder_vad_proc();
        }

        switch (ctx->state) {
        case VOICE_STATE_IN_IDLE: {
            if (work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
                tal_mutex_lock(ctx->rb_mutex);
                tuya_ring_buff_reset(ctx->rb_hdl);
                tal_mutex_unlock(ctx->rb_mutex);
            }

            if (tal_sw_timer_is_running(ctx->asr_timer_id)) {
                PR_DEBUG("stop asr timer");
                tal_sw_timer_stop(ctx->asr_timer_id);
            }
        } break;
        case VOICE_STATE_IN_SILENCE: {
        } break;
        case VOICE_STATE_IN_START: {
            uint8_t int_enable = (work_mode == APP_CHAT_BOT_WORK_MODE_FREE) ? 1 : 0;
            TUYA_CALL_ERR_LOG(app_ai_upload_start(int_enable));
            if (OPRT_OK == rt) {
                vad_first = 1;
                app_recorder_stat_post(VOICE_STATE_IN_VOICE);
            } else {
                // upload start failed
                app_recorder_stat_post(VOICE_STATE_IN_STOP);
            }
        } break;
        case VOICE_STATE_IN_VOICE: {
            uint32_t upload_size = APP_VOICE_FRAME_LEN_GET(RECORDER_UPLOAD_TIME_MS);
            uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_recorder.rb_hdl);
            if (upload_size > rb_used_size) {
                // not enough data
                break;
            }

            PR_DEBUG("upload_size: %d, rb_used_size: %d", upload_size, rb_used_size);
            tuya_ring_buff_read(sg_recorder.rb_hdl, sg_recorder.upload_buffer, upload_size);
            if (vad_first) {
                PR_DEBUG("recorder upload first frame");
                TUYA_CALL_ERR_LOG(app_ai_upload_data(1, sg_recorder.upload_buffer, upload_size));
                vad_first = 0;
            } else {
                TUYA_CALL_ERR_LOG(app_ai_upload_data(0, sg_recorder.upload_buffer, upload_size));
            }
        } break;
        case VOICE_STATE_IN_STOP: {
            // upload all data
            uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_recorder.rb_hdl);
            while (rb_used_size) {
                uint32_t upload_size = APP_VOICE_FRAME_LEN_GET(RECORDER_UPLOAD_TIME_MS);
                if (upload_size > rb_used_size) {
                    upload_size = rb_used_size;
                }

                tuya_ring_buff_read(sg_recorder.rb_hdl, sg_recorder.upload_buffer, upload_size);
                TUYA_CALL_ERR_LOG(app_ai_upload_data(0, sg_recorder.upload_buffer, upload_size));

                rb_used_size = tuya_ring_buff_used_size_get(sg_recorder.rb_hdl);
            }

            app_ai_upload_stop();
            tal_sw_timer_start(ctx->asr_timer_id, APP_RECORDER_WAIT_ASR_TM_MS, TAL_TIMER_ONCE);
            app_recorder_stat_post(VOICE_STATE_IN_WAIT_ASR);
        } break;
        case VOICE_STATE_IN_WAIT_ASR: {
            // wait asr, nothing to do
        } break;
        case VOICE_STATE_IN_RESUME: {
        } break;
        default:
            break;
        }
    }
}

OPERATE_RET app_recorder_stat_post(TUYA_AUDIO_VOICE_STATE stat)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_recorder.stat_mutex || NULL == sg_recorder.stat_queue) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_recorder.stat_mutex);
    rt = tal_queue_post(sg_recorder.stat_queue, &stat, 0);
    tal_mutex_unlock(sg_recorder.stat_mutex);

    return rt;
}

TUYA_AUDIO_VOICE_STATE app_recorder_stat_get(void)
{
    if (NULL == sg_recorder.stat_mutex || NULL == sg_recorder.stat_queue) {
        return VOICE_STATE_IN_IDLE;
    }

    return sg_recorder.state;
}

static OPERATE_RET app_recorder_stat_fetch(TUYA_AUDIO_VOICE_STATE *stat, uint32_t timeout)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_recorder.stat_queue) {
        return OPRT_COM_ERROR;
    }

    return tal_queue_fetch(sg_recorder.stat_queue, stat, timeout);
}

OPERATE_RET app_recorder_rb_write(const void *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_recorder.rb_hdl || NULL == sg_recorder.rb_mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_recorder.rb_mutex);
    rt = tuya_ring_buff_write(sg_recorder.rb_hdl, data, len);
    tal_mutex_unlock(sg_recorder.rb_mutex);

    return OPRT_OK;
}

OPERATE_RET app_recorder_rb_reset(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_recorder.rb_hdl || NULL == sg_recorder.rb_mutex) {
        return OPRT_COM_ERROR;
    }

    uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_recorder.rb_hdl);
    if (rb_used_size == 0) {
        return OPRT_OK;
    }

    tal_mutex_lock(sg_recorder.rb_mutex);
    tuya_ring_buff_reset(sg_recorder.rb_hdl);
    tal_mutex_unlock(sg_recorder.rb_mutex);

    return rt;
}

OPERATE_RET app_recorder_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s start", __func__);

    memset(&sg_recorder, 0, sizeof(APP_RECORDER_T));

    // upload buffer init
    sg_recorder.upload_buffer = (uint8_t *)tkl_system_psram_malloc(APP_VOICE_FRAME_LEN_GET(RECORDER_UPLOAD_TIME_MS));
    TUYA_CHECK_NULL_GOTO(sg_recorder.upload_buffer, __ERR);

    // asr timer init
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__app_recorder_wait_asr_tm_cb, NULL, &sg_recorder.asr_timer_id), __ERR);

    // create ring buffer
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_recorder.rb_mutex), __ERR);
    uint32_t recorder_rb_size = APP_VOICE_FRAME_LEN_GET(RECORDER_RB_TIME_MS);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(recorder_rb_size, OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder.rb_hdl), __ERR);

    // create stat queue
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_recorder.stat_mutex), __ERR);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&sg_recorder.stat_queue, sizeof(TUYA_AUDIO_VOICE_STATE), 10), __ERR);

    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&sg_recorder.thrd_hdl, "ai_recorder", 1024 * 4 * 4, THREAD_PRIO_2,
                                                  __app_recorder_task, NULL),
                       __ERR);

    PR_DEBUG("%s success", __func__);

    return OPRT_OK;

__ERR:
    if (sg_recorder.asr_timer_id) {
        tal_sw_timer_delete(sg_recorder.asr_timer_id);
        sg_recorder.asr_timer_id = NULL;
    }

    if (sg_recorder.upload_buffer) {
        tkl_system_psram_free(sg_recorder.upload_buffer);
        sg_recorder.upload_buffer = NULL;
    }

    if (sg_recorder.rb_mutex) {
        tal_mutex_release(sg_recorder.rb_mutex);
        sg_recorder.rb_mutex = NULL;
    }

    if (sg_recorder.rb_hdl) {
        tuya_ring_buff_free(sg_recorder.rb_hdl);
        sg_recorder.rb_hdl = NULL;
    }

    PR_ERR("%s failed", __func__);

    return rt;
}
