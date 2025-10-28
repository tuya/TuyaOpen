#include "tuya_ai_toy.h"
#include "tuya_device_cfg.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tkl_audio.h"
#include "tuya_ringbuf.h"
#include "tuya_devos_utils.h"
#include "tal_semaphore.h"
#include "tal_queue.h"
#include "tal_system.h"
#include "tal_network.h"
#include "tal_mutex.h"
#include "tkl_thread.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"
#include "base_event.h"
#include "tuya_iot_com_api.h"
#include "tuya_iot_wifi_api.h"
#include "tuya_speaker_service.h"
#include "media_src.h"
#include "tuya_led.h"
#include "tuya_key.h"
#include "tuya_ai_client.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_display.h"
#include "audio_recorder.h"
#include "tuya_ai_proc.h"
#include "tkl_video_enc.h"
#include "tkl_wakeup.h"
#include "tkl_sleep.h"
#include "tkl_ipc.h"
#include "audio_dump.h"
#include "tal_workq_service.h"
#include "tuya_ai_chat_protocol.h"
#include "tal_time_service.h"
#include "tuya_ai_battery.h"
#include "tal_sleep.h"
#if defined(ENABLE_APP_AI_MONITOR) && (ENABLE_APP_AI_MONITOR == 1)
#include "tuya_ai_monitor.h"
#endif

#if defined(ENABLE_AUDIO_ANALYSIS) && (ENABLE_AUDIO_ANALYSIS == 1)
#include "audio_analysis.h"
#include "aud_intf_types.h"
#endif
#include "tkl_gpio.h"
#include "app_gesture.h"

#if defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
#include "tuya_robot_actions.h"
#endif


#define AI_TOY_PARA                     "ai_toy_para"
#define LONG_KEY_TIME                   400
#define TOY_IDLE_TIMEOUT               (30 * 1000)      // 30sec
#define TOY_DEEPSLEEP_TIMEOUT          (60 * 1000)      // 10min
// #define TOY_DEEPSLEEP_TIMEOUT          (10 * 60 * 1000)      // 10min

//!  video
#define MAX_INPUT_RINGBUG_SIZE          (128*1024)
#define MAX_INPUT_BUF_SIZE              (16*1024)

#define AI_TOY_ALERT_PLAY_ID            "ai_toy_alert"

typedef struct {
    OPERATE_RET (*network_status_get)(TY_AI_NET_STATUS_E *status);
    OPERATE_RET (*upload_start)(VOID);
    OPERATE_RET (*upload_data)(CHAR_T *buf, UINT_T len);
    OPERATE_RET (*upload_stop)(VOID);
} TY_AI_OPS_T;

typedef enum {
    AI_TOY_PLAYER_IDLE,
    AI_TOY_PLAYER_REDAY,
    AI_TOY_PLAYER_START,
    AI_TOY_PLAYER_STOP,
} ai_toy_player_state_t;

typedef enum {
    AI_TOY_IDLE,
    AI_TOY_LISTEN,
    AI_TOY_UPLOAD,
    AI_TOY_THINK,
    AI_TOY_SPEAK,
} ai_toy_state_t;

typedef struct {
    TY_AI_OPS_T                  ops;
    TY_AI_TOY_CFG_T              cfg;
    INT_T                        player_stat;
    BOOL_T                       lp_stat;
    BOOL_T                       vad_active;
    ai_toy_state_t               state;
    UINT8_T                      volume;             // 音量, 0~100
    UINT8_T                      player_resume_flag: 1;  // 播放器需要恢复
    UINT8_T                      player_reply_flag: 1;   // 播放器需要重播
    UINT8_T                      player_next_flag: 1;    // 播放器需要重新请求播放
    GW_WIFI_NW_STAT_E            nw_stat;            // 网络状态
    ty_ai_proc_t                 *llm;
    TIMER_ID                     idle_timer;
    TIMER_ID                     lowpower_timer;
    DELAYED_WORK_HANDLE          wq_handle;
} TY_AI_TOY_T;


STATIC TY_AI_TOY_T         *s_ai_toy = NULL;
STATIC LED_HANDLE           s_ai_toy_led = NULL;
STATIC UINT8_T              s_lang =  TY_AI_DEFAULT_LANG;


OPERATE_RET ty_ai_toy_alert(TY_AI_TOY_ALERT_TYPE type, BOOL_T send_eof);
STATIC OPERATE_RET _report_sysinfo(VOID);

void ai_toy_led_on(void)
{
    if (s_ai_toy_led) {
        tuya_set_led_light_type(s_ai_toy_led, OL_HIGH, 200, 0xFFFF);
    }
}

void ai_toy_led_off(void)
{
    if (s_ai_toy_led) {
        tuya_set_led_light_type(s_ai_toy_led, OL_LOW, 200, 0xFFFF);
    }
}

void ai_toy_led_flash(int time)
{
    if (s_ai_toy_led) {
        tuya_set_led_light_type(s_ai_toy_led, OL_FLASH_LOW, time, 0xFFFF);
    }
}


int ai_toy_state_update(TY_AI_TOY_T *toy, uint8_t state)
{
    char *toy_state_str[] = {
        "AI_TOY_IDLE",
        "AI_TOY_LISTEN",
        "AI_TOY_UPLOAD",
        "AI_TOY_THINK",
        "AI_TOY_SPEAK"
    };

    TAL_PR_DEBUG("ai_toy stat change: %s -> %s", toy_state_str[toy->state], toy_state_str[state]);
    toy->state = state;
    //! lcd update
#ifdef ENABLE_TUYA_UI    
    tuya_ai_display_msg(&state, 1, TY_DISPLAY_TP_CHAT_STAT);
#endif
    // start idle timer, when listen status keep in listen but no vad more than TOY_IDLE_TIMEOUT, changeback to AI_TOY_IDLE
    if (AI_TOY_LISTEN == toy->state) {
        tal_sw_timer_start(toy->idle_timer, TOY_IDLE_TIMEOUT, TAL_TIMER_ONCE);
    }

    // start deepsleep timer, when status keep in AI_TOY_IDLE more than 
    if (AI_TOY_IDLE == toy->state) {
        TAL_PR_DEBUG("lowpower_timer start");
        tal_sw_timer_start(toy->lowpower_timer, TOY_DEEPSLEEP_TIMEOUT, TAL_TIMER_ONCE);

        if (tuya_audio_player_get_status(TUYA_AUDIO_PLAYER_TYPE_MUSIC) == TUYA_PLAYER_STATE_PAUSED &&
            toy->player_resume_flag) {
            TAL_PR_DEBUG("music resume");
            if (tuya_speaker_service_resume(0) == OPRT_OK) {
                return OPRT_OK;
            }
        }
    } else {
        // if status exit AI_TOY_IDLE, stop the deepseelp timer
        TAL_PR_DEBUG("lowpower_timer stop");
        tal_sw_timer_stop(toy->lowpower_timer);
    }

    toy->vad_active = false;

    return OPRT_OK;
}

void ai_toy_player_stop(TY_AI_TOY_T *toy)
{
    if (tuya_speaker_service_tone_is_playing()) {
        tuya_speaker_service_tone_stop();
    }
    if (tuya_audio_player_get_status(TUYA_AUDIO_PLAYER_TYPE_MUSIC) == TUYA_PLAYER_STATE_PLAYING) {
        TAL_PR_DEBUG("music is playing, pause");
        tuya_speaker_service_pause(0);
    }
    toy->player_stat = AI_TOY_PLAYER_STOP;
}


