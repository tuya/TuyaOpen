/**
 * @file game_ai_chat.c
 * @version 0.1
 * @date 2025-03-25
 */
#include "tal_api.h"

#include "app_display.h"
#include "game_pet.h"
#include "uart_expand.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static volatile BOOL_T sg_text_stream_active = FALSE;  /* Whether text stream is active */

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_chat_handle_event(AI_NOTIFY_EVENT_T *event)
{
    AI_NOTIFY_TEXT_T *text = NULL;

    switch(event->type) {
        case AI_USER_EVT_TEXT_STREAM_START: {
            sg_text_stream_active = TRUE;

            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                uart_print_write((const uint8_t *)text->data, text->datalen);
            }
        }
        break;
        case AI_USER_EVT_TEXT_STREAM_DATA: {
            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                uart_print_write((const uint8_t *)text->data, text->datalen);
            }
        }
        break;
        case AI_USER_EVT_TEXT_STREAM_STOP: {
            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                uart_print_write((const uint8_t *)text->data, text->datalen);
            }

            sg_text_stream_active = FALSE;
        }
        break;
        default:
            break;
    }
}

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static void __ai_video_display_flush(TDL_CAMERA_FRAME_T *frame)
{
    #if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_camera_flush(frame->data, frame->width, frame->height);
    #endif
}
#endif

OPERATE_RET game_ai_chat_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    //custom ui register
    TUYA_CALL_ERR_RETURN(ai_ui_chat_register());

    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_HOLD,
        .default_vol = 70,
        .evt_cb = __ai_chat_handle_event,
    };
    TUYA_CALL_ERR_RETURN(ai_chat_init(&ai_chat_cfg));

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    AI_VEDIO_CFG_T ai_video_cfg = {
        .disp_flush_cb = __ai_video_display_flush,
    };

    TUYA_CALL_ERR_LOG(ai_video_init(&ai_video_cfg));
#endif

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
    TUYA_CALL_ERR_RETURN(ai_mcp_init());
#endif

    TUYA_CALL_ERR_RETURN(uart_expand_init());

    return OPRT_OK;
}

/**
 * @brief Get text stream status
 * @return TRUE=text stream active, FALSE=text stream ended
 */
BOOL_T app_get_text_stream_status(void)
{
    return sg_text_stream_active;
}