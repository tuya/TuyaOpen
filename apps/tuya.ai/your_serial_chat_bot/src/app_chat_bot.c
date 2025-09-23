/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "tkl_memory.h"
#include "tkl_thread.h"
#include "tal_api.h"

#include "ai_audio.h"
#include "app_chat_bot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USER_TEXT_UART TUYA_UART_NUM_0
/***********************************************************
***********************typedef define***********************
***********************************************************/
THREAD_HANDLE sg_ai_text_hdl = NULL;
static uint8_t _serial_text_buf[256] = {0};
/***********************************************************
***********************const declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static void __app_ai_audio_evt_inform_cb(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{
    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {

    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_START: {

    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA: {
        tal_uart_write(USER_TEXT_UART, data, len);
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_END: {

    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_INTERUPT: {

    } break;
    case AI_AUDIO_EVT_AI_REPLIES_EMO: {
        AI_AUDIO_EMOTION_T *emo;
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        emo = (AI_AUDIO_EMOTION_T *)data;
        if (emo) {
            if (emo->name) {
                PR_DEBUG("emotion name:%s", emo->name);
            }

            if (emo->text) {
                // PR_DEBUG("emotion text:%s", emo->text);
                tal_uart_write(USER_TEXT_UART, (uint8_t *)emo->text, strlen(emo->text));
            }
        }
    } break;
    case AI_AUDIO_EVT_ASR_WAKEUP: {

    } break;

    default:
        break;
    }

    return;
}

static void __app_ai_audio_state_inform_cb(AI_AUDIO_STATE_E state)
{
    PR_DEBUG("ai audio state: %d", state);
    switch (state) {
    case AI_AUDIO_STATE_STANDBY:
        break;
    case AI_AUDIO_STATE_LISTEN:
        break;
    case AI_AUDIO_STATE_UPLOAD:
        break;
    case AI_AUDIO_STATE_AI_SPEAK:
        break;
    default:
        break;
    }
}

void __uart_text_scan_task(void *arg)
{
    while (1) {
        int len = tal_uart_read(USER_TEXT_UART, _serial_text_buf, sizeof(_serial_text_buf));
        tal_uart_write(TUYA_UART_NUM_1, _serial_text_buf, len);
        ai_text_agent_upload(_serial_text_buf, len);
        tal_system_sleep(100);
    }
}
OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    ai_audio_cfg.work_mode = AI_AUDIO_WORK_ASR_WAKEUP_FREE_TALK;
    ai_audio_cfg.evt_inform_cb = __app_ai_audio_evt_inform_cb;
    ai_audio_cfg.state_inform_cb = __app_ai_audio_state_inform_cb;

    TAL_UART_CFG_T uart_cfg = {0};
    uart_cfg.base_cfg.baudrate = 115200;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.rx_buffer_size = 512;
    uart_cfg.open_mode = O_BLOCK;
    tal_uart_init(USER_TEXT_UART, &uart_cfg);

    TUYA_CALL_ERR_RETURN(tkl_thread_create_in_psram(&sg_ai_text_hdl, "uart_text_handle", 1024 * 4, THREAD_PRIO_1,
                                                  __uart_text_scan_task, NULL));
    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

    return OPRT_OK;
}