OPERATE_RET _event_play_status_cb(TUYA_PLAYER_EVENT_INFO_S *event, VOID *user_data)
{
    if (!event)
        return OPRT_OK;
    
    TY_AI_TOY_T *toy = (TY_AI_TOY_T *)user_data;
    TAL_PR_DEBUG("player event: %d, src: %d, url: %s", event->event, event->src, event->url);

    if (event->event == TUYA_PLAYER_EVENT_FINISHED ||
        event->event == TUYA_PLAYER_EVENT_STOPPED) {
        if (event->url && strstr(event->url, AI_TOY_ALERT_PLAY_ID) != NULL) {
            TAL_PR_DEBUG("alert player stop event");
            return 0;
        }
        // 监测player stop状态，重新触发下一轮对话录音
        TAL_PR_DEBUG("toy->state %d", toy->state);
        if (AI_TOY_SPEAK == toy->state) {
            audio_recorder_mode_t mode = audio_recorder_mode_get();
            toy->player_stat = AI_TOY_PLAYER_STOP;
            ai_toy_state_update(toy, (AUDIO_RECODER_MODE_KEY_HOLD == mode || AUDIO_RECODER_MODE_WAKEUP == mode) ? AI_TOY_IDLE : AI_TOY_LISTEN);
        }
        // 接收到music finish事件，则请求下一首
        if (event->src == TUYA_AUDIO_PLAYER_TYPE_MUSIC) {
            if (event->event == TUYA_PLAYER_EVENT_FINISHED) {
                // 触发重新请求播放
                TAL_PR_DEBUG("player next");
                OPERATE_RET ret = tuya_speaker_service_next();
                TAL_PR_DEBUG("player next ret: %d", ret);
                return ret;
            }
        } else {
            // 忽略提示音
            if (!event->url || !strstr(event->url, AI_TOY_ALERT_PLAY_ID)) {
                if (toy->player_reply_flag) {
                    // 触发重复播放
                    TAL_PR_DEBUG("player reply");
                    toy->player_reply_flag = 0;
                    OPERATE_RET ret = tuya_speaker_service_replay();
                    TAL_PR_DEBUG("player reply ret: %d", ret);
                    return ret;
                } else if (tuya_audio_player_get_status(TUYA_AUDIO_PLAYER_TYPE_MUSIC) == TUYA_PLAYER_STATE_PAUSED &&
                           toy->player_resume_flag) {
                    TAL_PR_DEBUG("music resume");
                    if (tuya_speaker_service_resume(0) == OPRT_OK) {
                        return OPRT_OK;
                    }
                }
            }
        }
        TAL_PR_DEBUG("player stop event");
    }  else if (event->event == TUYA_PLAYER_EVENT_PAUSED) {
        TAL_PR_DEBUG("player pause event");
    } else if (event->event == TUYA_PLAYER_EVENT_STARTED) {
        TAL_PR_DEBUG("player start event %d", toy->player_stat);
        // tone is start and music is playing, pause the music player
        if (event->src != TUYA_AUDIO_PLAYER_TYPE_MUSIC) {
            if (tuya_audio_player_get_status(TUYA_AUDIO_PLAYER_TYPE_MUSIC) == TUYA_PLAYER_STATE_PLAYING) {
                TAL_PR_DEBUG("music is playing, pause");
                tuya_speaker_service_pause(0);
            }
        }

        if (AI_TOY_THINK == toy->state || AI_TOY_UPLOAD == toy->state) {
            toy->player_stat = AI_TOY_PLAYER_START;
            ai_toy_state_update(toy, AI_TOY_SPEAK);
        }
    } else {
        return 0;
    }
}

STATIC OPERATE_RET _network_status_get(TY_AI_NET_STATUS_E *status)
{
    if (status) {
        TY_AI_TOY_T *ctx = s_ai_toy;

        if (ctx->nw_stat == STAT_UNPROVISION_AP_STA_UNCFG) {
            *status = TY_AI_NET_STATUS_UNCFG;
        } else if (ctx->nw_stat != STAT_CLOUD_CONN || !ty_ai_chat_is_online()) {
            *status = TY_AI_NET_STATUS_DISCONNECTED;
        } else {
            *status = TY_AI_NET_STATUS_CONNECTED;
        }
    }
    return OPRT_OK;
}


STATIC VOID ai_toy_key_process(UINT_T port, PUSH_KEY_TYPE_E type, INT_T cnt) 
{
    static char *keystr[] = {
        "NORMAL_KEY",
        "SEQ_KEY",
        "LONG_KEY",
        "RELEASE_KEY",
    };

    TAL_PR_DEBUG("key process type: %s", keystr[type]);

    OPERATE_RET rt = OPRT_OK;
    if (s_ai_toy->lp_stat == TRUE) {
        // exit keep-alive status
        rt = tal_cpu_lp_disable();
        rt |= tal_wifi_lp_disable();
        
        // close LCD
        tkl_disp_set_brightness(NULL, 100);

        // close PA
        tkl_gpio_write(s_ai_toy->cfg.spk_en_pin, TUYA_GPIO_LEVEL_HIGH);

        // close battery report
        #if defined(TUYA_AI_TOY_BATTERY_ENABLE) && (TUYA_AI_TOY_BATTERY_ENABLE == 1)
        tuya_ai_toy_battery_init();
        #endif        

        s_ai_toy->lp_stat = FALSE;
        TAL_PR_DEBUG("tal_cpu_lp_disable rt=%d", rt);        
    }

#if (defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1) || (defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1))
    if (port == TUYA_GPIO_NUM_12 || port == TUYA_GPIO_NUM_8) {

        #define TOY_VOLUME_SETUP    20

        switch (type) {

        case NORMAL_KEY: {  //! 音量 +
            if (s_ai_toy && s_ai_toy->volume < 100) {
                if (s_ai_toy->volume % TOY_VOLUME_SETUP) {
                    s_ai_toy->volume = (s_ai_toy->volume / TOY_VOLUME_SETUP + 1) * TOY_VOLUME_SETUP;
                } else {
                    s_ai_toy->volume += TOY_VOLUME_SETUP;
                }
                TAL_PR_DEBUG("volume %d", s_ai_toy->volume);
                tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AO_0, NULL, s_ai_toy->volume);
                #ifdef ENABLE_TUYA_UI   
                tuya_ai_display_msg(&s_ai_toy->volume, 1, TY_DISPLAY_TP_VOLUME);
                #endif
                _report_sysinfo();
                // CHAR_T buf[32] = {0};
                // snprintf(buf, sizeof(buf), "{\"volume\": %d}", s_ai_toy->volume);
                // wd_common_write(AI_TOY_PARA, (CONST BYTE_T *)buf, strlen(buf));
            }
        } break;

        case LONG_KEY: {
            TAL_PR_DEBUG("Reset ctrl data!");
            tuya_iot_wf_gw_fast_unactive(GWCM_OLD, WF_START_SMART_AP_CONCURRENT);
        } break;

        case SEQ_KEY: {     //! 音量 -
            if (s_ai_toy && s_ai_toy->volume > 0) {
                if (s_ai_toy->volume % TOY_VOLUME_SETUP) {
                    s_ai_toy->volume = (s_ai_toy->volume / TOY_VOLUME_SETUP) * TOY_VOLUME_SETUP;
                } else {
                    s_ai_toy->volume -= TOY_VOLUME_SETUP;
                }
                TAL_PR_DEBUG("volume %d", s_ai_toy->volume);
                tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AO_0, NULL, s_ai_toy->volume);
                #ifdef ENABLE_TUYA_UI   
                tuya_ai_display_msg(&s_ai_toy->volume, 1, TY_DISPLAY_TP_VOLUME);
                #endif
                _report_sysinfo();
                // CHAR_T buf[32] = {0};
                // snprintf(buf, sizeof(buf), "{\"volume\": %d}", s_ai_toy->volume);
                // wd_common_write(AI_TOY_PARA, (CONST BYTE_T *)buf, strlen(buf));
            }
        }

        }

        return;
    }
#endif

    GW_WIFI_NW_STAT_E cur_nw_stat = 0;
    get_wf_gw_nw_status(&cur_nw_stat);
    if (cur_nw_stat == STAT_UNPROVISION_AP_STA_UNCFG) {
        ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_CFG, TRUE);
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(NULL, 0, TY_DISPLAY_TP_STAT_NETCFG);
        #endif
        return;
    }

    switch (type) {
        
    case NORMAL_KEY: {
        //! 按键唤醒
        audio_recorder_start();
    } break;

    case SEQ_KEY: {
        audio_recorder_mode_t mode = audio_recorder_mode_get();
        mode = (mode + 1) % AUDIO_RECODER_MODE_MAX;
        audio_recorder_mode_set(mode);
        TAL_PR_DEBUG("audio_recorder mode %d", mode);
    } break;

    case LONG_KEY: {
        //! 注释后，支持长按说话录音打断当前会话
        // if (AI_TOY_IDLE == s_ai_toy->state) {
        audio_recorder_key_vad_set(1);
        ai_toy_state_update(s_ai_toy, AI_TOY_LISTEN);
        // }
    } break;
     
    case RELEASE_KEY: {
        // if (AI_TOY_LISTEN == s_ai_toy->state) {
           audio_recorder_key_vad_set(0);
        // }
    } break;

    }
}

