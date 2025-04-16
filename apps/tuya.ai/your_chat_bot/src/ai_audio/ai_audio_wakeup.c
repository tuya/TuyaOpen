/**
 * @file app_asr_wakeup.c
 * @version 0.1
 * @date 2025-04-14
 */

#include "tkl_asr.h"
#include "tkl_audio.h"
#include "ty_vad_app.h"
#include "tkl_memory.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio_wakeup.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
typedef struct {
    bool is_vad_open;
    bool is_asr_open;
    bool is_awake;
    bool enable_manual_set;
    bool set_active;
    TUYA_RINGBUFF_T ringbuff_hdl;
    uint32_t frame_buff_len;
    TIMER_ID timer_id;
    uint32_t time_ms;
} AI_AUDIO_WAKEUP_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
const static TKL_ASR_WAKEUP_WORD_E cWAKEUP_KEYWORD_LIST[] = {
    TKL_ASR_WAKEUP_NIHAO_TUYA,
};

static AI_AUDIO_WAKEUP_T sg_audio_wakeup;
static bool sg_is_wakeup_init = false;
static AI_AUDIO_WAKEUP_ENTER_IDLE_CB sg_ai_audio_enter_idle_cb;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_audio_wakeup_timeout_cb(TIMER_ID timer_id, void *arg)
{
    if (sg_ai_audio_enter_idle_cb) {
        sg_ai_audio_enter_idle_cb();
    }

    sg_audio_wakeup.is_awake = false;
}

static TKL_ASR_WAKEUP_WORD_E __asr_recognize_wakeup_keyword(void)
{
    uint32_t rb_used_size = 0;
    TKL_ASR_WAKEUP_WORD_E wakeup_word = TKL_ASR_WAKEUP_WORD_UNKNOWN;
    uint32_t uint_size = tkl_asr_get_process_uint_size();

    if (NULL == sg_audio_wakeup.ringbuff_hdl || false == sg_audio_wakeup.is_asr_open) {
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

    rb_used_size = tuya_ring_buff_used_size_get(sg_audio_wakeup.ringbuff_hdl);

    if (rb_used_size < uint_size) {
        PR_DEBUG("wakeup data not enough");
        return false;
    }

    uint8_t p_buff = tkl_system_psram_malloc(uint_size);
    if (NULL == p_buff) {
        PR_ERR("malloc failed");
        return false;
    }

    while (rb_used_size >= uint_size) {
        tuya_ring_buff_read(sg_audio_wakeup.ringbuff_hdl, p_buff, uint_size);

        wakeup_word = tkl_asr_recognize_wakeup_word(p_buff, uint_size);
        if (wakeup_word != TKL_ASR_WAKEUP_WORD_UNKNOWN) {
            tkl_system_psram_free(p_buff);
            return wakeup_word;
        }

        rb_used_size -= uint_size;
    }

    tkl_system_psram_free(p_buff);

    return wakeup_word;
}

static void __ai_audio_set_wakeup(bool is_wakeup)
{
    if (false == sg_is_wakeup_init) {
        PR_ERR("app asr wakeup not init");
        return;
    }

    if (is_wakeup) {
        sg_audio_wakeup.is_awake = true;
        if (sg_audio_wakeup.time_ms) {
            tal_sw_timer_start(sg_audio_wakeup.timer_id, sg_audio_wakeup.time_ms, TAL_TIMER_ONCE);
        }
    } else {
        if (TRUE == tal_sw_timer_is_running(sg_audio_wakeup.timer_id)) {
            tal_sw_timer_stop(sg_audio_wakeup.timer_id);
        }
        sg_audio_wakeup.is_awake = false;
    }

    return;
}

OPERATE_RET ai_audio_wakeup_init(uint32_t keepalive_time_ms, AI_AUDIO_WAKEUP_ENTER_IDLE_CB fun_cb)
{
    OPERATE_RET rt = OPRT_OK;

    if (true == sg_is_wakeup_init) {
        PR_NOTICE("app asr wakeup already init");
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__ai_audio_wakeup_timeout_cb, NULL, &sg_audio_wakeup.timer_id));

    sg_audio_wakeup.time_ms = keepalive_time_ms;
    sg_ai_audio_enter_idle_cb = fun_cb;

    sg_is_wakeup_init = true;

    return OPRT_OK;
}

