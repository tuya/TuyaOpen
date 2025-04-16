/**
 * @file ai_audio_main.c
 * @version 0.1
 * @date 2025-04-15
 */

#include "tuya_cloud_types.h"
#include "tkl_audio.h"
#include "tkl_queue.h"
#include "tkl_memory.h"
#include "tkl_thread.h"

#include "tal_api.h"
#include "ai_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_SPEAK_VOLUME_KEY "spk_volume"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint32_t frame_len;
    uint8_t *frame;
} AI_AUDIO_FRAME_MSG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool sg_ai_audio_is_silent = true;
static bool sg_is_support_interrupt = 0;
static THREAD_HANDLE sg_ai_audio_thrd_hdl = NULL;
static QUEUE_HANDLE sg_ai_audio_frame_queue = NULL;
static AI_AGENT_MSG_CB sg_ai_agent_msg_cb = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_audio_silent_cb(void)
{
    ai_audio_set_silent(true);
}

static void __ai_audio_agent_msg_cb(AI_AGENT_MSG_T *msg)
{

    switch (msg->type) {
    case AI_AGENT_MSG_TP_TEXT_ASR: {
        if (msg->data_len > 0) {
            // Prepare to play mp3
            if (ai_audio_player_is_playing()) {
                PR_DEBUG("player is playing, stop it first");
                ai_audio_player_stop();
            }
            ai_audio_player_start();
        }

        if (AI_CLOUD_ASR_STATE_WAIT_ASR == ai_audio_cloud_asr_get_state()) {
            PR_DEBUG("get cloud asr, set idle");
            ai_audio_cloud_asr_reset();
        }

    } break;
    case AI_AGENT_MSG_TP_AUDIO_DATA: {
        ai_audio_player_data_write(msg->data, msg->data_len, 0);
    } break;
    case AI_AGENT_MSG_TP_AUDIO_STOP: {
        ai_audio_player_data_write(msg->data, msg->data_len, 1);
    } break;
    default:
        break;
    }

    if (sg_ai_agent_msg_cb) {
        sg_ai_agent_msg_cb(msg);
    }
}

static int __ai_audio_get_frame_cb(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    AI_AUDIO_FRAME_MSG_T msg;
    OPERATE_RET rt = OPRT_OK;

    if (true == sg_ai_audio_is_silent) {
        return 0;
    }

    if (false == sg_is_support_interrupt) {
        if (true == ai_audio_player_is_playing()) {
            return 0;
        }
    }

#if 0
     msg.frame_len = pframe->used_size;
     msg.frame = tkl_system_psram_malloc(pframe->used_size);
     if(NULL == msg.frame) {
         PR_ERR("malloc failed");
         return 1;
     }
     memcpy(msg.frame, pframe->pbuf, pframe->used_size);
 
     rt = tal_queue_post(sg_ai_audio_frame_queue, &msg, 0);
     if(rt !=  OPRT_OK) {
         PR_ERR("tal_queue_post failed:%d", rt);
         return 1;
     }
#else
    AI_AUDIO_WAKEUP_EVENT_E wakeup_evt = AI_AUDIO_WAKEUP_EVT_NONE;

    ai_audio_wakeup_feed(pframe->pbuf, pframe->used_size);

    wakeup_evt = ai_audio_wakeup_detect_event();

    if (true == ai_audio_is_awake()) {
        ai_audio_cloud_asr_input(pframe->pbuf, pframe->used_size);
    } else {
        ai_audio_cloud_asr_vad_input(pframe->pbuf, pframe->used_size);
    }

    if (AI_AUDIO_WAKEUP_EVT_ENTER_IDLE == wakeup_evt) {
        ai_audio_cloud_asr_stop();
    } else if (AI_AUDIO_WAKEUP_EVT_WAKEUP == wakeup_evt) {
        ai_audio_cloud_asr_start();
    }

#endif

    return rt;
}