bool ai_toy_state_interrupt_check(TY_AI_TOY_T *ai_toy, audio_recorder_mode_t mode)
{
    if (AI_TOY_LISTEN != ai_toy->state  &&  //！ 只允许监听状态
       (AI_TOY_SPEAK  != ai_toy->state || AUDIO_RECODER_MODE_FREE != mode)) { //! 自由说模式, 允许说话打断
        return false;
    }

    return true;
}


void ai_toy_audio_recoder_cb(audio_recorder_msg_t *msg, void *user_data)
{
    int rt = 0;
    TY_AI_TOY_T *ai_toy = (TY_AI_TOY_T *)user_data;

    switch (msg->state) {

    case AUDIO_RECODER_MODE_UPDATE:
        ai_toy_led_off();
        //! 播放停止
        ai_toy_player_stop(ai_toy);
        //! lcd 显示更新
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(&msg->mode, 1, TY_DISPLAY_TP_CHAT_MODE);
        #endif
        //! llm 中止处理
        ty_ai_proc_event_send(ai_toy->llm, AI_PROC_INTERRUPT_EVENT, NULL, 0);
        //! player 播放提示音
        ty_ai_toy_alert(TOY_ALART_TYPE_LONG_KEY_TALK + msg->mode, TRUE);
        //! 根据模式设置状态
        ai_toy_state_update(ai_toy, AI_TOY_IDLE);
        TAL_PR_DEBUG("AUDIO_RECODER_MODE_UPDATE");
        break;

    case AUDIO_RECODER_WAKEUP:  //! 唤醒打断
        TAL_PR_DEBUG("AUDIO_RECODER_WAKEUP");
    case AUDIO_RECODER_START:   //! 按键打断
        TAL_PR_DEBUG("AUDIO_RECODER_START");
        //! 显示状态更新
        ai_toy_led_on();
        //! 播放停止
        ai_toy_player_stop(ai_toy);
        //! llm 中止处理
        ty_ai_proc_event_send(ai_toy->llm, AI_PROC_INTERRUPT_EVENT, NULL, 0);
        TAL_PR_DEBUG("AI_PROC_INTERRUPT_EVENT");
        //! 播放提示音
        ty_ai_toy_alert(TOY_ALART_TYPE_WAKEUP, TRUE);
        //! 设备状态更新
        ai_toy_state_update(ai_toy, (AUDIO_RECODER_MODE_KEY_HOLD == msg->mode) ? AI_TOY_IDLE : AI_TOY_LISTEN);
        break;

    case AUDIO_RECODER_STOP:
        //! 显示状态更新
        TAL_PR_DEBUG("AUDIO_RECODER_STOP");
        ai_toy_led_off();
        //! 播放停止
        ai_toy_player_stop(ai_toy);
        ai_toy_state_update(ai_toy,  AI_TOY_IDLE);
        break;

    case AUDIO_RECODER_VAD_START:
        if (!ai_toy_state_interrupt_check(ai_toy, msg->mode)) {
            break;
        }

        //! TODO: 随意说状态更新
        TAL_PR_DEBUG("----------AUDIO_RECODER_VAD_START----------");
        ai_toy->vad_active = true;
        tal_sw_timer_stop(ai_toy->idle_timer);

        //! 播放停止
        ai_toy_player_stop(ai_toy);
        //! llm 中止处理
        // ty_ai_proc_event_send(ai_toy->llm, AI_PROC_INTERRUPT_EVENT, NULL, 0);
        //! led show
        ai_toy_led_flash(100);
        //! audio upload
        if (OPRT_OK != (rt = ty_ai_proc_event_send(ai_toy->llm, AI_PROC_AUDIO_EVENT, msg->data, msg->datalen))) {
            ai_toy_state_update(ai_toy,  AI_TOY_IDLE);
            if (rt == OPRT_NETWORK_ERROR) {
                ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_DISCONNECT, TRUE);
            }
            ai_toy_led_off();
        }
        break;

    case AUDIO_RECODER_VAD_SPEAK:
        if (!ai_toy_state_interrupt_check(ai_toy, msg->mode)) {
            break;
        }
        if (OPRT_OK != (rt = ty_ai_proc_event_send(ai_toy->llm, AI_PROC_AUDIO_EVENT, msg->data, msg->datalen))) {
            ai_toy_state_update(ai_toy,  AI_TOY_IDLE);
            if (rt == OPRT_NETWORK_ERROR) {
                ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_DISCONNECT, TRUE);
            }
            ai_toy_led_off();
        }
        break;

    case AUDIO_RECODER_VAD_END:
        if (!ai_toy_state_interrupt_check(ai_toy, msg->mode)) {
            break;
        }

        TAL_PR_DEBUG("----------AUDIO_RECODER_VAD_END----------");
        ai_toy_state_update(ai_toy, AI_TOY_UPLOAD);
        //! led end
        ai_toy_led_off();
        rt  = ty_ai_proc_event_send(ai_toy->llm, AI_PROC_AUDIO_EVENT, msg->data, msg->datalen);
        rt |= ty_ai_proc_event_send(ai_toy->llm, AI_PROC_FINSH_EVENT, NULL, 0);
        if (OPRT_OK != rt) {
            ai_toy_state_update(ai_toy,  AI_TOY_IDLE);
            if (rt == OPRT_NETWORK_ERROR) {
                ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_DISCONNECT, TRUE);
            }
        }
        TAL_PR_DEBUG("----------AUDIO_RECODER_FINSH----------");
        break;
    }
}

STATIC VOID ai_toy_text_stream_dump(int type, UCHAR_T *data, INT_T len)
{
    STATIC UCHAR_T buf[128] = {0};
    STATIC TIME_T ts = 0;
    switch (type) {
    case AI_PROC_TEXT_START:
        memset(buf, 0, sizeof(buf));
        ts = tal_time_get_posix_ms();
        TAL_PR_DEBUG("text stream start...");
        break;
    case AI_PROC_TEXT_DATA:
        while(len > 0) {
            if (strlen(buf) > 0 && strlen(buf) + len >= sizeof(buf) - 1) {
                TAL_PR_DEBUG("text stream: %s", buf);
                memset(buf, 0, sizeof(buf));
            }

            INT_T ret = MIN(len, sizeof(buf) - 1 - strlen(buf));
            memcpy(buf + strlen(buf), data, ret);
            len -= ret;
            data += ret;
            if (strlen(buf) >= sizeof(buf) - 1) {
                TAL_PR_DEBUG("text stream: %s", buf);
                memset(buf, 0, sizeof(buf));
            }
        }
        break;
    case AI_PROC_TEXT_STOP:
        if (strlen(buf) > 0) {
            TAL_PR_DEBUG("text stream: %s", buf);
            memset(buf, 0, sizeof(buf));
        }
        TAL_PR_DEBUG("text stream stop...");
        break;
    default:
        break;
    }
}

STATIC VOID ai_toy_text_display(int type, UCHAR_T *data, INT_T len)
{
    ai_toy_text_stream_dump(type, data, len);
#ifdef ENABLE_TUYA_UI
    tuya_ai_display_msg(data, len, TY_DISPLAY_TP_AI_CHAT);
#endif
}



