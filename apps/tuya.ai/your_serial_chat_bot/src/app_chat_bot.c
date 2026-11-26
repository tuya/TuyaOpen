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
#define USER_CHAT_UART             ((TUYA_UART_NUM_E)USER_CHAT_UART_NUMBER)

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
static bool sg_agent_initialized = false;

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
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"ai reply:\r\n", strlen("ai reply:\r\n"));

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

    if (!sg_agent_initialized) {
        TUYA_CALL_ERR_RETURN(ai_agent_init());
        sg_agent_initialized = true;
        PR_NOTICE("Serial chat AI agent initialized");
    }

    return rt;
}

static void __uart_text_scan_task(void *arg)
{
    int text_len = 0;
    bool overflow = false;
    uint8_t ch = 0;

    while (1) {
        if (tal_uart_read(USER_CHAT_UART, &ch, 1) != 1) {
            tal_system_sleep(10);
            continue;
        }

        if(ch == '\r' || ch == '\n') {
            if (overflow) {
                const char *error = "error: text exceeds 512 UTF-8 bytes\r\n";
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)error, strlen(error));
                text_len = 0;
                overflow = false;
                continue;
            }
            if(text_len == 0) {
                continue;
            } else {
                sg_serial_text_buf[text_len] = '\0';
            }
        } else {
            if(text_len < TEXT_CONTENT_MAX_LEN) {
                if (!overflow) {
                    sg_serial_text_buf[text_len++] = ch;
                }
            }else {
                overflow = true;
            }
            continue;
        }

        {
            if (!sg_agent_initialized || !tuya_ai_agent_is_ready()) {
                const char *error = "error: AI session is not ready\r\n";
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)error, strlen(error));
                text_len = 0;
                continue;
            }
            OPERATE_RET result = ai_agent_send_text(sg_serial_text_buf);
            if (result != OPRT_OK) {
                PR_ERR("Serial chat send failed: %d", result);
                const char *error = "error: AI session is not ready or send failed\r\n";
                tal_uart_write(USER_CHAT_UART, (const uint8_t *)error, strlen(error));
                text_len = 0;
                continue;
            }
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"user:\r\n", strlen("user:\r\n"));
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)sg_serial_text_buf, text_len);
            tal_uart_write(USER_CHAT_UART, (const uint8_t *)"\r\n", 2);
            text_len = 0;
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
    PR_NOTICE("Serial chat UART%d ready: 115200 8N1, UTF-8, CR/LF to send", USER_CHAT_UART_NUMBER);

    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 4096;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "uart_text_task";
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_uart_scan_hdl, NULL, NULL, __uart_text_scan_task, NULL, &thrd_param));

    return OPRT_OK;
}

