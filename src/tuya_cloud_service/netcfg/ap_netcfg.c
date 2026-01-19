/**
 * @file ap_netcfg.c
 * @brief This file contains the implementation of the AP (Access Point) network
 * configuration process. It includes functions for initializing the AP network
 * configuration, handling device configuration, broadcasting network
 * configuration, parsing configuration commands, and setting up TLS for secure
 *        communication.
 *
 *        The AP network configuration process allows devices to connect to a
 * local network via an Access Point, receive configuration parameters, and
 * establish a secure communication channel with the server.
 *
 *        This file is part of the Tuya IoT SDK and is used to facilitate device
 * network configuration and communication security.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "ap_netcfg.h"
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_protocol.h"
#include "tal_wifi.h"
#include "tal_network.h"
#include "tuya_iot.h"
#include "tuya_register_center.h"

#define AP_BROADCAST_PORT       6667
#define AP_TLS_PSK_PORT         6668 // AP tls + psk
#define AP_TLS_PSK_PINCODE_PORT 7001 // AP tls + psk(pincode)

#ifndef WIFI_AP_BUF_SIZE
#define AP_MAX_BUFSIZE 4096
#else
#define AP_MAX_BUFSIZE WIFI_AP_BUF_SIZE
#endif

#define AP_TLS_PSK_LEN  (37)
#define AP_MAX_STA_CONN (1)

#define AP_CFG_EXT_CMD 0x01E

typedef struct {
    THREAD_HANDLE thread;
    BOOL_T thread_exit_flag;

    uint8_t recv_buf[AP_MAX_BUFSIZE];

    netcfg_args_t netcfg_args;
    netcfg_info_t netcfg_info;
    netcfg_finish_cb_t netcfg_finish_cb;

    TUYA_IP_ADDR_T serv_ip;

    int psk_fd;
    int client_fd;
    int broadcast_fd;
    bool is_psk_pincode;

    tuya_tls_hander tls_hander;
    uint8_t app_key[APP_KEY_LEN];
    uint8_t tls_psk[AP_TLS_PSK_LEN + 1];

    TIMER_ID broadcast_timer;
} ap_netcfg_t;

static ap_netcfg_t *s_ap_netcfg = NULL;

int ap_pbkdf2_cacl(char *pin, char *uuid, uint8_t *buf, uint8_t buflen);

ap_netcfg_t *ap_netcfg_get(void)
{
    return s_ap_netcfg;
}

void ap_netcfg_free(void)
{
    if (s_ap_netcfg) {
        tal_free(s_ap_netcfg);
        s_ap_netcfg = NULL;
    }
}

static void ap_app_key_make(uint8_t *app_key)
{
    uint8_t app_key_encode[APP_KEY_LEN] = {0};

    app_key[0] = 'O';
    app_key[1] = 'X';
    app_key[2] = 'L';
    app_key[3] = 'v';
    app_key[4] = 's';
    app_key[5] = 'l';
    app_key[6] = 'C';
    app_key[7] = 'v';
    app_key[8] = 'U';
    app_key[9] = 'x';
    app_key[10] = 'c';
    app_key[11] = 'T';
    app_key[12] = 'P';
    app_key[13] = 'L';
    app_key[14] = 'G';
    app_key[15] = 'O';

    tal_md5_ret(app_key, APP_KEY_LEN, app_key_encode);
    memcpy(app_key, app_key_encode, APP_KEY_LEN);
}

static int ap_dev_config_make(ap_netcfg_t *ap, char **buf)
{
    int op_ret = OPRT_OK;

    NW_IP_S ip;
    memset(&ip, 0, sizeof(ip));

    op_ret = tal_wifi_get_ip(WF_AP, &ip);
    if (OPRT_OK != op_ret) {
        PR_ERR("get ip fail:%d", op_ret);
        op_ret = OPRT_NOT_FOUND;
    }

    char *json_buf = tal_malloc(256);
    if (NULL == json_buf) {
        PR_ERR("tal_malloc Fails");
        return OPRT_MALLOC_FAILED;
    }

    uint32_t offset = 0;
    int ret = 0;
    ret = snprintf(json_buf + offset, 256 - offset, "{\"ip\":\"%s\", \"uuid\":\"%s\", \"active\":0", ip.ip, ap->netcfg_args.uuid);
    if (ret < 0 || ret >= 256 - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += ret;
    ret = snprintf(json_buf + offset, 256 - offset, ",\"version\":\"%s\"", TUYA_LPV35);
    if (ret < 0 || ret >= 256 - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += ret;
    ret = snprintf(json_buf + offset, 256 - offset, ",\"sl\":%d", TUYA_SECURITY_LEVEL);
    if (ret < 0 || ret >= 256 - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += ret;
    ret = snprintf(json_buf + offset, 256 - offset, ",\"apConfigType\":1");
    if (ret < 0 || ret >= 256 - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += ret;
    ret = snprintf(json_buf + offset, 256 - offset, ",\"CombosFlag\":%d", (1 << 3));
    if (ret < 0 || ret >= 256 - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += ret;

    json_buf[offset] = '}';
    json_buf[offset + 1] = 0;

    *buf = json_buf;

    return OPRT_OK;
}

static void ap_broadcast_timeout(TIMER_ID timerID, void *pTimerArg)
{
    OPERATE_RET op_ret = OPRT_OK;

    char *json_buf = NULL;
    ap_netcfg_t *ap = (ap_netcfg_t *)pTimerArg;

    op_ret = ap_dev_config_make(ap, &json_buf);
    if (OPRT_OK != op_ret) {
        PR_ERR("ap_broadcast_data_make fail");
        return;
    }

    size_t plaintext_len = sizeof(lpv35_plaintext_data_t) + strlen(json_buf);
    lpv35_plaintext_data_t *plaintext_data = tal_malloc(plaintext_len);
    if (plaintext_data == NULL) {
        PR_ERR("plaintext_data fail");
        tal_free(json_buf);
        return;
    }
    plaintext_data->ret_code = 0;
    memcpy(plaintext_data->data, json_buf, strlen(json_buf));
    tal_free(json_buf);
    // lpv3.5 test arch
    lpv35_frame_object_t frame = {
        .sequence = 0,
        .type = FRM_TYPE_AP_ENCRYPTION,
        .data = (uint8_t *)plaintext_data,
        .data_len = plaintext_len,
    };
    size_t olen = 0;
    uint8_t *send_buf = (uint8_t *)tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf == NULL) {
        PR_ERR("send_buf malloc fail");
        tal_free(plaintext_data);
        return;
    }
    op_ret = lpv35_frame_serialize(ap->app_key, APP_KEY_LEN, &frame, send_buf, (int *)&olen);
    tal_free(plaintext_data);
    if (op_ret != OPRT_OK) {
        PR_ERR("lpv35_frame_serialize fail:%d", op_ret);
        tal_free(send_buf);
        return;
    }

    op_ret = tal_net_send_to(ap->broadcast_fd, send_buf, olen, TY_IPADDR_BROADCAST, AP_BROADCAST_PORT);
    if (op_ret < 0) {
        PR_ERR("sendto broadcast Failed,len:%d ret:%d,errno:%d", olen, op_ret, tal_net_get_errno());
    }

    tal_free(send_buf);
}

static int ap_cfg_cmd_patse(ap_netcfg_t *ap, char *data)
{
    cJSON *root = NULL;
    root = cJSON_Parse((char *)data);

    if (NULL == root) {
        PR_ERR("json parse err:%s", data);
        return OPRT_CJSON_GET_ERR;
    }

    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring == NULL || ssid_item->valuestring[0] == '\0') {
        PR_ERR("data format err:%s", data);
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }
    const char *ssid = ssid_item->valuestring;
    PR_DEBUG("Parse ssid:%s", ssid);

    char *token = NULL;
    cJSON *token_item = cJSON_GetObjectItem(root, "token");
    if (cJSON_IsString(token_item) && token_item->valuestring && token_item->valuestring[0]) {
        token = token_item->valuestring;
        PR_DEBUG("Parse token:%s", token);
    }

    char *passwd = NULL;
    cJSON *passwd_item = cJSON_GetObjectItem(root, "passwd");
    if (cJSON_IsString(passwd_item) && passwd_item->valuestring && passwd_item->valuestring[0]) {
        passwd = passwd_item->valuestring;
        PR_DEBUG("Parse passwd:%s", passwd);
    }

    memset(&ap->netcfg_info, 0, sizeof(ap->netcfg_info));

    // Copy SSID
    size_t s_len = strnlen(ssid, WIFI_SSID_LEN + 1);
    if (s_len == 0 || s_len > WIFI_SSID_LEN) {
        PR_ERR("ssid len invalid:%zu", s_len);
        cJSON_Delete(root);
        return OPRT_CJSON_GET_ERR;
    }
    memcpy(ap->netcfg_info.ssid, ssid, s_len);
    ap->netcfg_info.ssid[s_len] = '\0';
    ap->netcfg_info.s_len = (uint8_t)s_len;

    if (passwd == NULL) {
        ap->netcfg_info.p_len = 0;
    } else {
        // Copy password
        size_t p_len = strnlen(passwd, WIFI_PASSWD_LEN + 1);
        if (p_len == 0 || p_len > WIFI_PASSWD_LEN) {
            PR_ERR("passwd len invalid:%zu", p_len);
            cJSON_Delete(root);
            return OPRT_CJSON_GET_ERR;
        }
        memcpy(ap->netcfg_info.passwd, passwd, p_len);
        ap->netcfg_info.passwd[p_len] = '\0';
        ap->netcfg_info.p_len = (uint8_t)p_len;
    }

    if (token) {
        // Copy token
        size_t t_len = strnlen(token, WL_TOKEN_LEN + 1);
        if (t_len == 0 || t_len > WL_TOKEN_LEN) {
            PR_ERR("token len invalid:%zu", t_len);
            cJSON_Delete(root);
            return OPRT_CJSON_GET_ERR;
        }
        memcpy(ap->netcfg_info.token, token, t_len);
        ap->netcfg_info.token[t_len] = '\0';
        ap->netcfg_info.t_len = (uint8_t)t_len;
    }

    cJSON *reg = cJSON_GetObjectItem(root, "reg");
    if (reg) {
        char *app_reg = cJSON_PrintUnformatted(reg);
        cJSON_free(app_reg);
        if (OPRT_OK != tuya_register_center_save(RCS_APP, reg)) {
            PR_ERR("save to reg center err");
        }
    }

    cJSON_Delete(root);

    return OPRT_OK;
}

static int ap_setup_tls_serv(ap_netcfg_t *ap)
{
    int fd;
    int ret = OPRT_OK;

    /* create listening TCP socket */
    fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("ap socket create fail:%d", tal_net_get_errno());
        ret = fd;
        goto __exit;
    }

    ret = tal_net_set_reuse(fd);
    if (OPRT_OK != ret) {
        PR_ERR("ap set reuse fail:%d", tal_net_get_errno());
        ret = OPRT_SOCK_ERR;
        goto __exit;
    }

    int bind_port = ap->is_psk_pincode ? AP_TLS_PSK_PINCODE_PORT : AP_TLS_PSK_PORT;
    PR_DEBUG("ap try bind port:%d", bind_port);
    ret = tal_net_bind(fd, ap->serv_ip, bind_port);
    if (OPRT_OK != ret) {
        PR_ERR("ap bind fail:%d", tal_net_get_errno());
        ret = OPRT_SOCK_ERR;
        goto __exit;
    }
    ret = tal_net_listen(fd, 1);
    if (OPRT_OK != ret) {
        PR_ERR("ap listen fail:%d", tal_net_get_errno());
        ret = OPRT_SOCK_ERR;
        goto __exit;
    }
    ap->tls_hander = tuya_tls_connect_create();
    if (NULL == ap->tls_hander) {
        PR_ERR("ap tls create fail:%d", tal_net_get_errno());
        ret = OPRT_MALLOC_FAILED;
        goto __exit;
    }

    ap->psk_fd = fd;

    return OPRT_OK;