static int ai_toy_proc_output_cb(ai_proc_msg_t *msg,  void *user_data)
{
    // PR_DEBUG("ai_toy_proc call event %d", msg->event);

    TY_AI_TOY_T *toy = (TY_AI_TOY_T *)user_data;

    switch (msg->event) {

    case AI_PROC_UPLOAD_DONE:
        TAL_PR_DEBUG("AI_PROC_UPLOAD_DONE %d", toy->state);
        if (toy->state == AI_TOY_UPLOAD) {
            ai_toy_state_update(toy, AI_TOY_THINK);
        }
        break;

    case AI_PROC_ASR_OK:
        ai_toy_led_on();
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(msg->data, msg->datalen, TY_DISPLAY_TP_HUMAN_CHAT);
        #endif
        break;

    case AI_PROC_ASR_EMPTY:
        ai_toy_state_update(toy, (AUDIO_RECODER_MODE_KEY_HOLD == audio_recorder_mode_get()) ? AI_TOY_IDLE : AI_TOY_LISTEN);
        break;

    case AI_PROC_ASR_TIMEOUT:
    case AI_PROC_UPLOAD_FAIL:
    case AI_PROC_TTS_ABORT: //！ TODO:
    case AI_PROC_TTS_TIMEOUT:
        ai_toy_state_update(toy, (AUDIO_RECODER_MODE_KEY_HOLD == audio_recorder_mode_get()) ? AI_TOY_IDLE : AI_TOY_LISTEN);
        // ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_DISCONNECT, TRUE);
        break;

    case AI_PROC_TTS_START:
        //! 播放&&TTS同步
        toy->player_stat = AI_TOY_PLAYER_REDAY;
        break;

    case AI_PROC_TTS_DATA:
        break;

    case AI_PROC_TTS_STOP:
        break;

    case AI_PROC_ASR_EMOJI:
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(msg->data, msg->datalen, TY_DISPLAY_TP_EMOJI);
        #endif
        break;

    case AI_PROC_TTS_EMOJI:
        break;

    //! 文本处理显示
    case AI_PROC_TEXT_START:
    case AI_PROC_TEXT_DATA:
    case AI_PROC_TEXT_STOP:
        ai_toy_text_display(msg->event, msg->data, msg->datalen);
        break;

    //! llm player control
    case AI_PROC_PLAY_CTL_PLAY:
        TAL_PR_DEBUG("AI_PROC_PLAY_CTL_PLAY");
        toy->player_resume_flag = 1;
        break;
    case AI_PROC_PLAY_CTL_PAUSE:
        TAL_PR_DEBUG("AI_PROC_PLAY_CTL_PAUSE");
        // 收到暂停或停止播放后，下次不再恢复播放音乐
        toy->player_resume_flag = 0;
        break;
    case AI_PROC_PLAY_CTL_REPLAY:
        TAL_PR_DEBUG("AI_PROC_PLAY_CTL_REPLAY");
        toy->player_reply_flag = 1;
        toy->player_resume_flag = 0;
        break;
    case AI_PROC_PLAY_CTL_RESUME:
        TAL_PR_DEBUG("AI_PROC_PLAY_CTL_RESUME");
        // 收到恢复播放后，下次恢复音乐
        toy->player_resume_flag = 1;
        break;
    case AI_PROC_CHAT_BREAK:
        TAL_PR_DEBUG("AI_PROC_CHAT_BREAK");
        //! 播放停止
        ai_toy_player_stop(toy);
        break;
    //! llm skill handle
    case AI_PROC_SKILL_TRIGGER:
        // 触发AI聊天: 先打断当前会话，然后发起新的文本会话
        //! 播放停止
        ai_toy_player_stop(toy);
        break;
    default:
        break;
    }

    return 0;
}

static INT_T ai_toy_h264_cb(TKL_VENC_FRAME_T *pframe)
{
    if (!s_ai_toy) {
        return 1;
    }

    if (pframe->pbuf == NULL || pframe->buf_size == 0) {
        return 0;
    }

    if (pframe->frametype != TKL_VIDEO_I_FRAME) {
        return 0;
    }

#if !ENABLE_VIDEO_ONE_FRAME
    if (!s_ai_toy->vad_active) {
        return 1;
    }
#endif
    // //! video input
    int rt = ty_ai_proc_event_send(s_ai_toy->llm, AI_PROC_VIDEO_EVENT, pframe->pbuf, pframe->buf_size);
    // TAL_PR_DEBUG("__h264_cb frame size %d", pframe->buf_size);

    return 0;
}


STATIC INT_T _event_ota_process_cb(VOID_T *data)
{
    // stop audio
    TAL_PR_NOTICE("ota process, stop audio...");
    tkl_ai_stop(0, 0);
    return 0;
}

STATIC INT_T _event_ota_fail_cb(VOID_T *data)
{
    // FIXME: restart system
    TAL_PR_NOTICE("ota fail, restart system...");
    tal_system_reset();
    return 0;
}

STATIC VOID _wf_nw_stat_cb(GW_WIFI_NW_STAT_E nw_stat)
{
    // get language type: 0: chinese, 1: english
    CHAR_T *region = get_gw_region();
    if (0 == strlen(region)) {
        CHAR_T ccode[COUNTRY_CODE_LEN] = {0};
        tal_wifi_get_country_code(ccode);        
        s_lang = (NULL != strstr(ccode, "CN")? 0 : TY_AI_DEFAULT_LANG);
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(&s_lang, 1, TY_DISPLAY_TP_LANGUAGE);
        #endif
        TAL_PR_DEBUG("network status = %d, ccode %s, language %d", nw_stat, ccode, s_lang);
    } else {
        s_lang = (NULL != strstr(region, "AY")? 0 : TY_AI_DEFAULT_LANG);
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(&s_lang, 1, TY_DISPLAY_TP_LANGUAGE);
        #endif
        TAL_PR_DEBUG("network status = %d, region %s, language %d", nw_stat, region, s_lang);
    }
    TY_AI_TOY_T *ctx = s_ai_toy;
    if (ctx) {
        ctx->nw_stat = nw_stat;
    }

    uint8_t net_stat = 0;

    STATIC BOOL_T report_flag = FALSE;
    if (nw_stat == STAT_CLOUD_CONN && !report_flag) {
        _report_sysinfo();
    }

    switch (nw_stat) {

    case STAT_UNPROVISION_AP_STA_UNCFG:
        // 软重启，未配网，播报配网提示
        ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_CFG, TRUE);
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(NULL, 0, TY_DISPLAY_TP_STAT_NETCFG);
        tuya_ai_display_msg(&net_stat, 1, TY_DISPLAY_TP_STAT_NET);
        #endif
        ai_toy_led_flash(200);
        break;

    case STAT_STA_DISC:
        #ifdef ENABLE_TUYA_UI   
        tuya_ai_display_msg(&net_stat, 1, TY_DISPLAY_TP_STAT_NET);
        #endif
        ai_toy_led_off();
        break;

    case STAT_CLOUD_CONN:
        net_stat = 1;
        tuya_ai_display_msg(&net_stat, 1, TY_DISPLAY_TP_STAT_NET);
        ai_toy_led_on();
        break;
    }
}

STATIC INT_T _event_netcfg_err_cb(VOID_T *data)
{
    AP_CFG_ERR_CODE error_status = (AP_CFG_ERR_CODE *)data;
    if (error_status != AP_CFG_GW_ACTIVE_SUCCESS) {
        TAL_PR_NOTICE("netcfg fail");
        ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_FAIL, TRUE);
    }
    return 0;
}

STATIC VOID __delayed_client_run(VOID *data)
{
    ty_ai_toy_alert(TOY_ALERT_TYPE_NETWORK_CONNECTED, TRUE);
    #ifdef ENABLE_TUYA_UI   
    tuya_ai_display_msg(NULL, 0, TY_DISPLAY_TP_STAT_ONLINE);
    #endif

    TAL_PR_DEBUG("lowpower_timer start");
    tal_sw_timer_start(s_ai_toy->lowpower_timer, TOY_DEEPSLEEP_TIMEOUT, TAL_TIMER_ONCE);
}

STATIC INT_T _event_client_run(VOID_T *data)
{
    TAL_PR_NOTICE("connected to server");
    tal_workq_start_delayed(s_ai_toy->wq_handle, TY_AI_ALERT_DELAY_MS, LOOP_ONCE);
    return 0;
}

STATIC OPERATE_RET __ai_toy_config_load(TY_AI_TOY_T *ai_toy)
{
    OPERATE_RET rt = OPRT_OK;
    BYTE_T *value = NULL;
    UINT_T len = 0;

    // set default volume first
    ai_toy->volume = TY_SPK_DEFAULT_VOL;

    // read volume from kv
    TUYA_CALL_ERR_RETURN(wd_common_read(AI_TOY_PARA, &value, &len));
    // parse volume {"volume": 70}
    TAL_PR_DEBUG("read ai_toy config: %s", value);
    cJSON *root = ty_cJSON_Parse((CONST CHAR_T *)value);
    wd_common_free_data(value);
    if (root == NULL) {
        TAL_PR_ERR("parse ai_toy config fail");
        return -1;
    }
    cJSON *child = ty_cJSON_GetObjectItem(root, "volume");
    if (child == NULL) {
        TAL_PR_ERR("parse volume fail");
        // default volume
        // save volume to kv
        CHAR_T *value = ty_cJSON_Print(root);
        rt = wd_common_write(AI_TOY_PARA, (CONST BYTE_T *)value, strlen(value));
        ty_cJSON_FreeBuffer(value);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("save volume to kv fail");
            ty_cJSON_Delete(root);
            return rt;
        }
    }
    if (child->valueint <= 100 && child->valueint >= 0) {
        ai_toy->volume = child->valueint;
    }
    ty_cJSON_Delete(root);
    return OPRT_OK;
}

