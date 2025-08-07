/**
 * @file at_client.c
 * @brief at_client module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_client.h"
#include "at_line.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_CLIENT_RX_BUFFER_SIZE (5 * 1024) // Default size of the receive buffer

#define IDLE_SLEEP_MS    (100)
#define WAITING_SLEEP_MS (20)

typedef uint8_t AT_CLIENT_STATUS_T;
#define AT_CLIENT_STATUS_IDLE       0x00 // Client is idle, not processing any command
#define AT_CLIENT_STATUS_WAITING    0x01 // Client waiting for response
#define AT_CLIENT_STATUS_PROCESSING 0x02 // Client processing response
#define AT_CLIENT_STATUS_COMPLETED  0x03 // Client completed command

#define AT_CLIENT_STATUS_CHANGE(new_status)                                                                            \
    do {                                                                                                               \
        PR_DEBUG("AT client status changed: [%s] --> [%s]", AT_CLIENT_STATUS_STR[sg_at_client.status],                 \
                 AT_CLIENT_STATUS_STR[new_status]);                                                                    \
        sg_at_client.status = new_status;                                                                              \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    THREAD_HANDLE thread_hdl;
    MUTEX_HANDLE mutex;

    AT_CLIENT_STATUS_T status; // Status of the AT client

    TDL_TRANSPORT_HANDLE transport_hdl;

    char end_symbol[LINE_END_SYMBOL_MAX_LEN]; // End symbol for AT commands, e.g., "\r\n"

    SLIST_HEAD urc_head;         // List of URC handlers
    uint32_t urc_count;          // Count of URC handlers
    AT_LINE_HANDLE urc_line_hdl; // URC line handle for storing URC responses

    // List of response lines
    AT_LINE_HANDLE line_hdl;

    // Buffer for receiving data
    char *rx_buffer;
    uint32_t rx_buffer_size; // Size of the receive buffer
    uint32_t rx_buffer_used; // Amount of data currently in the receive buffer
} AT_CLIENT_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static char *__at_client_line_parse_input(AT_LINE_T **line, char *end_symbol, char *data, uint32_t len);
static OPERATE_RET __at_client_urc_process(AT_LINE_HANDLE line_hdl);
static AT_URC_T *__get_urc_handler(AT_LINE_T *line);

/***********************************************************
***********************variable define**********************
***********************************************************/
static char *AT_CLIENT_STATUS_STR[] = {
    "IDLE",
    "WAITING",
    "PROCESSING",
    "COMPLETED",
};

static AT_CLIENT_T sg_at_client = {
    .thread_hdl = NULL,
    .mutex = NULL,
    .status = AT_CLIENT_STATUS_IDLE,
    .transport_hdl = NULL,
    .end_symbol = {LINE_END_SYMBOL_CRLF},
    .urc_head = {NULL}, // Initialize the URC handler list
    .urc_count = 0,
    .urc_line_hdl = NULL, // Initialize the URC line handle
    .line_hdl = NULL,     // Initialize the line handle
    .rx_buffer = NULL,
    .rx_buffer_size = AT_CLIENT_RX_BUFFER_SIZE, // Default size of the receive buffer
    .rx_buffer_used = 0,
};
/***********************************************************
***********************function define**********************
***********************************************************/

