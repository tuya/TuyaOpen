/**
 * @file at_module_ml307r.c
 * @brief at_module_ml307r module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_module_ml307r.h"

#include "at_client.h"
#include "at_utils.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_SEND_TIMEOUT_MS (10 * 1000) // Timeout for interaction with 4G module

// send
#define AT         "AT\r"
#define AT_MIPOPEN "AT+MIPOPEN"
#define AT_MIPCFG  "AT+MIPCFG"
#define AT_MDNSGIP "AT+MDNSGIP"

// rsp
#define OK        "OK"
#define CME_ERROR "+CME ERROR: "

// PROTOCOL_TCP = 0,
// PROTOCOL_UDP = 1,
#define GET_PROTOCOL_TYPE(type) ((type) == PROTOCOL_TCP ? "TCP" : "UDP")

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_IP_ADDR_T dns_ip;
    AT_MODEM_CB urc_cb;

    MUTEX_HANDLE mutex;
} AT_MODULE_CONTEXT_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static int __parse_urc_tokens(char *data, const char *cmd_prefix, const char *delimiters, char **out_buffer,
                              char **token_array, int max_tokens);

static void __urc_matready_cb(char *data, uint32_t len);
static void __urc_cereg_cb(char *data, uint32_t len);
static void __urc_cpin_cb(char *data, uint32_t len);
static void __urc_mipopen_cb(char *data, uint32_t len);
static void __urc_mipclose_cb(char *data, uint32_t len);
static void __urc_mdnsgip_cb(char *data, uint32_t len);
static void __urc_mipurc_cb(char *data, uint32_t len);

/***********************************************************
***********************variable define**********************
***********************************************************/
static AT_MODULE_CONTEXT_T sg_ml307r_ctx = {0};

static AT_URC_T sg_ml307r_urc_handler[] = {
    {{NULL}, "+MATREADY", NULL, __urc_matready_cb},   {{NULL}, "+CEREG: ", NULL, __urc_cereg_cb},
    {{NULL}, "+CPIN: ", NULL, __urc_cpin_cb},         {{NULL}, "+MIPOPEN: ", NULL, __urc_mipopen_cb},
    {{NULL}, "+MIPCLOSE: ", NULL, __urc_mipclose_cb}, {{NULL}, "+MDNSGIP: ", NULL, __urc_mdnsgip_cb},
    {{NULL}, "+MIPURC: ", NULL, __urc_mipurc_cb},
};
/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Parse URC command tokens in format: "+CMD: token1,token2,..."
 * @param data Source data string to parse (will be modified during parsing)
 * @param cmd_prefix Expected command prefix (e.g., "+MIPOPEN")
 * @param delimiters String containing delimiter characters (e.g., ": ," or ": ,\"")
 * @param out_buffer Pointer to store allocated buffer (caller should free this)
 * @param token_array Array to store pointers to parsed token strings
 * @param max_tokens Maximum number of tokens to parse
 * @return Number of tokens parsed successfully, -1 on error
 *
 * Note: The token_array will contain pointers to strings within out_buffer.
 *       Caller should only free out_buffer, not individual token strings.
 */
static int __parse_urc_tokens(char *data, const char *cmd_prefix, const char *delimiters, char **out_buffer,
                              char **token_array, int max_tokens)
{
    if (!data || !cmd_prefix || !delimiters || !out_buffer || !token_array || max_tokens <= 0) {
        return -1;
    }

    // Allocate buffer for strtok processing
    size_t len = strlen(data);
    char *buffer = tal_malloc(len + 1);
    if (!buffer) {
        PR_ERR("malloc failed for URC parsing");
        return -1;
    }
    memset(buffer, 0, len + 1);
    strcpy(buffer, data);
    *out_buffer = buffer;

    // Parse command prefix
    char *token = strtok(buffer, delimiters);
    if (!token || strncmp(token, cmd_prefix, strlen(cmd_prefix)) != 0) {
        PR_ERR("Invalid URC format, expected: %s", cmd_prefix);
        tal_free(buffer);
        *out_buffer = NULL;
        return -1;
    }

    // Parse tokens and store pointers
    int parsed_count = 0;
    for (int i = 0; i < max_tokens; i++) {
        token = strtok(NULL, delimiters);
        if (!token) {
            if (i == 0) {
                PR_ERR("Invalid URC format, missing tokens after %s", cmd_prefix);
                tal_free(buffer);
                *out_buffer = NULL;
                return -1;
            }
            break; // End of tokens
        }
        token_array[i] = token; // Store pointer to token string
        parsed_count++;
    }

    return parsed_count;
}