static void __ai_audio_handle_frame_task(void *arg)
{
    AI_AUDIO_FRAME_MSG_T msg;
    AI_AUDIO_WAKEUP_EVENT_E wakeup_evt = AI_AUDIO_WAKEUP_EVT_NONE;

    for (;;) {
        tal_queue_fetch(sg_ai_audio_frame_queue, &msg, TKL_QUEUE_WAIT_FROEVER);

        ai_audio_wakeup_feed(msg.frame, msg.frame_len);

        wakeup_evt = ai_audio_wakeup_detect_event();

        if (true == ai_audio_is_awake()) {
            ai_audio_cloud_asr_input(msg.frame, msg.frame_len);
        } else {
            ai_audio_cloud_asr_vad_input(msg.frame, msg.frame_len);
        }

        if (AI_AUDIO_WAKEUP_EVT_ENTER_IDLE == wakeup_evt) {
            ai_audio_cloud_asr_stop();
        } else if (AI_AUDIO_WAKEUP_EVT_WAKEUP == wakeup_evt) {
            ai_audio_cloud_asr_start();
        }

        if (msg.frame) {
            tkl_system_psram_free(msg.frame);
        }
    }
}

static OPERATE_RET __ai_audio_hardware_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("tkl_audio_init...");

    TKL_AUDIO_CONFIG_T config;
    memset(&config, 0, sizeof(TKL_AUDIO_CONFIG_T));

    config.ai_chn = TKL_AI_0;
    config.sample = TKL_AUDIO_SAMPLE_16K;
    config.datebits = TKL_AUDIO_DATABITS_16;
    config.channel = TKL_AUDIO_CHANNEL_MONO;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = __ai_audio_get_frame_cb;

    config.spk_sample = TKL_AUDIO_SAMPLE_16K;
    config.spk_gpio = SPEAKER_EN_PIN;
    config.spk_gpio_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tkl_ai_init(&config, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_start(0, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80));

    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, ai_audio_get_volume()));

    PR_DEBUG("tkl_audio_init success");

    return OPRT_OK;
}

OPERATE_RET ai_audio_init(AI_AUDIO_CONFIG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (cfg->is_open_vad) {
        TUYA_CALL_ERR_RETURN(ai_audio_wakeup_open_vad());
        if (cfg->is_open_asr) {
            TUYA_CALL_ERR_RETURN(ai_audio_wakeup_open_asr());
        }
    }

    sg_is_support_interrupt = cfg->is_enable_interrupt;

    TUYA_CALL_ERR_RETURN(ai_audio_wakeup_init(cfg->wakeup_timeout, __ai_audio_silent_cb));

    TUYA_CALL_ERR_RETURN(ai_audio_cloud_asr_init(cfg->is_enable_interrupt));

    TUYA_CALL_ERR_RETURN(ai_audio_player_init());

    TUYA_CALL_ERR_RETURN(ai_audio_agent_init(__ai_audio_agent_msg_cb));
    sg_ai_agent_msg_cb = cfg->agent_msg_cb;

    TUYA_CALL_ERR_RETURN(__ai_audio_hardware_init());

    //  TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_ai_audio_frame_queue, sizeof(AI_AUDIO_FRAME_MSG_T), 50));

    //  TUYA_CALL_ERR_RETURN(tkl_thread_create_in_psram(&sg_ai_audio_thrd_hdl, "ai_audio", 1024 * 4, THREAD_PRIO_2,
    //                                                   __ai_audio_handle_frame_task, NULL));

    return OPRT_OK;
}

OPERATE_RET ai_audio_set_volume(uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    // kv storage
    TUYA_CALL_ERR_LOG(tal_kv_set(AI_AUDIO_SPEAK_VOLUME_KEY, &volume, sizeof(volume)));
    TUYA_CALL_ERR_LOG(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));

    return rt;
}

uint8_t ai_audio_get_volume(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t volume = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // kv read
    TUYA_CALL_ERR_LOG(tal_kv_get(AI_AUDIO_SPEAK_VOLUME_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read volume failed");
        volume = 50;
    } else {
        volume = *value;
    }

    PR_DEBUG("get spk volume: %d", volume);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return volume;
}

OPERATE_RET ai_audio_set_silent(bool is_silent)
{
    if (true == is_silent) {
        ai_audio_set_wakeup_manual(false);
        ai_audio_player_stop();
        ai_audio_cloud_asr_stop();
    } else {
        ai_audio_set_wakeup_manual(true);
    }

    sg_ai_audio_is_silent = is_silent;

    return OPRT_OK;
}

bool ai_audio_is_silent(void)
{
    return sg_ai_audio_is_silent;
}