static void __at_client_thread(void *arg)
{
    uint32_t delay_ms = 100;

    for (;;) {
        switch (sg_at_client.status) {
        case AT_CLIENT_STATUS_IDLE: {
            if (IDLE_SLEEP_MS != delay_ms) {
                delay_ms = IDLE_SLEEP_MS;
            }

            uint32_t available = tdl_transport_available(sg_at_client.transport_hdl);
            if (available > 0) {
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_WAITING);
            }
        } break;
        case AT_CLIENT_STATUS_WAITING: {
            if (WAITING_SLEEP_MS != delay_ms) {
                delay_ms = WAITING_SLEEP_MS;
            }

            uint32_t available = tdl_transport_available(sg_at_client.transport_hdl);
            uint8_t *p_rx = (uint8_t *)sg_at_client.rx_buffer + sg_at_client.rx_buffer_used;
            uint32_t free_space = sg_at_client.rx_buffer_size - sg_at_client.rx_buffer_used;
            if (available) {
                // Read data from transport
                PR_DEBUG("AT client waiting for response, available: %d bytes, free_space: %d", available, free_space);
                tal_mutex_lock(sg_at_client.mutex);
                uint32_t read_len = tdl_transport_read(sg_at_client.transport_hdl, p_rx, free_space);
                if (read_len > 0) {
                    sg_at_client.rx_buffer_used += read_len;
                    // PR_DEBUG("--> Read %d bytes from transport", read_len);
                    // PR_DEBUG("--> Received data: %.*s", read_len, p_rx);
                }
                tal_mutex_unlock(sg_at_client.mutex);
            }

            // check one of the response lines
            if (sg_at_client.rx_buffer_used > 3) {
                tal_mutex_lock(sg_at_client.mutex);
                AT_LINE_T *new_line = NULL;
                char *next_pos = __at_client_line_parse_input(&new_line, sg_at_client.end_symbol,
                                                              sg_at_client.rx_buffer, sg_at_client.rx_buffer_used);
                if (next_pos && next_pos > sg_at_client.rx_buffer && new_line) {
                    // get new line
                    // Check if it's URC
                    PR_DEBUG("---> Parsed new line: [%.*s]", new_line->len, new_line->line);
                    AT_URC_T *urc_handler = __get_urc_handler(new_line);
                    uint8_t is_urc = (urc_handler != NULL) ? 1 : 0;
                    AT_LINE_HANDLE line_hdl = is_urc ? sg_at_client.urc_line_hdl : sg_at_client.line_hdl;
                    PR_DEBUG("AT client checking response lines, is_urc: %d", is_urc);
                    // add to line handle
                    at_line_add(line_hdl, new_line);
                    // Move remaining data to the start of the buffer
                    uint32_t remaining_len = sg_at_client.rx_buffer_used - (next_pos - sg_at_client.rx_buffer);
                    memmove(sg_at_client.rx_buffer, next_pos, remaining_len);
                    sg_at_client.rx_buffer_used = remaining_len;
                    // Set 0 at the end of the used buffer
                    memset(sg_at_client.rx_buffer + sg_at_client.rx_buffer_used, 0,
                           sg_at_client.rx_buffer_size - sg_at_client.rx_buffer_used);
                } else if (next_pos && next_pos > sg_at_client.rx_buffer) {
                    // next_pos is valid but new_line is NULL (e.g., empty line or just end symbols)
                    // Still need to move the buffer to avoid data accumulation
                    PR_DEBUG("---> Skipping empty line or just end symbols");
                    uint32_t remaining_len = sg_at_client.rx_buffer_used - (next_pos - sg_at_client.rx_buffer);
                    memmove(sg_at_client.rx_buffer, next_pos, remaining_len);
                    sg_at_client.rx_buffer_used = remaining_len;
                    // Set 0 at the end of the used buffer
                    memset(sg_at_client.rx_buffer + sg_at_client.rx_buffer_used, 0,
                           sg_at_client.rx_buffer_size - sg_at_client.rx_buffer_used);
                }
                tal_mutex_unlock(sg_at_client.mutex);
            }

            if (at_line_get_count(sg_at_client.urc_line_hdl) > 0) {
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_PROCESSING);
            }
        } break;
        case AT_CLIENT_STATUS_PROCESSING: {
            // Processing response
            __at_client_urc_process(sg_at_client.urc_line_hdl);

            if (sg_at_client.rx_buffer_used > 3) {
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_WAITING);
            } else {
                AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_COMPLETED);
            }
        } break;
        case AT_CLIENT_STATUS_COMPLETED: {
            // Command completed
            PR_DEBUG("AT client command completed");
            // Reset status to idle after processing
            AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_IDLE);
        } break;
        default: {
            // Unknown status, reset to idle
            PR_ERR("Unknown AT client status: %d, resetting to IDLE", sg_at_client.status);
            AT_CLIENT_STATUS_CHANGE(AT_CLIENT_STATUS_IDLE);
        } break;
        }
        tal_system_sleep(delay_ms);
    }
}

static char *__at_client_line_parse_input(AT_LINE_T **line, char *end_symbol, char *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(line, NULL);
    TUYA_CHECK_NULL_RETURN(data, NULL);

    char *p_start = data;
    char *p_end = NULL;
    uint32_t offset = 0;

    // PR_DEBUG("Parsing input data: %.*s", len, data);

    do {
        p_end = strstr(p_start, end_symbol);
        if (p_end && p_end > p_start) {
            // at_line_add(line_hdl, p_start, p_end - p_start);
            *line = at_line_create(p_start, p_end - p_start);
            offset = p_end - data + strlen(end_symbol);
            p_start = data + offset;
            break;
        } else if (p_end == p_start) {
            p_start += strlen(end_symbol);
            offset += strlen(end_symbol);
        } else {
            break;
        }
    } while (1);

    return p_start;
}