static void __urc_matready_cb(char *data, uint32_t len)
{
    // PR_DEBUG("URC +MATREADY received: %.*s", len, data);
    // Maybe ML307R module is rebooting

    uint8_t at_ready = 0;

    if (strncmp("+MATREADY", data, len) == 0) {
        at_ready = 1;
    }

    if (sg_ml307r_ctx.urc_cb) {
        sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_READY, &at_ready);
    }

    return;
}

static void __urc_cereg_cb(char *data, uint32_t len)
{
    uint8_t net_ready = 0;

    char *buffer = NULL;
    char *tokens[7] = {NULL};

    PR_DEBUG("URC +CEREG received: %.*s", len, data);
    // +CEREG: 0
    // +CEREG: 5,"58BC","0C03A143",7
    // +CEREG: <n>,<stat>[,<lac>,<ci>]

    int parsed = __parse_urc_tokens(data, "+CEREG", ": ,", &buffer, tokens, 7);
    if (parsed == 0) {
        if (buffer) {
            tal_free(buffer);
        }
        return;
    }

    PR_DEBUG("Parsed %d tokens from +CEREG", parsed);
    for (int i = 0; i < parsed; i++) {
        PR_DEBUG("Token %d: %s", i, tokens[i]);
    }

    int n = atoi(tokens[0]);
    // if (n == 0 && parsed)
    if (n >= 0 && n <= 3 && parsed > 1) {
        int stat = atoi(tokens[1]);
        PR_DEBUG("CEREG stat: %d", stat);
        if (stat <= 5 && stat != 2) {
            net_ready = 1;
        } else {
            net_ready = 0;
        }
    } else if (n == 4 || n == 5) {
        net_ready = 1;
    } else {
        net_ready = 0;
    }

    if (sg_ml307r_ctx.urc_cb) {
        sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_NETWORK, &net_ready);
    }

    return;
}

static void __urc_cpin_cb(char *data, uint32_t len)
{
    // PR_DEBUG("URC +CPIN received: %.*s", len, data);

    // +CPIN: READY
    uint8_t cpin_ready = 0;

    if (strncmp("+CPIN: READY", data, len) == 0) {
        cpin_ready = 1;
    }

    if (sg_ml307r_ctx.urc_cb) {
        sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_SIM, &cpin_ready);
    }

    return;
}

static void __urc_mipopen_cb(char *data, uint32_t len)
{
    PR_TRACE("%s received: %.*s", __func__, len, data);

    // +MIPOPEN: <fd>,<result>
    AT_CONNECT_STATUS_T connect_status = {0};

    char *buffer = NULL;
    char *tokens[2] = {NULL};
    int parsed = __parse_urc_tokens(data, "+MIPOPEN", ": ,", &buffer, tokens, 2);
    if (parsed != 2) {
        if (buffer) {
            tal_free(buffer);
            buffer = NULL;
        }
        return;
    }

    connect_status.fd = atoi(tokens[0]);
    connect_status.result = atoi(tokens[1]);

    PR_TRACE("URC +MIPOPEN parsed: fd=%d, result=%d", connect_status.fd, connect_status.result);

    if (sg_ml307r_ctx.urc_cb) {
        sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_CONNECT_STATUS, &connect_status);
    }

    // Free the allocated buffer only, not the tokens
    tal_free(buffer);
    buffer = NULL;

    return;
}
static void __urc_mipclose_cb(char *data, uint32_t len)
{
    PR_TRACE("%s received: %.*s", __func__, len, data);
    // TODO:
    return;
}