STATIC OPERATE_RET _report_sysinfo(VOID)
{
    // report volume dp to cloud
    TAL_PR_DEBUG("report volume dp to cloud");
    CHAR_T *devid = tuya_iot_get_gw_id();
    TY_OBJ_DP_S dp = {
        .dpid = 3,
        .type = PROP_VALUE,
        .value.dp_value = s_ai_toy->volume,
    };
    return tuya_report_dp_async(devid, &dp, 1, NULL);
}



VOID ty_ai_toy_dp_cmd_cb(IN CONST TY_RECV_OBJ_DP_S *dp)
{
    for (UINT_T index = 0; index < dp->dps_cnt; index++) {
        // handle dp id = 3

        if (dp->dps[index].dpid == 8) {
        #if defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
            tuya_robot_action_set(dp->dps[index].value.dp_enum);
        #endif
            TAL_PR_DEBUG("SOC Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->dps[index].dpid, dp->dps[index].type, dp->dps[index].value.dp_enum);
        } else if (dp->dps[index].dpid == 3 && dp->dps[index].type == PROP_VALUE) {

            TAL_PR_DEBUG("SOC Rev DP Obj Cmd dpid:%d type:%d value:%d", dp->dps[index].dpid, dp->dps[index].type, dp->dps[index].value.dp_value);
            if (dp->dps[index].value.dp_value <= 100 && dp->dps[index].value.dp_value >= 0 && s_ai_toy->volume != dp->dps[index].value.dp_value) {
                // update cfg
                s_ai_toy->volume = dp->dps[index].value.dp_value;
                CHAR_T buf[32] = {0};
                snprintf(buf, sizeof(buf), "{\"volume\": %d}", s_ai_toy->volume);
                wd_common_write(AI_TOY_PARA, (CONST BYTE_T *)buf, strlen(buf));
                tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AO_0, NULL, s_ai_toy->volume);
                #ifdef ENABLE_TUYA_UI   
                tuya_ai_display_msg(&s_ai_toy->volume, 1, TY_DISPLAY_TP_VOLUME);
                #endif
                _report_sysinfo();
            }
        }
    }
}

#if defined(ENABLE_AUDIO_ANALYSIS) && (ENABLE_AUDIO_ANALYSIS == 1)
typedef struct {
    uint8_t  *data;
    uint32_t  datalen;
} audio_dump_t;
VOID_T __audio_test_cb(VOID *buf, UINT_T len, VOID *args)
{
    struct ipc_msg_s *msg = (struct ipc_msg_s *)buf;
    uint32_t event = msg->buf32[0];
    uint32_t data = msg->buf32[1];

    TAL_PR_DEBUG("play audio test %d", event);
    switch (event)
    {
        case AUDIO_TEST_EVENT_PLAY_BGM: {
            if (0 == data) {
                AUDIO_ANALYSIS_PARAMS_T params = {0};
                AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_RANG(&params);
                audio_analysis_play(AUDIO_ANALYSIS_TYPE_RANG, &params);
            } else if (1 == data) { /*单频*/
                uint32_t freq = msg->buf32[2];
                INT_T amp = msg->buf32[3];
                AUDIO_ANALYSIS_PARAMS_T params = {0};
                AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SINGLE(&params);
                if (freq > 0) {
                    params.freq = freq;
                }                
                audio_analysis_play(AUDIO_ANALYSIS_TYPE_SINGLE, &params);
            } else if (2 == data) { /*白噪声*/
                AUDIO_ANALYSIS_PARAMS_T params = {0};
                AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SWEEP(&params);
                audio_analysis_play(AUDIO_ANALYSIS_TYPE_SWEEP, &params);
            } else if (3 == data) { /*调频*/                
                AUDIO_ANALYSIS_PARAMS_T params = {0};
                INT_T amp = msg->buf32[2];
                if (amp < 0) {
                    params.amp = amp;
                }
                AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_SWEEPSPECIAL(&params);
                audio_analysis_play(AUDIO_ANALYSIS_TYPE_SWEEPSPECIAL, &params);
            } else if (4 == data) { /*最小信号*/
                uint32_t freq = msg->buf32[2];
                INT_T amp = msg->buf32[3];
                AUDIO_ANALYSIS_PARAMS_T params = {0};
                AUDIO_ANALYSIS_DEFAULT_PARAMS_GET_MINSIN(&params);
                if (freq > 0) {
                    params.freq = freq;
                }                
                audio_analysis_play(AUDIO_ANALYSIS_TYPE_MINSIN, &params);
            } 
            else {
                TAL_PR_ERR("unknown audio test data %d", data);
            }
        } break;
        case AUDIO_TEST_EVENT_SET_VOLUME: {
            tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AO_0, NULL, data);
        } break;
        case AUDIO_TEST_EVENT_SET_MICGAIN: {
            tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, data);
            TAL_PR_INFO("****************************** set mic gain %d", data);
        } break;
        case AUDIO_TEST_EVENT_SET_ALG_PARA: {
            TKL_AUDIO_ALG_TYPE type = data;
            uint32_t value = (uint32_t)msg->buf32[2];
            if (type == TKL_AUDIO_ALG_VAD_SPTHR) {
                TAL_PR_DEBUG("set audio alg para %d, value[%d] %d", type, (value >> 24) & 0xff, value & 0xffff);
            } else {
                TAL_PR_DEBUG("set audio alg para %d, value %d", type, value);
            }
            if (type > TKL_AUDIO_ALG_AEC_NULL && type < TKL_AUDIO_ALG_MAX) {
                tkl_ai_alg_para_set(type, &value);
            }
        } break;
        case AUDIO_TEST_EVENT_GET_ALG_PARA: {
            TKL_AUDIO_ALG_TYPE type = data;
            if (type > TKL_AUDIO_ALG_AEC_NULL && type < TKL_AUDIO_ALG_MAX) {
                uint32_t value = (uint32_t)msg->buf32[2];
                int ret = tkl_ai_alg_para_get(type, &value);
                if (ret == 0) {
                    if (type == TKL_AUDIO_ALG_VAD_SPTHR) {
                        TAL_PR_DEBUG("get audio alg para %d, value[%d] %d", type, (value >> 24) & 0xff, value & 0xffff);
                    } else {
                        TAL_PR_DEBUG("get audio alg para %d, value %d", type, value);
                    }
                }
            }
        } break;
        case AUDIO_TEST_EVENT_DUMP_ALG_PARA: {
            TAL_PR_DEBUG("dump audio alg para");
            tkl_ai_alg_para_dump();
        } break;
        case AUDIO_TEST_EVENT_NET_DUMP_AUDIO: {
            TAL_PR_DEBUG("net dump audio");
            uint32_t type = data;
            audio_dump_t *dump = (audio_dump_t *)msg->buf32[2];
#if defined(ENABLE_APP_AI_MONITOR) && (ENABLE_APP_AI_MONITOR == 1)
            if (dump && dump->data && dump->datalen > 0) {
                TAL_PR_DEBUG("net dump audio type %d, len %d", type, dump->datalen);
                // dump pcm data interval is 100ms
                for (int i = 0; i < dump->datalen; i += 3200) {
                    TAL_PR_DEBUG("net dump audio data offset %d, len %d", i, MIN(3200, dump->datalen - i));
                    AI_STREAM_TYPE stype = AI_STREAM_ONE;
                    if (i == 0 && i + 3200 >= dump->datalen) {
                        stype = AI_STREAM_ONE;
                    } else if (i == 0) {
                        stype = AI_STREAM_START;
                    } else if (i + 3200 >= dump->datalen) {
                        stype = AI_STREAM_END;
                    } else {
                        stype = AI_STREAM_ING;
                    }

                    if (type == 0) {
                        tuya_ai_monitor_broadcast_audio_mic(stype, dump->data + i, MIN(3200, dump->datalen - i));
                    } else if (type == 1) {
                        tuya_ai_monitor_broadcast_audio_ref(stype, dump->data + i, MIN(3200, dump->datalen - i));
                    } else if (type == 2) {
                        tuya_ai_monitor_broadcast_audio_aec(stype, dump->data + i, MIN(3200, dump->datalen - i));
                    }
                }
                dump->datalen = 0; // reset dump data length
            }
#else
            TAL_PR_ERR("ENABLE_APP_AI_MONITOR is not enabled, can not net dump audio");
#endif

        } break;
        default:
            break;
    }
}
#endif // end of ENABLE_AUDIO_ANALYSIS