__exit:
    if (fd > 0) {
        tal_net_close(fd);
    }

    return ret;
}

static int ap_tls_psk_set(ap_netcfg_t *ap)
{
    if (ap->is_psk_pincode) {
        if (OPRT_OK != ap_pbkdf2_cacl(ap->netcfg_args.pincode, ap->netcfg_args.uuid, ap->tls_psk, AP_TLS_PSK_LEN)) {
            PR_ERR("psk cacl error");
            return OPRT_COM_ERROR;
        }
        PR_DEBUG("ap->netcfg_args.pincode %s", ap->netcfg_args.pincode);
        PR_HEXDUMP_NOTICE("psk", ap->tls_psk, AP_TLS_PSK_LEN);
        tuya_tls_config_set(ap->tls_hander, &(tuya_tls_config_t){.mode = TUYA_TLS_PSK_MODE,
                                                                 .psk_key = (char *)ap->tls_psk,
                                                                 .psk_key_size = AP_TLS_PSK_LEN,
                                                                 .psk_id = ap->netcfg_args.uuid,
                                                                 .psk_id_size = strlen(ap->netcfg_args.uuid)});
    } else {
        tuya_tls_config_set(ap->tls_hander, &(tuya_tls_config_t){.mode = TUYA_TLS_PSK_MODE,
                                                                 .psk_key = "123456",
                                                                 .psk_key_size = strlen("123456"),
                                                                 .psk_id = "psk_identity",
                                                                 .psk_id_size = strlen("psk_identity")});
    }

    return OPRT_OK;
}