static void __urc_mdnsgip_cb(char *data, uint32_t len)
{
    PR_TRACE("%s received: %.*s", __func__, len, data);

    // +MDNSGIP: "<domainname>","<ip1>","<ip2>",...,"<ipn>"

    // Count how many IP addresses are returned by searching for ','
    int ip_count = 0;
    char *token = strchr(data, ',');
    while (token) {
        ip_count++;
        token = strchr(token + 1, ',');
    }

    PR_TRACE("MDNSGIP IP count: %d", ip_count);

    if (ip_count == 0) {
        PR_ERR("No IP addresses found in URC +MDNSGIP");
        return;
    }

    char *buffer = NULL;
    char *tokens[4] = {NULL};
    int parsed = __parse_urc_tokens(data, "+MDNSGIP:", ",", &buffer, tokens, 4);
    // PR_DEBUG("Parsed %d tokens from +MDNSGIP", parsed);
    // for (int i = 0; i < parsed; i++) {
    //     PR_DEBUG("Token %d: %s", i, tokens[i]);
    // }
    if (parsed == 0) {
        PR_ERR("Failed to parse URC +MDNSGIP, parsed count: %d", parsed);
        if (buffer) {
            tal_free(buffer);
        }
        return;
    }

    char *ip_str = NULL;

    for (int i = 0; i < parsed; i++) {
        ip_str = tokens[i];
        if (at_utils_is_ipv4(ip_str)) {
            // Convert IP string to address
            TUYA_IP_ADDR_T ip_addr = at_utils_str2addr(ip_str);
            if (ip_addr != -1) {
                PR_DEBUG("Parsed IP address: %s -> 0x%08X", ip_str, ip_addr);
                // sg_dns_addr = ip_addr; // Store the last valid IP address
                sg_ml307r_ctx.dns_ip = ip_addr;
                break; // Use the first valid IP address
            } else {
                PR_ERR("Invalid IP address format: %s", ip_str);
            }
        }
    }

    tal_free(buffer);

    return;
}

static void __urc_mipurc_cb(char *data, uint32_t len)
{
    // PR_DEBUG("%s received: len: %d, data: %.*s", __func__, len, len * 2, data);

    // +MIPURC: "disconn",<fd>,<connect_state>
    // +MIPURC: "rtcp",<fd>,<recv_len>,<recv_data>

    char *buffer = NULL;
    char *tokens[4] = {NULL};
    int parsed = __parse_urc_tokens(data, "+MIPURC", ": ,\"", &buffer, tokens, 4);
    if (parsed < 2) {
        if (buffer) {
            tal_free(buffer);
        }
        PR_ERR("Failed to parse MIPURC command, parsed count: %d", parsed);
        return;
    }

    char *type = tokens[0];
    // int fd = -1;

    if (!type) {
        PR_ERR("MIPURC type is NULL");
        if (buffer) {
            tal_free(buffer);
        }
        return;
    }

    if (strcmp(type, "disconn") == 0) {
        // int connect_state = -1;
        AT_CONNECT_STATUS_T socket_conn = {0};

        if (parsed >= 2 && tokens[1]) {
            socket_conn.fd = atoi(tokens[1]);
        }
        // if (parsed >= 3 && tokens[2]) {
        //     // Disconnect reason
        // }
        socket_conn.result = 1;
        if (sg_ml307r_ctx.urc_cb) {
            sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_CONNECT_STATUS, &socket_conn);
        }
        PR_DEBUG("URC +MIPURC disconn: fd=%d, connect_state=%d", socket_conn.fd, socket_conn.result);
    } else if (strcmp(type, "rtcp") == 0) {
        AT_SOCKET_RECV_T socket_recv = {0};
        if (parsed >= 2 && tokens[1]) {
            socket_recv.fd = (uint32_t)(atoi(tokens[1]));
        }
        if (parsed >= 3 && tokens[2]) {
            socket_recv.len = atoi(tokens[2]);
        }
        if (parsed >= 4 && tokens[3]) {
            char *byte_data = tal_malloc(socket_recv.len);
            if (!byte_data) {
                PR_ERR("Failed to allocate memory for socket recv data");
                tal_free(buffer);
                return;
            }
            memset(byte_data, 0, socket_recv.len);

            at_utils_hex_char_to_byte(tokens[3], socket_recv.len * 2, (uint8_t *)byte_data, socket_recv.len);
            socket_recv.data = byte_data;
            if (sg_ml307r_ctx.urc_cb) {
                sg_ml307r_ctx.urc_cb(TAL_AT_MODEM_CMD_SOCKET_RECV, &socket_recv);
            }
            tal_free(byte_data);
        }
    } else {
        PR_ERR("Unknown URC type: %s", type);
    }

    if (buffer) {
        tal_free(buffer);
    }

    return;
}