OPERATE_RET ai_audio_wakeup_open_vad(void)
{
    OPERATE_RET rt = OPRT_OK;

    ty_vad_config_t vad_config;
    memset(&vad_config, 0, sizeof(ty_vad_config_t));
    vad_config.start_threshold_ms = 300;
    vad_config.end_threshold_ms = 500;
    vad_config.silence_threshold_ms = 0;
    vad_config.sample_rate = TKL_AUDIO_SAMPLE_16K;
    vad_config.channel = TKL_AUDIO_CHANNEL_MONO;
    vad_config.vad_frame_duration = 10;
    vad_config.scale = 2.5;
    TUYA_CALL_ERR_RETURN(ty_vad_app_init(&vad_config));
    TUYA_CALL_ERR_RETURN(ty_vad_app_start());

    sg_audio_wakeup.is_vad_open = true;

    return OPRT_OK;
}

OPERATE_RET ai_audio_wakeup_open_asr(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_asr_init());
    TUYA_CALL_ERR_RETURN(tkl_asr_wakeup_word_config(cWAKEUP_KEYWORD_LIST, CNTSOF(cWAKEUP_KEYWORD_LIST)));

    // create ring buffer
    uint32_t rb_size = tkl_asr_get_process_uint_size() * 15;
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(rb_size, OVERFLOW_PSRAM_STOP_TYPE, &sg_audio_wakeup.ringbuff_hdl));

    sg_audio_wakeup.is_asr_open = true;

    return OPRT_OK;
}

OPERATE_RET ai_audio_wakeup_feed(uint8_t *data, uint32_t len)
{
    if (false == sg_is_wakeup_init) {
        PR_ERR("audio wakeup not init");
        return;
    }

    if (sg_audio_wakeup.is_vad_open) {
        ty_vad_frame_put(data, len);
    }

    if (sg_audio_wakeup.is_asr_open) {
        tuya_ring_buff_write(sg_audio_wakeup.ringbuff_hdl, data, len);
    }

    return OPRT_OK;
}

AI_AUDIO_WAKEUP_EVENT_E ai_audio_wakeup_detect_event(void)
{
    AI_AUDIO_WAKEUP_EVENT_E event = AI_AUDIO_WAKEUP_EVT_NONE;
    bool is_detect_active = sg_audio_wakeup.is_awake;
    TKL_ASR_WAKEUP_WORD_E key = TKL_ASR_WAKEUP_WORD_UNKNOWN;

    if (sg_audio_wakeup.is_vad_open) {
        if (TY_VAD_FLAG_VAD_START == ty_get_vad_flag()) {
            if (sg_audio_wakeup.is_asr_open) {
                key = __asr_recognize_wakeup_keyword();
                if (TKL_ASR_WAKEUP_WORD_UNKNOWN != key) {
                    event = AI_AUDIO_WAKEUP_EVT_WAKEUP;
                    is_detect_active = true;
                }
            } else {
                if (false == sg_audio_wakeup.is_awake) {
                    event = AI_AUDIO_WAKEUP_EVT_WAKEUP;
                }
                is_detect_active = true;
            }
        } else {
            if (true == sg_audio_wakeup.is_awake) {
                event = AI_AUDIO_WAKEUP_EVT_ENTER_IDLE;
            }
            is_detect_active = false;
        }
    }

    if (event != AI_AUDIO_WAKEUP_EVT_NONE) {
        PR_DEBUG("wakeup state:%d event:%d", is_detect_active, event);
        __ai_audio_set_wakeup(is_detect_active);
    }

    return event;
}

void ai_audio_set_wakeup_manual(bool is_wakeup)
{
    if (false == sg_is_wakeup_init) {
        PR_ERR("app asr wakeup not init");
        return;
    }

    sg_audio_wakeup.enable_manual_set = true;
    __ai_audio_set_wakeup(is_wakeup);

    return;
}

bool ai_audio_is_awake(void)
{
    return sg_audio_wakeup.is_awake;
}