OPERATE_RET ty_ai_toy_hardware_init(TY_AI_TOY_T *ai_toy, TY_AI_TOY_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    if (!ai_toy || !cfg) {
        return OPRT_INVALID_PARM;
    }

    // save configure
    memcpy(&ai_toy->cfg, cfg, sizeof(TY_AI_TOY_CFG_T));

    // init gpio
    TUYA_GPIO_BASE_CFG_T key_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_HIGH
    };

    KEY_USER_DEF_S trigger_pin;
    trigger_pin.port                = cfg->audio_trigger_pin;
    trigger_pin.low_level_detect    = TRUE;
    trigger_pin.lp_tp               = LP_ONCE_TRIG;
    trigger_pin.long_key_time       = LONG_KEY_TIME;
    trigger_pin.seq_key_detect_time = 200;
    trigger_pin.call_back           = ai_toy_key_process;
    key_init(NULL, 0, 20);
    reg_proc_key(&trigger_pin);
    TUYA_CALL_ERR_LOG(tkl_gpio_init(cfg->audio_trigger_pin, &key_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(cfg->audio_trigger_pin));

    TAL_PR_DEBUG("tkl_gpio_init %d", cfg->audio_trigger_pin);

#if (defined(T5AI_BOARD_EVB)   && T5AI_BOARD_EVB   == 1) || (defined(T5AI_BOARD_CELLULAR) && (T5AI_BOARD_CELLULAR == 1)) || \
    (defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1) || (defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1)
    KEY_USER_DEF_S net_pin;
#if defined(T5AI_BOARD_EVB) && (T5AI_BOARD_EVB == 1)
    net_pin.port                    = TUYA_GPIO_NUM_12;
#elif defined(T5AI_CELLULAR_BOARD) && (T5AI_CELLULAR_BOARD == 1)
    net_pin.port                    = TUYA_GPIO_NUM_8;
#elif defined(T5AI_BOARD_ROBOT) && (T5AI_BOARD_ROBOT == 1)
    net_pin.port                    = TUYA_GPIO_NUM_4;
#elif defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1
    net_pin.port                    = TUYA_GPIO_NUM_7;
#endif
    net_pin.low_level_detect        = TRUE;
    net_pin.lp_tp                   = LP_ONCE_TRIG;
    net_pin.long_key_time           = LONG_KEY_TIME * 10;
    net_pin.seq_key_detect_time     = 200;
    net_pin.call_back               = ai_toy_key_process;
    reg_proc_key(&net_pin);
    TUYA_CALL_ERR_LOG(tkl_gpio_init(net_pin.port, &key_cfg));
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(net_pin.port));
#endif

    //! led init
    if (cfg->led_pin != TUYA_GPIO_NUM_MAX) {
        tuya_create_led_handle(cfg->led_pin, OL_HIGH, &s_ai_toy_led);
    }

    //! audio
    audio_recorder_cfg_t audio_cfg;
    AUDIO_RECORDER_CFG_INIT(&audio_cfg, ai_toy_audio_recoder_cb, ai_toy, cfg->trigger_mode, cfg->spk_en_pin, TUYA_GPIO_LEVEL_LOW);
    audio_recorder_init(&audio_cfg);

    tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, TKL_AO_0, NULL, ai_toy->volume);
#if defined(ENABLE_AUDIO_ANALYSIS) && (ENABLE_AUDIO_ANALYSIS == 1)
    tkl_audio_test_init(__audio_test_cb, NULL);
    TAL_PR_NOTICE(">>> audio test init <<<");
#endif // end of ENABLE_AUDIO_ANALYSIS

    TAL_PR_NOTICE("ai toy hardware init sucess");

    return OPRT_OK;
}


static void ai_toy_camera_worq(void *args)
{
    TAL_PR_NOTICE("ai toy camera init event");

#if (defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1)
    // uvc
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type                = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type  = TKL_VI_CAMERA_TYPE_UVC;
    ext_conf.camera.fmt          = TKL_CODEC_VIDEO_MJPEG;
    ext_conf.camera.power_pin    = TUYA_GPIO_NUM_6;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;

    vi_config.isp.width = 640;
    vi_config.isp.height = 480;
    vi_config.isp.fps = 15;
    vi_config.pdata = &ext_conf;

    tkl_vi_init(&vi_config, 0);

    TKL_VENC_CONFIG_T h264_config = {0};
    h264_config.enable_h264_pipeline = 1;
    h264_config.put_cb = ai_toy_h264_cb;
    tkl_venc_init(0, &h264_config, 0);
#else
    // dvp
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type = TKL_VI_CAMERA_TYPE_DVP;
    ext_conf.camera.fmt = TKL_CODEC_VIDEO_MJPEG;

    ext_conf.camera.power_pin = TUYA_GPIO_NUM_51;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;
    ext_conf.camera.i2c.clk = TUYA_GPIO_NUM_13;
    ext_conf.camera.i2c.sda = TUYA_GPIO_NUM_15;

    vi_config.isp.width = 480;
    vi_config.isp.height = 480;
    vi_config.isp.fps = 10;
    vi_config.pdata = &ext_conf;

    tkl_vi_init(&vi_config, 0);

    //! dvp
    TKL_VENC_CONFIG_T h264_config;
    // DVP:0, UVC:1
    h264_config.enable_h264_pipeline = 0; // dvp
    h264_config.put_cb = ai_toy_h264_cb;
    tkl_venc_init(0, &h264_config, 0);
    bk_printf("----- dvp end\r\n");
#endif

    DELAYED_WORK_HANDLE delayed_work = *(DELAYED_WORK_HANDLE *)args;
    tal_workq_cancel_delayed(delayed_work);
}

static int ai_toy_camera_event(void *args)
{
    static DELAYED_WORK_HANDLE delayed_work;
    //! dvp摄像头枚举时间较长，这里延后加快连接大模型
    tal_workq_init_delayed(WORKQ_SYSTEM, ai_toy_camera_worq, &delayed_work, &delayed_work);
    tal_workq_start_delayed(delayed_work, 500, LOOP_ONCE);
}

VOID_T tuya_ai_camera_init(VOID_T)
{
    ty_subscribe_event(EVENT_AI_CLIENT_RUN, "ai_toy", ai_toy_camera_event, SUBSCRIBE_TYPE_ONETIME);
}

STATIC VOID ai_toy_idle_timer(TIMER_ID timer_id, VOID_T *arg)
{
    TY_AI_TOY_T *ctx = (TY_AI_TOY_T *)arg;
    TAL_PR_NOTICE("ai proc ai_toy_idle_timer");

    //! 需要重新唤醒
    if (!tuya_speaker_service_is_playing() && !tuya_speaker_service_tone_is_playing()) {
        audio_recorder_stop();
    } else {
        tal_sw_timer_start(ctx->idle_timer, TOY_IDLE_TIMEOUT, TAL_TIMER_ONCE);
    }
}

static void __set_wakeup_source(uint32_t pin)
{
    TAL_PR_NOTICE("ai proc ai_toy_lowpower_timer pin %d", pin);
    TUYA_GPIO_BASE_CFG_T io_cfg;
    io_cfg.direct = TUYA_GPIO_INPUT;
    io_cfg.mode = TUYA_GPIO_FLOATING;
    io_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(pin, &io_cfg);

    TUYA_WAKEUP_SOURCE_BASE_CFG_T cfg;
    cfg.source = TUYA_WAKEUP_SOURCE_GPIO;
    cfg.wakeup_para.gpio_param.gpio_num = pin;
    cfg.wakeup_para.gpio_param.level = TUYA_GPIO_WAKEUP_RISE;
    tkl_wakeup_source_set(&cfg);
    tal_system_sleep(200);
}