static int ap_send(ap_netcfg_t *ap, uint32_t frame_type, uint32_t ret_code, uint8_t *p_data, uint32_t data_len)
{
    int op_ret = OPRT_OK;

    size_t plaintext_len = sizeof(lpv35_plaintext_data_t) + data_len;
    lpv35_plaintext_data_t *plaintext_data = tal_malloc(plaintext_len);
    if (plaintext_data == NULL) {
        PR_ERR("plaintext_data fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(plaintext_data, 0, sizeof(lpv35_plaintext_data_t));

    plaintext_data->ret_code = ret_code;
    if (p_data != NULL) {
        memcpy(plaintext_data->data, p_data, data_len);
    }

    // lpv3.5 test arch
    lpv35_frame_object_t frame = {
        .sequence = 0,
        .type = frame_type,
        .data = (uint8_t *)plaintext_data,
        .data_len = plaintext_len,
    };

    size_t olen = 0;
    uint8_t *send_buf = (uint8_t *)tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf == NULL) {
        PR_ERR("send_buf malloc fail");
        return OPRT_MALLOC_FAILED;
    }

    op_ret = lpv35_frame_serialize(ap->app_key, APP_KEY_LEN, &frame, send_buf, (int *)&olen);
    if (op_ret != OPRT_OK) {
        PR_ERR("lpv35_frame_serialize fail:%d", op_ret);
        tal_free(send_buf);
        return OPRT_COM_ERROR;
    }

    op_ret = tuya_tls_write(ap->tls_hander, send_buf, olen);
    if (op_ret != olen) {
        PR_ERR("tuya_tls_write_ap fail! write:%d, olen:%d, frame_type:%d", op_ret, olen, frame_type);
    }

    PR_TRACE("tls write :%d", op_ret);
    tal_free(send_buf);
    return OPRT_OK;
}

static int ap_get_wifi_list(char *wifi_list, uint16_t wifi_list_size, uint16_t max_cnt)
{
    int ret = OPRT_OK;
    uint16_t offset = 0;
    uint16_t loop = 0;
    AP_IF_S *ap_if = NULL;
    uint32_t ap_num = 0;
    BOOL_T first_ap = TRUE;

    TUYA_CHECK_NULL_RETURN(wifi_list, OPRT_INVALID_PARM);

    ret = tal_wifi_all_ap_scan(&ap_if, &ap_num);
    if ((OPRT_OK != ret) || (ap_num == 0)) {
        PR_DEBUG("scan ap null:%d %d", ret, ap_num);

        snprintf(wifi_list + offset, wifi_list_size - offset, "{\"wifi_list\":[]}");
        return OPRT_OK;
    }

    // Sort and get the max cnt data with the strongest signal
    int n = snprintf(wifi_list + offset, wifi_list_size - offset, "{\"wifi_list\":[");
    if (n < 0 || n >= wifi_list_size - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += n;
    max_cnt = (max_cnt > ap_num) ? ap_num : max_cnt;
    for (loop = 0; loop < max_cnt; loop++) {
        if (0 == strlen((char *)ap_if[loop].ssid)) {
            continue;
        }
        if (offset + 64 > wifi_list_size) {
            break;
        }
        if (!first_ap) {
            n = snprintf(wifi_list + offset, wifi_list_size - offset, ",");
            if (n < 0 || n >= wifi_list_size - offset) break;
            offset += n;
        }
        n = snprintf(wifi_list + offset, wifi_list_size - offset, "{\"ssid\":\"%s\",\"rssi\":%d,\"sec\":%u}", ap_if[loop].ssid,
                          ap_if[loop].rssi, ap_if[loop].security);
        if (n < 0 || n >= wifi_list_size - offset) break;
        offset += n;
        first_ap = FALSE;
    }
    n = snprintf(wifi_list + offset, wifi_list_size - offset, "]}");
    if (n < 0 || n >= wifi_list_size - offset) return OPRT_BUFFER_NOT_ENOUGH;
    offset += n;
    tal_wifi_release_ap(ap_if);

    return ret;
}

static int ap_ext_cmd_parse(ap_netcfg_t *ap, char *data)
{
    int rt = OPRT_OK;
    cJSON *root = NULL;
    char *buffer = NULL;
    uint16_t buffer_size = 1024 * 4 + 32;

    // PR_DEBUG("ap_ext_cmd_parse data %s", data);
    //  dev_log_collect
    TUYA_CHECK_NULL_RETURN(root = cJSON_Parse(data), OPRT_INVALID_PARM);

    cJSON *reqtype = cJSON_GetObjectItem(root, "reqType");
    if (NULL == reqtype) {
        rt = OPRT_CJSON_GET_ERR;
        goto __exit;
    }

    buffer = tal_malloc(buffer_size);
    if (NULL == buffer) {
        rt = OPRT_MALLOC_FAILED;
        goto __exit;
    }

    if (0 == strcmp(reqtype->valuestring, "query_dev")) {
        char *data = NULL;
        rt = ap_dev_config_make(ap, &data);
        if (OPRT_OK != rt) {
            goto __exit;
        }
        snprintf(buffer, buffer_size, "{\"reqType\":\"query_dev_rpt\",\"data\":%s}", data);
        tal_free(data);
    } else if (0 == strcmp(reqtype->valuestring, "get_wifi_list")) {
        cJSON *jdata = cJSON_GetObjectItem(root, "data");
        if (NULL == jdata) {
            rt = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        cJSON *item = cJSON_GetObjectItem(jdata, "cnt");
        if (NULL == item) {
            rt = OPRT_CJSON_GET_ERR;
            goto __exit;
        }
        int16_t offset = snprintf(buffer, buffer_size, "{\"reqType\":\"wifi_list_rpt\",\"data\":");
        if (offset < 0 || offset >= buffer_size) {
            rt = OPRT_BUFFER_NOT_ENOUGH;
            goto __exit;
        }
        TUYA_CALL_ERR_GOTO(ap_get_wifi_list(buffer + offset, buffer_size - offset, item->valueint), __exit);
        size_t cur_len = strlen(buffer);
        if (cur_len + 2 < buffer_size) {
            buffer[cur_len] = '}';
            buffer[cur_len + 1] = '\0';
        } else {
            rt = OPRT_BUFFER_NOT_ENOUGH;
            goto __exit;
        }
    } else if (0 == strcmp(reqtype->valuestring,
                           "query_netcfg_stat")) { //  query_netcfg_stat
        char *out = "{\"type\":1,\"stage\":2,\"status\":0}";
        snprintf(buffer, buffer_size, "{\"reqType\":\"netcfg_stat_rpt\",\"data\":%s}", out);
    } else {
        PR_DEBUG("not support reqtype:%s", reqtype->valuestring);
        memset(buffer, 0, buffer_size);
    }
    PR_DEBUG("report %s", buffer);

    //! report:
    rt = ap_send(ap, AP_CFG_EXT_CMD, 0, (uint8_t *)buffer, strlen(buffer));

__exit:
    if (root) {
        cJSON_Delete(root);
    }

    if (buffer) {
        tal_free(buffer);
    }

    return rt;
}

/**
 * Receives an AP frame and stores it in the specified output object.
 *
 * @param ap The AP network configuration object.
 * @param out The output object to store the received frame.
 * @return Returns an integer value indicating the status of the operation.
 */
int ap_frame_recv(ap_netcfg_t *ap, lpv35_frame_object_t *out)
{
    int ret = OPRT_OK;
    int read_len = tuya_tls_read(ap->tls_hander, ap->recv_buf, sizeof(ap->recv_buf));
    if (read_len <= 0) {
        PR_ERR("ap tls read fail. %d", read_len);
        ret = OPRT_RECV_ERR;
        goto __err_exit;
    }
    // recv data process
    if (read_len < LPV35_FRAME_MINI_SIZE) {
        PR_ERR("not enough data len:%d", read_len);
        return OPRT_COM_ERROR;
    }

    uint32_t offset = 0;
    uint8_t *frame_buffer = NULL;
    while (read_len >= LPV35_FRAME_MINI_SIZE + offset) {
        if (memcmp(ap->recv_buf + offset, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE) != 0) {
            offset++;
            continue;
        }
        frame_buffer = ap->recv_buf + offset;
        lpv35_fixed_head_t *fixed_head = (lpv35_fixed_head_t *)(frame_buffer + LPV35_FRAME_HEAD_SIZE);
        // frame_len verify
        uint32_t frame_len =
            LPV35_FRAME_HEAD_SIZE + sizeof(lpv35_fixed_head_t) + UNI_NTOHL(fixed_head->length) + LPV35_FRAME_TAIL_SIZE;
        if (frame_len > (read_len - offset)) { // recv data not enough buf
            if (frame_len > AP_MAX_BUFSIZE) {
                PR_ERR("frame data len is out of limit");
                return OPRT_BUFFER_NOT_ENOUGH;
            }
            uint32_t rd_size = 0;
            uint32_t nd_size = frame_len - (read_len - offset);
            //! init frame
            memmove(ap->recv_buf, ap->recv_buf + offset, read_len - offset);
            frame_buffer = ap->recv_buf + (read_len - offset);
            while (rd_size < nd_size) {
                read_len = tuya_tls_read(ap->tls_hander, ((uint8_t *)frame_buffer + rd_size), nd_size - rd_size);
                if (read_len <= 0) {
                    TUYA_ERRNO tuya_errno = tal_net_get_errno();
                    if (tuya_errno == UNW_EINTR || tuya_errno == UNW_EAGAIN) {
                        tal_system_sleep(10);
                        continue;
                    }
                    break;
                }
                rd_size += read_len;
            }
            if (read_len <= 0 || rd_size < nd_size) {
                PR_ERR("tls nd read Fail. %d, frame size %d,  nd_size %d", read_len, frame_len, nd_size);
                goto __err_exit;
            }
        }
        //! one by one
        ret = lpv35_frame_parse(ap->app_key, APP_KEY_LEN, frame_buffer, frame_len, out);
        if (ret != OPRT_OK) {
            PR_ERR("lpv35_frame_parse fail:%d", ret);
            ap_send(ap, UNI_NTOHL(fixed_head->type), 1, NULL, 0);
        }
        break;
    }

    return ret;

__err_exit:
    tal_net_close(ap->client_fd);
    ap->client_fd = -1;
    tuya_tls_disconnect(ap->tls_hander);

    return ret;
}

/**
 * @brief Exits the AP task.
 *
 * This function is used to exit the AP task.
 *
 * @param ap Pointer to the `ap_netcfg_t` structure.
 */
void ap_task_exit(ap_netcfg_t *ap)
{
    int rt = OPRT_OK;

    if (ap->broadcast_timer) {
        tal_sw_timer_delete(ap->broadcast_timer);
        ap->broadcast_timer = NULL;
    }

    if (ap->broadcast_fd) {
        tal_net_close(ap->broadcast_fd);
        ap->broadcast_fd = -1;
    }

    if (ap->client_fd >= 0) {
        tal_net_close(ap->client_fd);
        ap->client_fd = -1;
    }

    if (ap->psk_fd >= 0) {
        tal_net_close(ap->psk_fd);
        ap->psk_fd = -1;
    }

    if (ap->tls_hander) {
        tuya_tls_disconnect(ap->tls_hander);
        tuya_tls_connect_destroy(ap->tls_hander);
        ap->tls_hander = NULL;
    }

    if (ap->thread != NULL) {
        TUYA_CALL_ERR_LOG(tal_thread_delete(ap->thread));
        ap->thread = NULL;
    }

    ap_netcfg_free();
}

static void ap_netcfg_thread(void *args)
{
    int ret = OPRT_OK;
    TLS_TCP_STAT_E status = TSS_START;
    int actv_cnt = 0;
    int max_fd = 0;
    TUYA_FD_SET_T readfds;
    TUYA_FD_SET_T errfds;

    PR_DEBUG("start TCP ap thread");

    ap_netcfg_t *ap = (ap_netcfg_t *)args;

    ap_app_key_make(ap->app_key);

    while (!ap->thread_exit_flag) {

        switch (status) {

        case TSS_START: {
            if (OPRT_OK != ap_setup_tls_serv(ap)) {
                PR_ERR("create server socket err");
                tal_system_sleep(1500);
                continue;
            }
            PR_DEBUG("Waiting for a remote connection");
            status = TSS_ACCEPT;
        } break;

        case TSS_ACCEPT: {
            // set readfd
            tal_net_fd_zero(&readfds);
            tal_net_fd_zero(&errfds);
            tal_net_fd_set(ap->psk_fd, &readfds);
            max_fd = ap->psk_fd;
            if (-1 != ap->client_fd) {
                tal_net_fd_set(ap->client_fd, &readfds);
                max_fd = max_fd > ap->client_fd ? max_fd : ap->client_fd;
            }
            actv_cnt = tal_net_select(max_fd + 1, &readfds, NULL, &errfds, 1000);
            if (actv_cnt < 0) {
                PR_ERR("Select failed:errno:%d", tal_net_get_errno());
                tal_system_sleep(1500);
                continue;
            } else if (0 == actv_cnt) {
                continue;
            } else {
                PR_TRACE("active socket num:%d", actv_cnt);
                if (tal_net_fd_isset(ap->psk_fd, &errfds)) {
                    PR_DEBUG("recv socket err event");
                    continue;
                }
            }
            if (tal_net_fd_isset(ap->psk_fd, &readfds)) {
                PR_TRACE("recv tcp packets,tls+gcm mode");
                TUYA_IP_ADDR_T addr = 0;
                if (-1 != ap->client_fd) {
                    tal_net_close(ap->client_fd);
                    ap->client_fd = -1;
                    tuya_tls_disconnect(ap->tls_hander);
                }
                ap->client_fd = tal_net_accept(ap->psk_fd, &addr, NULL);
                if (ap->client_fd < 0) {
                    ret = OPRT_SET_SOCK_ERR;
                    tal_system_sleep(1500);
                    PR_ERR("accept failed %d (errno: %d)", ap->client_fd, tal_net_get_errno());
                    continue;
                }
                // set reuse
                ret = tal_net_set_reuse(ap->client_fd);
                // set no block
                ret += tal_net_set_block(ap->client_fd, FALSE);
                if (0 > ret) {
                    PR_ERR("tcp listen select fail");
                    tal_system_sleep(1500);
                    continue;
                }
                PR_DEBUG("new client connect. fd:%d ip:%d.%d.%d.%d", ap->client_fd, (uint8_t)(addr >> 24),
                         (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr);
                ret = ap_tls_psk_set(ap);
                if (OPRT_OK != ret) {
                    PR_ERR("tls psk set fail");
                    continue;
                }
                ret = tuya_tls_connect(ap->tls_hander, NULL, 0, ap->client_fd, 10 * 1000);
                if (ret != OPRT_OK) {
                    tal_net_close(ap->client_fd);
                    ap->client_fd = -1;
                    tuya_tls_disconnect(ap->tls_hander);
                    PR_ERR("tls connect Fail. %d", ret);
                    continue;
                }
            } else if (tal_net_fd_isset(ap->client_fd, &readfds)) {
                lpv35_frame_object_t frame_object;

                ret = ap_frame_recv(ap, &frame_object);
                if (OPRT_OK != ret) {
                    PR_DEBUG("ap_frame_recv error %d", ret);
                    continue;
                }
                if (frame_object.type == FRM_AP_CFG_WF_V40) {
                    ret = ap_cfg_cmd_patse(ap, (char *)frame_object.data);
                    ap_send(ap, frame_object.type, ret, NULL, 0);
                    if (OPRT_OK != ret) {
                        PR_ERR("wifi ap_rcv parse fail. ret:%x.", ret);
                        continue;
                    }
                    if (ap->netcfg_finish_cb) {
                        ap->netcfg_finish_cb(NETCFG_TUYA_WIFI_AP, &ap->netcfg_info);
                    }
                } else if (frame_object.type == AP_CFG_EXT_CMD) {
                    ap_ext_cmd_parse(ap, (char *)frame_object.data);
                }
                tal_free(frame_object.data);
            }
        } break;

        default: /* do nothing */
            break;
        }
    }

    ap_task_exit(ap);

    PR_DEBUG("ap thread exit");
}

static int ap_mode_start(ap_netcfg_t *ap)
{
    int op_ret = OPRT_OK;

    op_ret = tal_wifi_set_work_mode(WWM_SOFTAP);
    if (OPRT_OK != op_ret) {
        PR_ERR("wf_wk_mode_set error:%d", op_ret);
        return op_ret;
    }

    WF_AP_CFG_IF_S ap_cfg = {0};

    NW_MAC_S mac;
    memset(&mac, 0, sizeof(NW_MAC_S));
    op_ret = tal_wifi_get_mac(WF_AP, &mac);
    if (OPRT_OK != op_ret) {
        PR_ERR("get mac failed:%d", op_ret);
        return op_ret;
    }
    //! default ip info
    strncpy(ap_cfg.ip.ip, "192.168.176.1", sizeof(ap_cfg.ip.ip) - 1);
    ap_cfg.ip.ip[sizeof(ap_cfg.ip.ip) - 1] = '\0';
    strncpy(ap_cfg.ip.gw, "192.168.176.1", sizeof(ap_cfg.ip.gw) - 1);
    ap_cfg.ip.gw[sizeof(ap_cfg.ip.gw) - 1] = '\0';
    strncpy(ap_cfg.ip.mask, "255.255.255.0", sizeof(ap_cfg.ip.mask) - 1);
    ap_cfg.ip.mask[sizeof(ap_cfg.ip.mask) - 1] = '\0';
    //! default ssid
    snprintf((char *)ap_cfg.ssid, sizeof(ap_cfg.ssid), "%s-%02X%02X", TUYA_AP_SSID_DEFAULT, mac.mac[4], mac.mac[5]);
    ap_cfg.s_len = strlen((char *)ap_cfg.ssid);
    ap_cfg.md = WAAM_OPEN;
    ap_cfg.chan = 6;
    ap_cfg.max_conn = AP_MAX_STA_CONN;
    ap_cfg.ms_interval = 100;

    op_ret = tal_wifi_ap_start(&ap_cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("start ap failed:%d", op_ret);
        return op_ret;
    } else {
        PR_DEBUG("start ap success:%s", ap_cfg.ssid);
    }

    return OPRT_OK;
}

static int ap_netcfg_start(int type, netcfg_finish_cb_t cb, void *args)
{
    int op_ret = OPRT_OK;

    ap_netcfg_t *ap = ap_netcfg_get();

    PR_DEBUG("ap cfg start:%d", type);

    if (NULL == cb || NULL == ap) {
        PR_ERR("ap mgr or netcfg_finish_cb NULL");
        return OPRT_MALLOC_FAILED;
    }

    op_ret = ap_mode_start(ap);
    if (OPRT_OK != op_ret) {
        PR_ERR("ap mode start fail:%d", op_ret);
        goto __exit;
    }

    NW_IP_S ip;
    memset(&ip, 0, sizeof(ip));
    op_ret = tal_wifi_get_ip(WF_AP, &ip);
    if (OPRT_OK != op_ret) {
        PR_ERR("get ip fail:%d", op_ret);
        op_ret = OPRT_NOT_FOUND;
        goto __exit;
    }
    PR_DEBUG("ap netcfg server ip:%s", ip.ip);
    ap->serv_ip = tal_net_str2addr(ip.ip);
    ap->netcfg_finish_cb = cb;
    ap->client_fd = -1;
    ap->psk_fd = -1;

    if (NULL == ap->netcfg_args.pincode || 0 == strlen(ap->netcfg_args.pincode)) {
        PR_NOTICE("tuya ap using tls + psk", op_ret);
        ap->broadcast_fd = tal_net_socket_create(PROTOCOL_UDP);
        if (ap->broadcast_fd < 0) {
            PR_ERR("net_socket_create fail:%d", tal_net_get_errno());
            op_ret = OPRT_COM_ERROR;
            goto __exit;
        }
        tal_net_set_broadcast(ap->broadcast_fd);
        tal_net_bind(ap->broadcast_fd, ap->serv_ip, AP_BROADCAST_PORT);
        op_ret = tal_sw_timer_create((TAL_TIMER_CB)ap_broadcast_timeout, ap, &(ap->broadcast_timer));
        op_ret |= tal_sw_timer_start(ap->broadcast_timer, 1 * 1000,
                                     TAL_TIMER_CYCLE); // 1s
        if (OPRT_OK != op_ret) {
            goto __exit;
        }
    } else {
        ap->is_psk_pincode = true;
        PR_NOTICE("tuya ap using tls + psk(pincode), scan qrcode");
    }

    THREAD_CFG_T thread_cfg = {.priority = THREAD_PRIO_2, .stackDepth = 4096, .thrdname = "ap_cfg_task"};
    op_ret = tal_thread_create_and_start(&ap->thread, NULL, NULL, ap_netcfg_thread, ap, &thread_cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("tuya cli create thread failed %d", op_ret);
        goto __exit;
    }

    return op_ret;

__exit:
    if (ap) {
        if (ap->broadcast_timer) {
            tal_sw_timer_delete(ap->broadcast_timer);
            ap->broadcast_timer = NULL;
        }
        if (ap->broadcast_fd > 0) {
            tal_net_close(ap->broadcast_fd);
        }
    }

    ap_netcfg_free();

    return op_ret;
}

static int ap_netcfg_stop(int type)
{
    int rt = OPRT_OK;
    TUYA_CALL_ERR_LOG(tal_wifi_ap_stop());
    TUYA_CALL_ERR_LOG(tal_wifi_set_work_mode(WWM_STATION));
    ap_netcfg_t *ap = ap_netcfg_get();
    if (ap) {
        if (ap->broadcast_timer) {
            tal_sw_timer_stop(ap->broadcast_timer);
        }
        ap->thread_exit_flag = TRUE;
    }

    PR_DEBUG("ap cfg stop:%d", type);

    return OPRT_OK;
}

/**
 * @brief Initializes the AP network configuration.
 *
 * This function initializes the AP network configuration by waiting for a
 * reset, allocating memory for the AP network configuration structure, and
 * copying the provided network configuration arguments to the structure.
 *
 * @param[in] netcfg_args Pointer to the network configuration arguments.
 * @return Returns the result of the netcfg_register function.
 */
int ap_netcfg_init(netcfg_args_t *netcfg_args)
{
    //! simple wait for reset
    while (s_ap_netcfg != NULL) {
        tal_system_sleep(200);
    }

    TUYA_CHECK_NULL_RETURN(s_ap_netcfg = tal_malloc(sizeof(ap_netcfg_t)), OPRT_MALLOC_FAILED);
    memset(s_ap_netcfg, 0, sizeof(ap_netcfg_t));
    memcpy(&s_ap_netcfg->netcfg_args, netcfg_args, sizeof(netcfg_args_t));

    return netcfg_register(NETCFG_TUYA_WIFI_AP, ap_netcfg_start, ap_netcfg_stop);
}