static OPERATE_RET __ml307r_urc_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint32_t urc_count = sizeof(sg_ml307r_urc_handler) / sizeof(AT_URC_T);

    for (uint32_t i = 0; i < urc_count; i++) {
        TUYA_CALL_ERR_RETURN(at_client_urc_handler_register(&sg_ml307r_urc_handler[i]));
    }

    PR_DEBUG("Registering ML307R URC handler successfully");

    return rt;
}

static uint8_t __ml307r_at_check(void)
{
    uint8_t at_ready = 1;
    OPERATE_RET rt = OPRT_OK;
    AT_LINE_T *rsp_line = NULL;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return 0;
    }
    tal_mutex_lock(sg_ml307r_ctx.mutex);

    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(AT, strlen(AT), &rsp_line, AT_SEND_TIMEOUT_MS));
    if (OPRT_OK != rt || NULL == rsp_line) {
        at_ready = 0;
        goto __EXIT;
    }

    if (strncmp(rsp_line->line, OK, strlen(OK)) != 0) {
        PR_ERR("AT check failed, response: %s", rsp_line->line);
        at_ready = 0;
        goto __EXIT;
    }

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    return at_ready;
}

static OPERATE_RET __ml307r_get_cereg_status(void)
{
    OPERATE_RET rt = OPRT_OK;

    // AT+CEREG?
    char *send_buf = "AT+CEREG?\r";
    AT_LINE_T *rsp_line = NULL;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(sg_ml307r_ctx.mutex);

    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(send_buf, strlen(send_buf), &rsp_line, AT_SEND_TIMEOUT_MS));
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+CEREG? failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // +CME ERROR: <error_code>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+CEREG? failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // OK
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+CEREG? failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

__EXIT:

    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    return rt;
}

static uint8_t __ml307r_at_get_socket_num_max(void)
{
    return 6;
}

static TUYA_ERRNO __ml307r_at_connect(int fd, TUYA_PROTOCOL_TYPE_E type, const char *ip, uint16_t port,
                                      uint32_t timeout_ms)
{
    TUYA_ERRNO rt_erron = UNW_SUCCESS;
    OPERATE_RET rt = OPRT_OK;
    char tmp_buf[64] = {0};
    AT_LINE_T *rsp_line = NULL;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return 0;
    }
    tal_mutex_lock(sg_ml307r_ctx.mutex);

    // open socket
    // "AT+MIPOPEN=fd,\"TCP\",\"<addr>\",<port>,timeout\r";
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPOPEN=%d,\"%s\",\"%s\",%d,%d\r", fd, GET_PROTOCOL_TYPE(type), ip, port,
             timeout_ms);
    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(tmp_buf, strlen(tmp_buf), &rsp_line, AT_SEND_TIMEOUT_MS));
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+MIPOPEN failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }

    // +CME ERROR: <error_code>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+MIPSEND failed, response: %s", rsp_line->line);
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }

    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+MIPOPEN failed, response: %s", rsp_line->line);
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;

    // AT+MIPCFG="autofree",<connect_id>,<free_mode>
    // free_mode: 0: Automatically release after disconnection; 1: Do not release after disconnection, manual release
    // required
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCFG=\"autofree\",%d,0\r", fd);
    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(tmp_buf, strlen(tmp_buf), &rsp_line, 50 * 1000));
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+MIPCFG failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+MIPCFG failed, response: %s", rsp_line->line);
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;
    PR_DEBUG("AT+MIPCFG autofree set successfully");

    // encode hex
    // AT+MIPCFG="encoding",<connect_id>,<send_format>,<recv_format>
    // send_format: 0: ASCII, 1: HEX, 2: Escaped string
    memset(tmp_buf, 0, sizeof(tmp_buf));
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCFG=\"encoding\",%d,1,1\r", fd);
    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(tmp_buf, strlen(tmp_buf), &rsp_line, AT_SEND_TIMEOUT_MS));
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+MIPCFG encoding failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+MIPCFG encoding failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt_erron = UNW_FAIL;
        goto __EXIT;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;
    PR_DEBUG("AT+MIPCFG encoding set successfully");

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    return rt_erron;
}