STATIC VOID ai_toy_lowpower_timer(TIMER_ID timer_id, VOID_T *arg)
{
#ifdef TY_AI_DEFAULT_LOWP_MODE    
    // music or story 
    if (tuya_audio_player_get_status(TUYA_AUDIO_PLAYER_TYPE_MUSIC) == TUYA_PLAYER_STATE_PLAYING) {
        TAL_PR_NOTICE("music or story playing, restart timer!"); 
        tal_sw_timer_start(s_ai_toy->lowpower_timer, TOY_DEEPSLEEP_TIMEOUT, TAL_TIMER_ONCE);
        return;
    }

    OPERATE_RET rt = OPRT_OK;
    TAL_PR_NOTICE("ai proc ai_toy_lowpower_timer"); 
    if (TY_AI_DEFAULT_LOWP_MODE == TUYA_CPU_DEEP_SLEEP) {

        // set wakeup source
        __set_wakeup_source(s_ai_toy->cfg.audio_trigger_pin);

        // enter deepsleep status
        tal_cpu_sleep_mode_set(1, TUYA_CPU_DEEP_SLEEP);
    } else if (TY_AI_DEFAULT_LOWP_MODE == TUYA_CPU_SLEEP) {
        // close battery report
        #if defined(TUYA_AI_TOY_BATTERY_ENABLE) && (TUYA_AI_TOY_BATTERY_ENABLE == 1)
        tuya_ai_toy_battery_uninit();
        #endif        
        
        // close PA
        tkl_gpio_write(s_ai_toy->cfg.spk_en_pin, TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_write(s_ai_toy->cfg.led_pin, TUYA_GPIO_LEVEL_LOW);
        
        // close LCD
        tkl_disp_set_brightness(NULL, 0);
        
        // enter keep-alive status
        rt = tal_cpu_lp_enable();
        rt |= tal_wifi_lp_enable();
        s_ai_toy->lp_stat = TRUE;
        
        ty_ai_toy_destroy();
        TAL_PR_DEBUG("tal_cpu_lp_enable rt=%d", rt);  
    }
#endif
}

OPERATE_RET ty_ai_toy_alert(TY_AI_TOY_ALERT_TYPE type, BOOL_T send_eof)
{
    OPERATE_RET ret = OPRT_OK;

    TAL_PR_DEBUG("toy alert type=%d", type);
    CONST CHAR_T *audio_data = NULL;
    UINT32_T audio_size = 0;
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
    CONST CHAR_T *alert_prompt = NULL;
#endif

    switch (type) {
    case TOY_ALERT_TYPE_POWER_ON:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "欢迎来到我的世界" : "Welcome to my world";
#else
        audio_data = (s_lang == 0) ? media_src_prologue_zh : media_src_prologue_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_prologue_zh) : sizeof(media_src_prologue_en);     
#endif
        break;

    case TOY_ALERT_TYPE_NOT_ACTIVE:
        audio_data = (s_lang == 0) ? media_src_network_conn_zh : media_src_network_conn_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_network_conn_zh) : sizeof(media_src_network_conn_en);
        break;
        
    case TOY_ALERT_TYPE_NETWORK_CFG:
        audio_data = (s_lang == 0) ? media_src_network_config_zh : media_src_network_config_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_network_config_zh) : sizeof(media_src_network_config_en);   
        break;

    case TOY_ALERT_TYPE_NETWORK_CONNECTED:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "联网成功" : "Network connected successfully";
#else
        audio_data = (s_lang == 0) ? media_src_network_conn_success_zh : media_src_network_conn_success_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_network_conn_success_zh) : sizeof(media_src_network_conn_success_en); 
#endif
        break;

    case TOY_ALERT_TYPE_NETWORK_FAIL:
        audio_data = (s_lang == 0) ? media_src_network_conn_failed_zh : media_src_network_conn_failed_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_network_conn_failed_zh) : sizeof(media_src_network_conn_failed_en);  
        break;

    case TOY_ALERT_TYPE_NETWORK_DISCONNECT:

    audio_data = (s_lang == 0) ? media_src_network_reconfigure_zh : media_src_network_reconfigure_en;
    audio_size = (s_lang == 0) ? sizeof(media_src_network_reconfigure_zh) : sizeof(media_src_network_reconfigure_en);
        break;
    case TOY_ALERT_TYPE_BATTERY_LOW:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        if (ty_ai_chat_is_online()) {
            alert_prompt = (s_lang == 0) ? "电量不足" : "Low battery";
        } else {
            audio_data = (s_lang == 0) ? media_src_low_battery_zh : media_src_low_battery_en;
            audio_size = (s_lang == 0) ? sizeof(media_src_low_battery_zh) : sizeof(media_src_low_battery_en);
        }
#else
        audio_data = (s_lang == 0) ? media_src_low_battery_zh : media_src_low_battery_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_low_battery_zh) : sizeof(media_src_low_battery_en);
#endif
        break;

    case TOY_ALERT_TYPE_PLEASE_AGAIN:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "请再说一遍" : "Please say again";
#else
        audio_data = (s_lang == 0) ? media_src_please_again_zh : media_src_please_again_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_please_again_zh) : sizeof(media_src_please_again_en);
#endif
        break;
        
    case TOY_ALART_TYPE_WAKEUP:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "你好我在" : "Hello, I am here";
#else
        audio_data = (s_lang == 0) ? media_src_ai_zh : media_src_ai_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_ai_zh) : sizeof(media_src_ai_en);
#endif
        break;

    case TOY_ALART_TYPE_LONG_KEY_TALK:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "长按对话" : "Long press to talk";
#else
        audio_data = (s_lang == 0) ? media_src_long_press_zh : media_src_long_press_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_long_press_zh) : sizeof(media_src_long_press_en);    
#endif
        break;

    case TOY_ALART_TYPE_KEY_TALK:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "按键对话" : "Press to talk";
#else
        audio_data = (s_lang == 0) ? media_src_press_talk_zh : media_src_press_talk_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_press_talk_zh) : sizeof(media_src_press_talk_en); 
#endif
        break;

    case TOY_ALART_TYPE_WAKEUP_TALK:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "唤醒对话" : "Wake up to talk";
#else
        audio_data = (s_lang == 0) ? media_src_wakeup_chat_zh : media_src_wakeup_chat_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_wakeup_chat_zh) : sizeof(media_src_wakeup_chat_en); 
#endif
        break;
        
    case TOY_ALART_TYPE_RANDOM_TALK:
#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
        alert_prompt = (s_lang == 0) ? "随意对话" : "Free talk";
#else
        audio_data = (s_lang == 0) ? media_src_free_chat_zh : media_src_free_chat_en;
        audio_size = (s_lang == 0) ? sizeof(media_src_free_chat_zh) : sizeof(media_src_free_chat_en); 
#endif
        break;
    default:
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_CLOUD_ALERT) && (ENABLE_CLOUD_ALERT == 1)
    if (alert_prompt) {
        ret = tuya_ai_chat_proto_request_alert(alert_prompt);
        if (ret != OPRT_OK) {
            TAL_PR_ERR("tuya_ai_chat_proto_request_alert failed, ret=%d", ret);
            return ret;
        }
        return ret;
    }
#endif

    tuya_speaker_service_tone_play_data(AI_TOY_ALERT_PLAY_ID, TUYA_AI_CHAT_AUDIO_FORMAT_MP3, audio_data, audio_size);

    return ret;
}



OPERATE_RET ty_ai_toy_create(TY_AI_TOY_T **ai_toy)
{
    OPERATE_RET rt = OPRT_OK;
    TY_AI_TOY_T *toy;

    toy = tal_psram_malloc(sizeof(TY_AI_TOY_T));
    if (toy == NULL) {
        TAL_PR_ERR("ai_toy malloc failed");
        goto __error;
    }
    memset(toy, 0, sizeof(TY_AI_TOY_T));
    TAL_PR_NOTICE("ty_ai_toy_create...");

    ty_ai_proc_cfg_t cfg = {
        .output_cb = ai_toy_proc_output_cb,
        .user_data = toy
    };
    
    toy->llm = ty_ai_proc_create(&cfg);
    if (toy->llm  == NULL) {
        TAL_PR_ERR("toy->llm  malloc failed");
        goto __error;
    }

    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(ai_toy_idle_timer, toy, &toy->idle_timer), __error);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(ai_toy_lowpower_timer, toy, &toy->lowpower_timer), __error);
    TUYA_CALL_ERR_GOTO(tal_workq_init_delayed(WORKQ_SYSTEM, __delayed_client_run, NULL, &toy->wq_handle), __error);

    __ai_toy_config_load(toy);

    *ai_toy = toy;

    TAL_PR_NOTICE("ty_ai_toy_create success");
    return OPRT_OK;