OPERATE_RET at_client_init(TDL_TRANSPORT_HANDLE transport_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(transport_hdl, OPRT_INVALID_PARM);

    if (NULL != sg_at_client.thread_hdl) {
        PR_WARN("AT client already initialized");
        return OPRT_OK;
    }

    sg_at_client.transport_hdl = transport_hdl;

    // Initialize mutex for thread safety
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_at_client.mutex), __ERR);

    // Initialize the line handle
    TUYA_CALL_ERR_GOTO(at_line_init(&sg_at_client.line_hdl), __ERR);
    TUYA_CALL_ERR_GOTO(at_line_init(&sg_at_client.urc_line_hdl), __ERR);

    // Allocate receive buffer
    sg_at_client.rx_buffer = (char *)tal_malloc(sg_at_client.rx_buffer_size);
    if (NULL == sg_at_client.rx_buffer) {
        PR_ERR("Failed to allocate memory for AT client receive buffer");
        rt = OPRT_MALLOC_FAILED;
        goto __ERR;
    }
    memset(sg_at_client.rx_buffer, 0, sg_at_client.rx_buffer_size);
    sg_at_client.rx_buffer_used = 0;

    // Initialize the AT client status
    sg_at_client.status = AT_CLIENT_STATUS_IDLE;

    // Initialize thread handle
    THREAD_CFG_T thrd_param = {4 * 1024, THREAD_PRIO_1, "at_client"};
    TUYA_CALL_ERR_GOTO(
        tal_thread_create_and_start(&sg_at_client.thread_hdl, NULL, NULL, __at_client_thread, NULL, &thrd_param),
        __ERR);

    PR_DEBUG("AT client initialized successfully");

    return rt;

__ERR:
    at_client_deinit();

    return rt;
}

OPERATE_RET at_client_deinit(void)
{
    if (NULL == sg_at_client.mutex) {
        PR_WARN(" AT client mutex is NULL, maybe not init");
        return OPRT_OK;
    }

    tal_mutex_lock(sg_at_client.mutex);
    if (NULL != sg_at_client.thread_hdl) {
        tal_thread_delete(sg_at_client.thread_hdl);
        sg_at_client.thread_hdl = NULL;
    }

    if (NULL != sg_at_client.line_hdl) {
        at_line_deinit(sg_at_client.line_hdl);
        sg_at_client.line_hdl = NULL;
    }

    if (NULL != sg_at_client.urc_line_hdl) {
        at_line_deinit(sg_at_client.urc_line_hdl);
        sg_at_client.urc_line_hdl = NULL;
    }

    if (NULL != sg_at_client.rx_buffer) {
        tal_free(sg_at_client.rx_buffer);
        sg_at_client.rx_buffer = NULL;
    }

    // TODO: free urc list

    tal_mutex_unlock(sg_at_client.mutex);

    tal_mutex_release(sg_at_client.mutex);
    sg_at_client.mutex = NULL;

    return OPRT_OK;
}

OPERATE_RET at_client_send(const char *cmd, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(sg_at_client.thread_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cmd, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_at_client.mutex);
    tdl_transport_send(sg_at_client.transport_hdl, (const uint8_t *)cmd, len);
    tal_mutex_unlock(sg_at_client.mutex);

    return rt;
}

AT_LINE_T *at_client_get_response(uint32_t timeout_ms)
{
    AT_LINE_T *line = NULL;

    TUYA_CHECK_NULL_RETURN(sg_at_client.thread_hdl, NULL);
    TUYA_CHECK_NULL_RETURN(sg_at_client.line_hdl, NULL);

    // Wait for response with timeout
    uint32_t delay_cnt = 0;
    do {
        if (at_line_get_count(sg_at_client.line_hdl) > 0) {
            break;
        }
        tal_system_sleep(10);
        delay_cnt++;
    } while (delay_cnt * 10 < timeout_ms);

    if (delay_cnt * 10 >= timeout_ms) {
        PR_WARN("No response received within timeout: %d ms", timeout_ms);
        return NULL;
    }

    // Get the response line
    tal_mutex_lock(sg_at_client.mutex);
    line = at_line_get(sg_at_client.line_hdl);
    tal_mutex_unlock(sg_at_client.mutex);

    return line;
}