static OPERATE_RET __ml307r_at_gethostbyname(const char *domain, TUYA_IP_ADDR_T *addr)
{
    OPERATE_RET rt = OPRT_OK;

    // AT+MDNSGIP="www.xxxxxx.com"
    char *send_buf = NULL;
    AT_LINE_T *rsp_line = NULL;
    uint32_t send_len = strlen(domain) + 16;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(sg_ml307r_ctx.mutex);

    send_buf = tal_malloc(send_len);
    TUYA_CHECK_NULL_RETURN(send_buf, OPRT_MALLOC_FAILED);
    memset(send_buf, 0, send_len);
    snprintf(send_buf, send_len, "AT+MDNSGIP=\"%s\"\r", domain);

    TUYA_CALL_ERR_LOG(at_client_send_with_rsp(send_buf, strlen(send_buf), &rsp_line, AT_SEND_TIMEOUT_MS));
    tal_free(send_buf);
    send_buf = NULL;
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+MDNSGIP failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // +CME ERROR: <error_code>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+MDNSGIP failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+MDNSGIP failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // wait for URC +MDNSGIP:
    uint32_t cnt = 0;
    do {
        tal_system_sleep(100);
        cnt++;

        if (cnt * 100 > 50 * 1000) {
            PR_ERR("Get DNS timeout");
            rt = OPRT_TIMEOUT;
            break;
        }
    } while (sg_ml307r_ctx.dns_ip == 0);

    if (OPRT_OK == rt && addr) {
        *addr = sg_ml307r_ctx.dns_ip;
    }
    sg_ml307r_ctx.dns_ip = 0;

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (send_buf) {
        tal_free(send_buf);
        send_buf = NULL;
    }

    if (rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    return rt;
}

static TUYA_ERRNO __ml307r_at_send(const int fd, const void *buf, const uint32_t nbytes)
{
    TUYA_ERRNO rt_errno = UNW_FAIL;
    OPERATE_RET rt = OPRT_OK;
    char *buffer = NULL;
    AT_LINE_T *rsp_line = NULL;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return 0;
    }

    tal_mutex_lock(sg_ml307r_ctx.mutex);

    // AT+MIPSEND=fd,len,"<data>"

    uint32_t offset = 0;
    uint32_t send_len = nbytes * 2 + 64;
    char *send_cmd = tal_malloc(send_len);
    if (send_cmd == NULL) {
        PR_ERR("malloc for send command failed");
        return UNW_FAIL;
    }
    memset(send_cmd, 0, send_len);

    offset += snprintf(send_cmd + offset, send_len - offset, "AT+MIPSEND=%d,%d,\"", fd, nbytes);
    for (uint32_t i = 0; i < nbytes; i++) {
        offset += snprintf(send_cmd + offset, send_len - offset, "%02X", ((uint8_t *)buf)[i]);
    }
    offset += snprintf(send_cmd + offset, send_len - offset, "\"\r");

    rt = at_client_send(send_cmd, offset);
    if (OPRT_OK != rt) {
        PR_ERR("AT+MIPSEND failed, response: %s", send_cmd);
        rt_errno = UNW_FAIL;
        goto __EXIT;
    }
    tal_free(send_cmd);
    send_cmd = NULL;

    // +MIPSEND: fd,len
    rsp_line = at_client_get_response(AT_SEND_TIMEOUT_MS);
    if (rsp_line == NULL) {
        PR_ERR("AT+MIPSEND response timeout");
        rt_errno = UNW_FAIL;
        goto __EXIT;
    }

    // +CME ERROR: <error_code>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+MIPSEND failed, response: %s", rsp_line->line);
        rt_errno = UNW_FAIL; // TODO: improve error handling
        goto __EXIT;
    }

    // +MIPSEND: <fd>,<send_length>
    char *tokens[2] = {NULL};
    int parsed = __parse_urc_tokens(rsp_line->line, "+MIPSEND", ": ,", &buffer, tokens, 2);
    if (parsed < 2 || tokens[0] == NULL || tokens[1] == NULL) {
        PR_ERR("AT+MIPSEND response error, expected: MIPSEND: <fd>,<send_length>, response: %.*s", rsp_line->len,
               rsp_line->line);
        rt_errno = UNW_FAIL;
        goto __EXIT;
    }
    if (fd != atoi(tokens[0])) {
        PR_ERR("AT+MIPSEND response fd mismatch, expected: %d, got: %s", fd, tokens[0]);
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;

    rt_errno = atoi(tokens[1]);
    PR_DEBUG("response len: %d", rt_errno);

    tal_free(buffer);
    buffer = NULL;

    // OK
    rsp_line = at_client_get_response(AT_SEND_TIMEOUT_MS);
    if (rsp_line == NULL) {
        PR_ERR("AT+MIPSEND response timeout");
        rt_errno = UNW_FAIL;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        rt_errno = UNW_FAIL;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (NULL != send_cmd) {
        tal_free(send_cmd);
        send_cmd = NULL;
    }

    if (NULL != rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    if (NULL != buffer) {
        tal_free(buffer);
        buffer = NULL;
    }

    return rt_errno;
}

TUYA_ERRNO __ml307r_at_close(const int fd)
{
    TUYA_ERRNO rt_errno = UNW_SUCCESS;

    // AT+MIPCLOSE=<fd>
    char tmp_buf[32] = {0};
    AT_LINE_T *rsp_line = NULL;

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return 0;
    }

    tal_mutex_lock(sg_ml307r_ctx.mutex);

    snprintf(tmp_buf, sizeof(tmp_buf), "AT+MIPCLOSE=%d\r", fd);
    OPERATE_RET rt = at_client_send_with_rsp(tmp_buf, strlen(tmp_buf), &rsp_line, AT_SEND_TIMEOUT_MS);
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+MIPCLOSE failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt_errno = UNW_FAIL;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+MIPCLOSE failed, response: %s", rsp_line->line);
        rt_errno = UNW_FAIL;
    }

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (NULL != rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    return rt_errno;
}

OPERATE_RET at_module_ml307r_get_at_modem_ip(NW_IP_S *ip)
{
    OPERATE_RET rt = OPRT_OK;
    char *buffer = NULL;
    AT_LINE_T *rsp_line = NULL;

    if (NULL == ip) {
        PR_ERR("Invalid parameter: ip is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL == sg_ml307r_ctx.mutex) {
        PR_ERR("ML307R context mutex is NULL");
        return 0;
    }

    tal_mutex_lock(sg_ml307r_ctx.mutex);

    // AT+CGDCONT?
    char *send_buf = "AT+CGDCONT?\r";
    rt = at_client_send_with_rsp(send_buf, strlen(send_buf), &rsp_line, AT_SEND_TIMEOUT_MS);
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+CGDCONT? failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }
    // +CME ERROR: <err>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+CGDCONT? failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // +CGDCONT: 1,"IPV4V6","CMIOT",,0,0,,,,
    char *tokens[5] = {NULL};
    int parsed = __parse_urc_tokens(rsp_line->line, "+CGDCONT", ": ,", &buffer, tokens, 5);
    if (parsed < 2 || tokens[1] == NULL || tokens[2] == NULL) {
        PR_ERR("AT+CGDCONT? response format error, response: %.*s", rsp_line->len, rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }
    int cid = atoi(tokens[0]); // CID
    at_client_response_free(rsp_line);
    rsp_line = NULL;
    tal_free(buffer);
    buffer = NULL;

    // OK
    rsp_line = at_client_get_response(AT_SEND_TIMEOUT_MS);
    if (rsp_line == NULL) {
        PR_ERR("AT+CGDCONT? response timeout");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+CGDCONT? failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;

    // Get IP address
    // AT+CGPADDR=<cid>
    char tmp_buf[32] = {0};
    snprintf(tmp_buf, sizeof(tmp_buf), "AT+CGPADDR=%d\r", cid);
    rt = at_client_send_with_rsp(tmp_buf, strlen(tmp_buf), &rsp_line, AT_SEND_TIMEOUT_MS);
    if (OPRT_OK != rt || NULL == rsp_line) {
        PR_ERR("AT+CGPADDR failed, response: %s", rsp_line ? rsp_line->line : "NULL");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // +CME ERROR: <err>
    if (strncmp(rsp_line->line, "+CME ERROR:", 11) == 0) {
        PR_ERR("AT+CGPADDR failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // +CGPADDR: 1,"10.112.152.218","2409:8D28:248:B226:1859:274B:5503:CFE"
    tokens[0] = NULL;
    parsed = __parse_urc_tokens(rsp_line->line, "+CGPADDR", ",", &buffer, tokens, 5);
    if (parsed < 2 || tokens[1] == NULL) {
        PR_ERR("AT+CGPADDR response format error, response: %.*s", rsp_line->len, rsp_line->line);
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    // PR_DEBUG("parsed: %d", parsed);
    // for (int i = 0; i < parsed; i++) {
    //     PR_DEBUG("Token %d: %s", i, tokens[i]);
    // }

    char *ip_str = NULL;
    for (int i = 0; i < parsed; i++) {
        if (at_utils_is_ipv4(tokens[i])) {
            ip_str = tokens[i];
            break; // Use the first valid IPv4 address
        }
    }
    if (ip_str != NULL) {
        strncpy(ip->ip, ip_str, sizeof(ip->ip) - 1);
    } else {
        PR_ERR("No valid IPv4 address found in AT+CGPADDR response");
        rt = OPRT_COM_ERROR;
    }
    at_client_response_free(rsp_line);
    rsp_line = NULL;
    tal_free(buffer);
    buffer = NULL;

    // OK
    rsp_line = at_client_get_response(AT_SEND_TIMEOUT_MS);
    if (rsp_line == NULL) {
        PR_ERR("AT+CGPADDR response timeout");
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }
    if (strcmp(OK, rsp_line->line) != 0) {
        PR_ERR("AT+CGPADDR failed, response: %s", rsp_line->line);
        rt = OPRT_COM_ERROR;
    }

__EXIT:
    tal_mutex_unlock(sg_ml307r_ctx.mutex);

    if (rsp_line) {
        at_client_response_free(rsp_line);
        rsp_line = NULL;
    }

    if (buffer) {
        tal_free(buffer);
        buffer = NULL;
    }

    return rt;
}

OPERATE_RET at_module_ml307r_init(AT_MODULE_OPS_T *ops, AT_MODEM_CB urc_cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(ops, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(urc_cb, OPRT_INVALID_PARM);

    if (sg_ml307r_ctx.mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_ml307r_ctx.mutex));
    }

    sg_ml307r_ctx.urc_cb = urc_cb;

    ops->at_check = __ml307r_at_check;
    ops->at_get_cereg_status = __ml307r_get_cereg_status;
    ops->at_get_socket_num_max = __ml307r_at_get_socket_num_max;
    ops->at_connect = __ml307r_at_connect;
    ops->at_gethostbyname = __ml307r_at_gethostbyname;
    ops->at_send = __ml307r_at_send;
    ops->at_close = __ml307r_at_close;
    ops->get_at_modem_ip = at_module_ml307r_get_at_modem_ip;

    // register urc handler
    TUYA_CALL_ERR_RETURN(__ml307r_urc_register());

    PR_DEBUG("ML307R module initialized successfully");

    return rt;
}
