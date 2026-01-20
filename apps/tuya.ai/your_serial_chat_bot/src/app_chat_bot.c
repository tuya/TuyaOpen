/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "tal_api.h"

#include "cJSON.h"
#include "tuya_ai_agent.h"

#include "ai_agent.h"
#include "ai_user_event.h"

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
#include "ai_mcp_server.h"
#include "ai_mcp.h"
#endif

#include "app_chat_bot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USER_CHAT_UART             TUYA_UART_NUM_0

#define USER_UART_BAUDRATE          115200
#define USER_UART_RX_BUF_SIZE       1024

#define TEXT_CONTENT_MAX_LEN        512
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************const declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_uart_scan_hdl = NULL;
static char sg_serial_text_buf[TEXT_CONTENT_MAX_LEN+1] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __ai_chat_handle_event(AI_NOTIFY_EVENT_T *event)
{
    AI_NOTIFY_TEXT_T *text = NULL;

    if(NULL == event) {
        return;
    }

    switch(event->type) {
        case AI_USER_EVT_TEXT_STREAM_START:{
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"ai replay:\r\n", strlen("ai replay:\r\n"));

            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)text->data, text->datalen);
            }
        }
        break;
        case AI_USER_EVT_TEXT_STREAM_DATA:{
            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)text->data, text->datalen);
            }
        }
        break;
        case AI_USER_EVT_TEXT_STREAM_STOP:{
            text = (AI_NOTIFY_TEXT_T *)event->data;
            if(text && text->datalen > 0 && text->data) {
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)text->data, text->datalen);
            }

            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"\r\n", 2);
        }
        break;
        case AI_USER_EVT_LLM_EMOTION:
        case AI_USER_EVT_EMOTION:{
            AI_NOTIFY_EMO_T *emo = (AI_NOTIFY_EMO_T *)(event->data);

            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"emo: \r\n", strlen("emo: \r\n"));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)emo->name, strlen(emo->name));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)" ", strlen(" "));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)emo->emoji, strlen(emo->emoji));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"\r\n", 2);
        }
        break;
        default:
            break;
    }
}

/**
@brief MQTT connected event callback
@param data Event data (unused)
@return int Operation result
*/
static int __ai_mqtt_connected_evt(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(ai_agent_init());

    return rt;
}

static void __uart_text_scan_task(void *arg)
{
    int text_len = 0;
    bool need_upload = false;
    uint8_t ch = 0;

    while (1) {
        tal_uart_read(USER_CHAT_UART, &ch, 1);

        if(ch == '\r' || ch == '\n') {
            if(text_len == 0) {
                continue;
            } else {
                sg_serial_text_buf[text_len] = '\0';
                need_upload = true;
            }
        } else {
            if(text_len < TEXT_CONTENT_MAX_LEN) {
                sg_serial_text_buf[text_len++] = ch;
            }else {
                need_upload = true;
            }
        }

        if(true == need_upload) {
            ai_agent_send_text(sg_serial_text_buf);
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"user:\r\n", strlen("user:\r\n"));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)sg_serial_text_buf, text_len);
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"\r\n", 2);
            text_len = 0;
            need_upload = false;
        }
    }
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    ai_user_event_notify_register(__ai_chat_handle_event);

    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_MQTT_CONNECTED, "ai_agent_init", __ai_mqtt_connected_evt, SUBSCRIBE_TYPE_NORMAL));

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
    TUYA_CALL_ERR_RETURN(ai_mcp_init());
#endif

    TAL_UART_CFG_T uart_cfg = {0};
    uart_cfg.base_cfg.baudrate = USER_UART_BAUDRATE;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.base_cfg.parity   = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.rx_buffer_size    = USER_UART_RX_BUF_SIZE;
    uart_cfg.open_mode         = O_BLOCK;
    TUYA_CALL_ERR_RETURN(tal_uart_init(USER_CHAT_UART, &uart_cfg));

    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 4096;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "uart_text_task";
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_uart_scan_hdl, NULL, NULL, __uart_text_scan_task, NULL, &thrd_param));

    return OPRT_OK;
}