void at_client_response_free(AT_LINE_T *line)
{
    at_line_free(line);
    return;
}

OPERATE_RET at_client_send_with_rsp(const char *cmd, uint32_t len, AT_LINE_T **rsp_line, uint32_t timeout_ms)
{
    OPERATE_RET rt = OPRT_OK;

    // Send the command
    TUYA_CALL_ERR_RETURN(at_client_send(cmd, len));

    // Wait for response
    *rsp_line = at_client_get_response(timeout_ms);

    if (NULL == *rsp_line) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

OPERATE_RET at_client_urc_handler_register(AT_URC_T *urc_handler)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(sg_at_client.thread_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(urc_handler, OPRT_INVALID_PARM);

    tal_mutex_lock(sg_at_client.mutex);
    tuya_init_slist_node(&urc_handler->node);
    tuya_slist_add_head(&sg_at_client.urc_head, &urc_handler->node);
    sg_at_client.urc_count++;
    tal_mutex_unlock(sg_at_client.mutex);

    return rt;
}

static AT_URC_T *__get_urc_handler(AT_LINE_T *line)
{
    TUYA_CHECK_NULL_RETURN(line, 0);

    char *p_start = line->line;
    uint32_t len = line->len;

    // PR_DEBUG("Checking if line is URC: %.*s", len, p_start);

    SLIST_HEAD *pos = NULL;
    for (pos = sg_at_client.urc_head.next; pos != NULL; pos = pos->next) {
        AT_URC_T *urc_handler = (AT_URC_T *)pos;

        // PR_DEBUG("Checking URC handler: prefix: %s, suffix: %s", urc_handler->prefix, urc_handler->suffix);
        if (urc_handler->prefix) {
            if (strncmp(p_start, urc_handler->prefix, strlen(urc_handler->prefix)) == 0) {
                return urc_handler;
            }
        }
        if (urc_handler->suffix) {
            if (len >= strlen(urc_handler->suffix) && strncmp(p_start + len - strlen(urc_handler->suffix),
                                                              urc_handler->suffix, strlen(urc_handler->suffix)) == 0) {
                return urc_handler;
            }
        }
    }

    return 0; // No URC match found
}

static OPERATE_RET __at_client_urc_process(AT_LINE_HANDLE line_hdl)
{
    PR_DEBUG("Processing URC lines");

    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(line_hdl, OPRT_INVALID_PARM);

    if (at_line_get_count(line_hdl) == 0) {
        PR_DEBUG("No URC lines to process");
        return OPRT_OK; // No URC lines to process
    }

    // Process each URC line
    AT_LINE_T *line = NULL;
    while (at_line_get_count(line_hdl)) {
        line = at_line_get(line_hdl);
        if (NULL == line) {
            PR_DEBUG("No URC line to process");
            break;
        }
        // PR_DEBUG("Processing URC line: %d [%.*s]", line->len, line->len, line->line);
#if 0
        SLIST_HEAD *pos = NULL;
        for (pos = sg_at_client.urc_head.next; pos != NULL; pos = pos->next) {
            AT_URC_T *urc_handler = (AT_URC_T *)pos;
            if (urc_handler->prefix) {
                if (strncmp(line->line, urc_handler->prefix, strlen(urc_handler->prefix)) == 0) {
                    if (urc_handler->urc_handler) {
                        urc_handler->urc_handler(line->line, line->len);
                    }
                    break;
                }
            }
            if (urc_handler->suffix) {
                if (line->len >= strlen(urc_handler->suffix) &&
                    strncmp(line->line + line->len - strlen(urc_handler->suffix), urc_handler->suffix,
                            strlen(urc_handler->suffix)) == 0) {
                    if (urc_handler->urc_handler) {
                        urc_handler->urc_handler(line->line, line->len);
                    }
                    break;
                }
            }
        }
#else
        AT_URC_T *urc_handler = __get_urc_handler(line);
        PR_DEBUG("URC handler found: %s", urc_handler->prefix ? urc_handler->prefix : "NULL");
        if (urc_handler && urc_handler->urc_handler) {
            // PR_DEBUG("Invoking URC handler for line: %.*s", line->len, line->line);
            urc_handler->urc_handler(line->line, line->len);
        } else {
            PR_DEBUG("No URC handler found for line: %.*s", line->len, line->line);
        }
#endif
        at_line_free(line); // Free the URC line after processing
        line = NULL;        // Reset line pointer to avoid dangling pointer
    }

    return rt;
}