__error:
    if (toy) {
        if (toy->llm) {
            ty_ai_proc_destroy(toy->llm);
        }

        if (toy->idle_timer) {
            tal_sw_timer_delete(toy->idle_timer);
        }

        if (toy->lowpower_timer){
            tal_sw_timer_delete(toy->lowpower_timer);
        }

        if (toy->wq_handle) {
            tal_workq_cancel_delayed(toy->wq_handle);
        }

        tal_psram_free(toy);
    }

    return rt;
}


OPERATE_RET ty_ai_toy_destroy(VOID)
{
    OPERATE_RET rt = OPRT_OK;

    TAL_PR_NOTICE("ty_ai_toy_destroy...");

    // unsubscribe event
    ty_unsubscribe_event(EVENT_OTA_PROCESS_NOTIFY, "ai_toy", _event_ota_process_cb);
    ty_unsubscribe_event(EVENT_OTA_FAILED_NOTIFY,  "ai_toy", _event_ota_fail_cb);
    ty_unsubscribe_event(EVENT_NETCFG_ERROR, "ai_toy", _event_netcfg_err_cb);
    ty_unsubscribe_event(EVENT_AI_CLIENT_RUN, "ai_toy", _event_client_run);

    // should stop audio first
    TAL_PR_DEBUG("tkl_ai_stop...");
    TUYA_CALL_ERR_LOG(tkl_ai_stop(TKL_AUDIO_TYPE_BOARD, 0));
    if (NULL == s_ai_toy) {
        TAL_PR_NOTICE("s_ai_toy is null...");
        return OPRT_INVALID_PARM;
    }

    TY_AI_TOY_T *ctx = s_ai_toy;

    //! TODO: thread realse
    // destroy wq_handle
    if (ctx->wq_handle) {
        tal_workq_cancel_delayed(ctx->wq_handle);
    }

    tkl_system_psram_free(ctx);
    s_ai_toy = NULL;

    TUYA_CALL_ERR_LOG(tkl_ai_uninit());

    TAL_PR_NOTICE("ty_ai_toy_destroy success");

    return OPRT_OK;
}

UINT8_T tuya_ai_get_lang()
{
    return s_lang;
}


OPERATE_RET ty_ai_toy_start(TY_AI_TOY_T *ai_toy)
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == ai_toy) {
        return OPRT_RESOURCE_NOT_READY;
    }

    TY_AI_TOY_T *ctx = ai_toy;

    ctx->ops.network_status_get   = _network_status_get;

#if defined(TUYA_UPLOAD_DEBUG) && (TUYA_UPLOAD_DEBUG == 1)
    ctx->ops.upload_start = ty_debug_upload_start_cb;
    ctx->ops.upload_data = ty_debug_upload_data_cb;
    ctx->ops.upload_stop = ty_debug_upload_stop_cb;
    ty_debug_init();
#endif

    // subscribe ota event
    // ty_subscribe_event(EVENT_RESET,              "ai_toy", _event_reset_cb, SUBSCRIBE_TYPE_EMERGENCY);
    ty_subscribe_event(EVENT_OTA_PROCESS_NOTIFY, "ai_toy", _event_ota_process_cb, SUBSCRIBE_TYPE_NORMAL);
    ty_subscribe_event(EVENT_OTA_FAILED_NOTIFY,  "ai_toy", _event_ota_fail_cb, SUBSCRIBE_TYPE_NORMAL);
    // ty_subscribe_event(EVENT_NETCFG_ERROR,       "ai_toy", _event_netcfg_err_cb, SUBSCRIBE_TYPE_NORMAL);
    ty_subscribe_event(EVENT_AI_CLIENT_RUN,     "ai_toy", _event_client_run, SUBSCRIBE_TYPE_NORMAL);

    TUYA_SPEAKER_SERVICE_CONFIG_S player_cfg = {0};
    TUYA_CALL_ERR_GOTO(tuya_speaker_service_init(&player_cfg), __error);
    TUYA_CALL_ERR_GOTO(tuya_audio_player_set_event_callback(_event_play_status_cb, ai_toy), __error);

    tuya_iot_reg_get_wf_nw_stat_cb(_wf_nw_stat_cb);

    // tuya_ai_display_msg(&ai_toy->volume, 1, TY_DISPLAY_TP_VOLUME);

#if defined(ENABLE_APP_AI_MONITOR) && (ENABLE_APP_AI_MONITOR == 1)
    // start ai monitor
    ai_monitor_config_t monitor_cfg = AI_MONITOR_CFG_DEFAULT;
    TUYA_CALL_ERR_RETURN(tuya_ai_monitor_init(&monitor_cfg));
#endif

    ty_ai_proc_start(ctx->llm);

    TAL_PR_NOTICE("ty_ai_toy_start success");

    return OPRT_OK;

__error:

    TAL_PR_ERR("ty_ai_toy_start failed");
    ty_ai_toy_destroy();
    return rt;
}

#if defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
OPERATE_RET robot_emoji_show(uint8_t dir)
{
    static const char *emotion[] = {
        "neutral",
        "annoyed",
        "cool",
        "delicious",
        "fearful",
        "lovestruck",
        "unamused",
        "winking",
        "zany",
        /*----------------- */
        "angry",
        "confused",
        "disappointed",
        "embarrassed",
        "happy",
        "laughing",
        "relaxed",
        "sad",
        "surprise",
        "thinking",
    };
    static uint8_t index = 0;
    uint8_t num = sizeof(emotion) / sizeof(emotion[0]);


    if (dir) {
        index = (index + 1) % num;
    } else {
        index = (0 == index) ? num - 1 : index - 1;
    }

    if (index > num) {
        return OPRT_NOT_EXIST;
    }

    return tuya_ai_display_msg(emotion[index], strlen(emotion[index]), TY_DISPLAY_TP_EMOJI);
}


 VOID ai_robot_gesture_cb(GESTURE_TYPE_E gesture)
 {
    //TAL_PR_NOTICE("ai_toy_gesture_cb gesture %d", gesture);
    if (gesture == GESTURE_LEFT) {
        robot_emoji_show(0);
    } else if (gesture == GESTURE_RIGHT) {
        robot_emoji_show(1);
    }
} 
#endif

int ty_ai_vad_config_with_hardware(void)
{
    // spthr_idx[8] || rev[8] || value[16]
    uint32_t SPthr[6] = {
        0 << 24,
        1 << 24, 
        2 << 24,
        3 << 24,
        4 << 24,
        5 << 24
    };
    //! tkl_audio.c 有推荐说明
    //！参考T5音频调试指南
#if defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
    //! 0 vad power
    SPthr[0] |= 2000;
    //! vad active
    SPthr[1] |= 90;
    SPthr[3] |= 4;
    SPthr[4] |= 5;
    //! vad end
    SPthr[2] |= 150;
    SPthr[5] |= 9;

    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[0]);
    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[1]);
    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[3]);
    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[4]);
    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[2]);
    tkl_ai_alg_para_set(TKL_AUDIO_ALG_VAD_SPTHR, &SPthr[5]);
#endif
}

OPERATE_RET ty_ai_toy_init(TY_AI_TOY_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }
    if (s_ai_toy) {
        return OPRT_OK;
    }

    TAL_PR_NOTICE("ai_toy_init");
    //! TODO:
    tal_wifi_lp_disable();
    TUYA_CALL_ERR_GOTO(ty_ai_toy_create(&s_ai_toy), __error);
    TUYA_CALL_ERR_GOTO(ty_ai_toy_hardware_init(s_ai_toy, cfg), __error);
    // start ai toy
    TUYA_CALL_ERR_LOG(ty_ai_toy_start(s_ai_toy));

#if defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
    tuya_robot_action_init();
    app_gesture_init(ai_robot_gesture_cb);
#endif

    ty_ai_vad_config_with_hardware();

    return OPRT_OK;

__error:
    ty_ai_toy_destroy();

    return rt;
}
