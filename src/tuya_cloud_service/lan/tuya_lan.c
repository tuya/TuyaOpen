/**
 * @file tuya_lan.c
 * @brief Implementation of Tuya LAN communication protocol handling, including
 * session management, encryption/decryption of messages, and handling of LAN
 * commands.
 *
 * This file provides the functionality required to manage LAN sessions between
 * the Tuya IoT devices and the local network, ensuring secure and reliable
 * communication. It includes mechanisms for session creation, closure, fault
 * handling, and encryption/decryption of LAN messages using the GCM mode.
 * Additionally, it supports the registration and handling of LAN commands
 * through a flexible callback mechanism.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_event.h"
#include "tuya_protocol.h"
#include "cipher_wrapper.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#include "uni_random.h"
#include "tuya_lan.h"
#include "tal_network.h"
#include "mix_method.h"
#include "lan_sock.h"
#include "tuya_iot_dp.h"
#include "crc32i.h"
#include "cJSON.h"
#include "netmgr.h"

#define SERV_PORT_TCP           6668 // device listens for the APP TCP connection
#define SERV_PORT_APP_UDP_BCAST 7000 // APP broadcast, device listening port

#define UDP_T_ITRV         5 // s
#define CLIENT_LMT         3
#define RECV_BUF_LMT       512
#define LAN_FRAME_MAX_LEN  (4 * 1024)
#define HEART_BEAT_TIMEOUT 30
#define ALLOW_NO_KEY_NUM   3

#define HMAC_LEN       32
#define RAND_LEN       16
#define SESSIONKEY_LEN 16

typedef struct {
    BOOL_T active;
    BOOL_T fault;
    int fd;
    TIME_T time;
    uint32_t sequence_in;
    uint32_t sequence_out;
    uint8_t randA[RAND_LEN];
    uint8_t randB[RAND_LEN];
    uint8_t hmac[HMAC_LEN];
    uint8_t secret_key[SESSIONKEY_LEN];
} lan_session_t;

typedef struct {
    uint32_t frame_type;
    lan_cmd_handler_cb handler;
} lan_cmd_cb_t;

//! FIXME:
#define LAN_CLOSED_CB_CNT 5
#define LAN_CMD_EXT_COUNT 5

typedef struct {
    uint32_t client_num;                     // client number limit
    uint32_t bufsize;                        // rev buffer size
    uint32_t heart_timeout;                  // heat beat timeout
    uint32_t sequence_err_threshold;         // sequence err threshold
    uint32_t allow_no_session_key_num;       // allow no session key num
    lan_cmd_cb_t cmd_ext[LAN_CMD_EXT_COUNT]; // ext lan cmd
} lan_cfg_t;

typedef struct {
    int fd_num;
    lan_session_t *session;
    MUTEX_HANDLE mutex;
    MUTEX_HANDLE tcp_mutex;

    int udp_serv_fd;
    int udp_client_fd; // udp socket fd
    int tcp_serv_fd;
    BOOL_T serv_fd_switch;

    NW_IP_S ip;

    tuya_iot_client_t *iot_client;
    lan_cfg_t *cfg;
    // extension
    uint32_t recv_offset;
    uint8_t recv_buf[0]; // keep it last !!!
} lan_mgr_t;

static uint8_t app_key2[APP_KEY_LEN] = {0};
static uint8_t app_key3[APP_KEY_LEN] = {0};

static lan_mgr_t *s_lan_mgr = NULL;
static lan_cfg_t s_lan_cfg = {.client_num = CLIENT_LMT,
                              .bufsize = RECV_BUF_LMT,
                              .heart_timeout = HEART_BEAT_TIMEOUT,
                              .allow_no_session_key_num = ALLOW_NO_KEY_NUM};

static lan_mgr_t *lan_mgr_get(void)
{
    return s_lan_mgr;
}

static void lan_session_free(lan_session_t *session)
{
    memset(session, 0, sizeof(lan_session_t));
    session->fd = -1;
}

static void lan_session_close(lan_session_t *session)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (lan == NULL || session == NULL) {
        PR_ERR("param err");
        return;
    }
    if (0 == lan->fd_num || session->active == false) {
        PR_ERR("close session err");
        return;
    }
    tal_mutex_lock(lan->mutex);
    if (session->fd != -1) {
        tal_event_publish(EVENT_LAN_CLIENT_CLOSE, &session->fd);
        tuya_unreg_lan_sock(session->fd);
        lan_session_free(session);
        lan->fd_num--;
    }
    tal_mutex_unlock(lan->mutex);
}

static void lan_sesison_add(int socket, TIME_T time)
{
    int i;

    lan_mgr_t *lan = lan_mgr_get();

    if (lan == NULL || socket < 0 || (lan->fd_num >= lan->cfg->client_num)) {
        PR_ERR("add socket err socket %d", socket);
        return;
    }

    tal_mutex_lock(lan->mutex);
    for (i = 0; i < lan->cfg->client_num; i++) {
        if (lan->session[i].active) {
            continue;
        }
        PR_TRACE("add session[%d] socket:%d", i, socket);
        lan_session_free(&lan->session[i]);
        lan->session[i].active = true;
        lan->session[i].fd = socket;
        lan->session[i].fault = false;
        lan->session[i].time = time;
        lan->session[i].sequence_out = uni_random_range(0xFFFF);
        lan->fd_num++;
        break;
    }
    tal_mutex_unlock(lan->mutex);
}

static void lan_session_fault_set(lan_session_t *session)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (NULL == lan || session == NULL) {
        PR_ERR("param err");
        return;
    }
    if (0 == lan->fd_num || session->active == false || session->fd < 0) {
        PR_ERR("set socket fault err %d %d", lan->fd_num, session->fd);
        return;
    }
    PR_DEBUG("set socket fault %d", session->fd);
    session->fault = true;
}

static void lan_session_close_all(void)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (NULL == lan) {
        return;
    }

    int i;
    PR_DEBUG("close all socket");

    tal_mutex_lock(lan->mutex);
    if (lan->tcp_serv_fd != -1) {
        tuya_unreg_lan_sock(lan->tcp_serv_fd);
        lan->tcp_serv_fd = -1;
    }
    if (lan->udp_serv_fd != -1) {
        tuya_unreg_lan_sock(lan->udp_serv_fd);
        lan->udp_serv_fd = -1;
    }
    for (i = 0; i < lan->cfg->client_num; i++) {
        if (lan->session[i].active) {
            tal_event_publish(EVENT_LAN_CLIENT_CLOSE, &lan->session[i].fd);
            tuya_unreg_lan_sock(lan->session[i].fd);
            lan_session_free(&lan->session[i]);
        }
    }
    lan->fd_num = 0;
    tal_mutex_unlock(lan->mutex);
}

static void lan_session_time_update(lan_session_t *session, const TIME_T time)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (NULL == lan || session == NULL) {
        PR_ERR("param err");
        return;
    }
    if (0 == lan->fd_num || session->active == false) {
        PR_ERR("up_socket_time err");
        return;
    }
    PR_TRACE("up_socket_time %d", session->time);
    session->time = time;
}

static void lan_session_time_check(const TIME_T time)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (0 == lan->fd_num) {
        return;
    }
    int i;
    for (i = 0; i < lan->cfg->client_num; i++) {
        if (lan->session[i].active) {
            if ((time - lan->session[i].time) >= 2592000 && (true != lan->session[i].fault)) { // 1 month sencond
                continue;
            } else if ((time - lan->session[i].time >= lan->cfg->heart_timeout) || (lan->session[i].fault == true)) {
                PR_DEBUG("i:%d,time:%d,time:%d,fault:%d", i, time, lan->session[i].time, lan->session[i].fault);
                lan_session_close(&lan->session[i]);
            }
        }
    }
}

static lan_session_t *lan_session_get_by_fd(int fd)
{
    int i;

    lan_mgr_t *lan = lan_mgr_get();

    for (i = 0; lan && i < lan->cfg->client_num; i++) {
        if (lan->session[i].fd == fd) {
            return &lan->session[i];
        }
    }

    return NULL;
}

static int lan_session_active_num_get(void)
{
    lan_mgr_t *lan = lan_mgr_get();

    if (lan == NULL) {
        return 0;
    }

    int num;
    tal_mutex_lock(lan->mutex);
    num = lan->fd_num;
    tal_mutex_unlock(lan->mutex);

    PR_TRACE("Curr Socket Num:%d", num);
    return num;
}

static lan_session_t *lan_sessions_get(void)
{
    lan_mgr_t *lan = lan_mgr_get();

    return lan ? lan->session : NULL;
}

int lan_msg_gcm_encrpt(uint8_t *data, uint32_t len, uint8_t **ec_data, uint32_t *ec_len, uint8_t *key,
                       uint32_t frame_type)
{
    int op_ret = OPRT_OK;
    int olen = 0;

    lpv35_frame_object_t frame = {
        .sequence = 0,
        .type = frame_type,
        .data = (uint8_t *)data,
        .data_len = len,
    };

    uint8_t *send_buf = tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf == NULL) {
        PR_ERR("send_buf malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(send_buf, 0, lpv35_frame_buffer_size_get(&frame));
    op_ret = lpv35_frame_serialize(key, APP_KEY_LEN, &frame, send_buf, &olen);
    if (OPRT_OK == op_ret) {
        *ec_data = send_buf;
        *ec_len = olen;
    }

    return op_ret;
}

static void lan_app_key_make(void)
{
    uint8_t app_key_encode[APP_KEY_LEN] = {0};
    app_key2[0] = 'y';
    app_key2[1] = 'G';
    app_key2[2] = 'A';
    app_key2[3] = 'd';
    app_key2[4] = 'l';
    app_key2[5] = 'o';
    app_key2[6] = 'p';
    app_key2[7] = 'o';
    app_key2[8] = 'P';
    app_key2[9] = 'V';
    app_key2[10] = 'l';
    app_key2[11] = 'd';
    app_key2[12] = 'A';
    app_key2[13] = 'B';
    app_key2[14] = 'f';
    app_key2[15] = 'n';
    tal_md5_ret(app_key2, APP_KEY_LEN, app_key_encode);
    memcpy(app_key2, app_key_encode, APP_KEY_LEN);

    app_key3[0] = 'W';
    app_key3[1] = 'z';
    app_key3[2] = 'Y';
    app_key3[3] = 'w';
    app_key3[4] = 'F';
    app_key3[5] = 'x';
    app_key3[6] = 'I';
    app_key3[7] = 'U';
    app_key3[8] = 'b';
    app_key3[9] = 'i';
    app_key3[10] = 'F';
    app_key3[11] = 'h';
    app_key3[12] = 'M';
    app_key3[13] = 'm';
    app_key3[14] = 'N';
    app_key3[15] = 'J';
    tal_md5_ret(app_key3, APP_KEY_LEN, app_key_encode);
    memcpy(app_key3, app_key_encode, APP_KEY_LEN);
}

static int lan_tcp_setup_serv_socket(int port)
{
    int fd = 0;
    // NW_IP_S ip;
    TUYA_IP_ADDR_T ip_addr = TY_IPADDR_ANY;
    int ret = OPRT_OK;

    /* create listening TCP socket */
    fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("Socket create fail:%d, Port:%d", tal_net_get_errno(), port);
        ret = fd;
        goto __exit;
    }
    ret = tal_net_set_reuse(fd);
    ret |= tal_net_bind(fd, ip_addr, port);
    ret |= tal_net_listen(fd, 5);
    if (OPRT_OK != ret) {
        ret = OPRT_SOCK_ERR;
        goto __exit;
    }

    return fd;

__exit:
    PR_DEBUG("setup lan tcp failed %d!", ret);
    if (fd > 0) {
        tal_net_close(fd);
    }

    return ret;
}

static int lan_get_valid_socket_num(lan_mgr_t *lan)
{
    if (NULL == lan) {
        return 0;
    }

    int num = 0;

    tal_mutex_lock(lan->mutex);
    num = lan->fd_num;
    if (0 == num) {
        tal_mutex_unlock(lan->mutex);
        return 0;
    }

    int i, fault_cnt = 0;
    for (i = 0; i < lan->cfg->client_num; i++) {
        if (lan->session[i].active) {
            if (lan->session[i].fault == true) {
                fault_cnt++;
            }
        }
    }
    tal_mutex_unlock(lan->mutex);

    if (fault_cnt >= num) {
        PR_TRACE("socket all falult:%d ", num);
        return 0;
    }

    PR_TRACE("socketNum:%d ", num - fault_cnt);
    return (num - fault_cnt);
}

static int lan_send(lan_session_t *session, uint32_t fr_num, uint32_t fr_type, uint32_t ret_code, uint8_t *data,
                    uint32_t len, BOOL_T encryption)
{
    int op_ret = OPRT_OK;
    if (session == NULL) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }
    if (session->active == false) {
        PR_ERR("session->active == false");
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(s_lan_mgr->mutex);
    if (session->fault == true) {
        PR_ERR("session is error");
        tal_mutex_unlock(s_lan_mgr->mutex);
        return OPRT_SVC_LAN_SOCKET_FAULT;
    }
    tal_mutex_unlock(s_lan_mgr->mutex);

    uint8_t *send_buf = NULL;
    uint32_t send_len = 0;

    PR_TRACE("tcp sendbuf socket:%d fr_num:%u fr_type:%d ret:%d len:%d", session->fd, fr_num, fr_type, ret_code, len);

    uint8_t *key = NULL;
    lan_mgr_t *lan = lan_mgr_get();
    if (lan->iot_client->is_activated) {
        if (session->secret_key[0]) {
            key = (uint8_t *)session->secret_key;
        } else {
            key = (uint8_t *)lan->iot_client->activate.localkey;
        }
    } else {
        //! TODO:
        return OPRT_COM_ERROR;
    }
    int plaintext_len = sizeof(lpv35_plaintext_data_t) + len;
    lpv35_plaintext_data_t *plaintext_data = tal_malloc(plaintext_len);
    if (plaintext_data == NULL) {
        PR_ERR("plaintext_data fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(plaintext_data, 0, plaintext_len);
    plaintext_data->ret_code = ret_code;
    memcpy(plaintext_data->data, data, len);
    // lpv3.5 test arch
    lpv35_frame_object_t frame = {.sequence = session->sequence_out++,
                                  .type = fr_type,
                                  .data = (void *)plaintext_data,
                                  .data_len = plaintext_len};
    send_buf = tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf == NULL) {
        PR_ERR("send_buf malloc fail");
        tal_free(plaintext_data);
        return OPRT_MALLOC_FAILED;
    }
    memset(send_buf, 0, lpv35_frame_buffer_size_get(&frame));
    op_ret = lpv35_frame_serialize(key, 16, &frame, send_buf, (int *)&send_len);
    tal_free(plaintext_data);
    if (op_ret != OPRT_OK) {
        PR_ERR("lpv35_frame_serialize fail:%d", op_ret);
        tal_free(send_buf);
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(s_lan_mgr->mutex);
    int ret = tal_net_send(session->fd, send_buf, send_len);
    if (ret <= 0 || ret != send_len) {
        if ((tal_net_get_errno() == UNW_EINTR) || (tal_net_get_errno() == UNW_EAGAIN)) {
            tal_system_sleep(100);
            ret = tal_net_send(session->fd, send_buf, send_len);
            if (ret <= 0 || ret != send_len) {
                op_ret = OPRT_SVC_LAN_SEND_ERR;
            }
        } else {
            op_ret = OPRT_SVC_LAN_SEND_ERR;
        }
    }

    tal_free(send_buf);
    if (op_ret == OPRT_SVC_LAN_SEND_ERR) {
        lan_session_fault_set(session);
        PR_ERR("ret:%d send_len:%d errno:%d", ret, send_len, tal_net_get_errno());
    }
    tal_mutex_unlock(s_lan_mgr->mutex);
    return op_ret;
}

static int lan_setup_udp_serv_socket(int port)
{
    int ret = OPRT_OK;

    int fd = 0;

    fd = tal_net_socket_create(PROTOCOL_UDP);
    if (fd < 0) {
        PR_ERR("Socket create fail:%d, Port:%d", tal_net_get_errno(), port);
        ret = fd;
        goto __exit;
    }
    ret = tal_net_set_reuse(fd);
    ret |= tal_net_bind(fd, TY_IPADDR_ANY, port);
    if (OPRT_OK != ret) {
        ret = OPRT_SOCK_ERR;
        goto __exit;
    }

    return fd;

__exit:
    if (fd > 0) {
        tal_net_close(fd);
    }

    return ret;
}

static void lan_make_udp_packets(uint8_t **out, int *p_olen)
{
    int op_ret = OPRT_OK;
    NW_IP_S ip;

    memset(&ip, 0, sizeof(NW_IP_S));

    //! TODO:
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP, &ip);

    lan_mgr_t *lan = lan_mgr_get();

    uint32_t offset = 0;
    char *id = NULL;
    if (lan->iot_client->is_activated) {
        id = lan->iot_client->activate.devid;
    } else {
        id = (char *)lan->iot_client->config.uuid;
    }

    uint32_t data_len = 256;
    char *json_buf = tal_malloc(data_len);
    if (NULL == json_buf) {
        PR_ERR("tal_malloc Fail");
        return;
    }
    memset(json_buf, 0, data_len);

    offset += sprintf(json_buf + offset, "{\"ip\":\"%s\",\"gwId\":\"%s\",\"uuid\":\"%s\"", ip.ip, id,
                      lan->iot_client->config.uuid);
    offset += sprintf(json_buf + offset, ",\"active\":%d,\"ablilty\":0", lan->iot_client->is_activated ? 2 : 0);
    offset += sprintf(json_buf + offset, ",\"encrypt\":true");
    offset += sprintf(json_buf + offset, ",\"productKey\":\"%s\"", lan->iot_client->config.productkey);
    offset += sprintf(json_buf + offset, ",\"version\":\"%s\"", TUYA_LPV35);
    offset += sprintf(json_buf + offset, ",\"sl\":%d", TUYA_SECURITY_LEVEL);
    json_buf[offset] = '}';
    json_buf[offset + 1] = 0;

    // PR_DEBUG("BufToSend %d %d:%s",data_len, offset, json_buf);
    int plaintext_len = sizeof(lpv35_plaintext_data_t) + strlen(json_buf);
    lpv35_plaintext_data_t *plaintext_data = tal_malloc(plaintext_len);
    if (plaintext_data == NULL) {
        PR_ERR("plaintext_data fail");
        tal_free(json_buf);
        return;
    }
    memset(plaintext_data, 0, plaintext_len);
    plaintext_data->ret_code = 0;
    memcpy(plaintext_data->data, json_buf, strlen(json_buf));
    tal_free(json_buf);

    // lpv3.5 test arch
    lpv35_frame_object_t frame = {
        .sequence = 0,
        .type = FRM_TYPE_ENCRYPTION,
        .data = (uint8_t *)plaintext_data,
        .data_len = plaintext_len,
    };

    uint8_t *send_buf = tal_malloc(lpv35_frame_buffer_size_get(&frame));
    if (send_buf == NULL) {
        PR_ERR("send_buf malloc fail");
        tal_free(plaintext_data);
        return;
    }
    memset(send_buf, 0, lpv35_frame_buffer_size_get(&frame));
    op_ret = lpv35_frame_serialize(app_key2, APP_KEY_LEN, &frame, send_buf, p_olen);
    tal_free(plaintext_data);
    if (op_ret != OPRT_OK) {
        PR_ERR("lpv35_frame_serialize fail:%d", op_ret);
        tal_free(send_buf);
        return;
    }

    // tuya_debug_hex_dump("frame", 8, send_buf, olen);
    // PR_DEBUG("local key:%s", gw_cntl->gw_actv.local_key);

    *out = send_buf;
}

/**
 * @brief Reports a data point (DP) value over the local area network (LAN).
 *
 * This function is used to report a data point value over the local area
 * network (LAN).
 *
 * @param dpstr The string representation of the data point value to be
 * reported.
 * @return Returns an integer value indicating the success or failure of the
 * operation. A return value of 0 indicates success, while a non-zero value
 * indicates failure.
 */
int tuya_lan_dp_report(char *dpstr)
{
    lan_mgr_t *lan = lan_mgr_get();

    int num = lan_session_active_num_get();
    if (0 == num) {
        PR_DEBUG("lan socket num is 0. skip send");
        return OPRT_INVALID_PARM;
    }
    int op_ret = OPRT_OK;
    uint8_t *out = NULL;
    uint32_t out_len = 0;

    op_ret = tuya_pack_protocol_data(DP_CMD_LAN, (const char *)dpstr, PRO_DATA_PUSH,
                                     (uint8_t *)lan->iot_client->activate.localkey, (char **)&out, &out_len);
    if (OPRT_OK != op_ret) {
        PR_ERR("pack_data_with_cmd er:%d", op_ret);
        return op_ret;
    } else {
        PR_DEBUG("Prepare To Send Lan:%s, msg_len:%d, out_len:%d", out, strlen(dpstr), out_len);
    }

    lan_session_t *session = lan_sessions_get();
    int i = 0;

    for (i = 0; i < lan->cfg->client_num; i++) {
        if (session[i].active && session[i].fault == false && session[i].secret_key != NULL) {
            op_ret = lan_send(&session[i], 0, FRM_TP_STAT_REPORT, 0, out, out_len, false);
            if (OPRT_OK != op_ret) {
                PR_ERR("tcp_send op_ret:%d", op_ret);
            }
        }
    }
    tal_free(out);

    return OPRT_OK;
}

static void lan_protocol_process(lan_mgr_t *lan, lan_session_t *session, lpv35_frame_object_t *frame)
{
    int op_ret = OPRT_OK;
    uint8_t *out = frame->data;
    uint32_t out_len = frame->data_len;

    PR_DEBUG("Process Data. FD:%d, Num:%d, Type:%d, Len:%d", session->fd, frame->sequence, frame->type,
             frame->data_len);

    switch (frame->type) {
    case FRM_TP_CMD:
    case FRM_TP_NEW_CMD: {
        PR_TRACE("Rev TP CMD %d", frame->type);
        cJSON *root = NULL;
        cJSON *data_json = NULL;
        char *describe = NULL;

        char *jsonstr = NULL;
        op_ret =
            tuya_parse_protocol_data(DP_CMD_LAN, out, out_len, lan->iot_client->activate.localkey, (char **)&jsonstr);
        if (OPRT_OK != op_ret) {
            PR_ERR("Cmd Parse Fail:%d", op_ret);
            describe = "parse data error";
            goto FRM_TP_CMD_ERR;
        }
        PR_DEBUG("JSON string:%s", jsonstr);
        root = cJSON_Parse(jsonstr);
        if (NULL == root) {
            PR_ERR("Not Json Cmd Parse Fails %s", jsonstr);
            describe = "parse data error";
            goto FRM_TP_CMD_ERR;
        }
        data_json = cJSON_DetachItemFromObject(root, "data");
        if (NULL == data_json) {
            PR_ERR("NULL == data_json");
            goto FRM_TP_CMD_ERR;
        }
        if (NULL == cJSON_GetObjectItem(data_json, "dps")) {
            PR_ERR("Json Cmd Lack devId or dps");
            describe = "data format error";
            cJSON_Delete(data_json);
            goto FRM_TP_CMD_ERR;
        }
        PR_DEBUG("Rev TP CMD. Send to User,Lan Ver 3.5");
        describe = NULL;
        tuya_iot_dp_parse(lan->iot_client, DP_CMD_LAN, data_json);

    FRM_TP_CMD_ERR:
        lan_send(session, frame->sequence, frame->type, 1, (uint8_t *)describe, describe ? strlen(describe) : 0, true);
        if (jsonstr) {
            tal_free(jsonstr);
        }
        if (root) {
            cJSON_Delete(root);
        }
        break;
    }

    case FRM_SECURITY_TYPE3:
        if (out_len < RAND_LEN) {
            PR_ERR("len < RAND_LEN, len=%d", out_len);
            break;
        }
        // randA
        memcpy(session->randA, out, RAND_LEN);
        // hmac randA
        tal_sha256_mac((const uint8_t *)s_lan_mgr->iot_client->activate.localkey,
                       strlen(s_lan_mgr->iot_client->activate.localkey), session->randA, RAND_LEN, session->hmac);
        // make randB
        uni_random_string((char *)(session->randB), RAND_LEN);
        // make frame buffer
        uint8_t *frame_buffer = tal_malloc(RAND_LEN + HMAC_LEN);
        if (NULL == frame_buffer) {
            PR_ERR("frame_buffer tal_malloc fail");
            break;
        }
        memset(frame_buffer, 0, RAND_LEN + HMAC_LEN);
        memcpy(frame_buffer, session->randB, RAND_LEN);
        memcpy(frame_buffer + RAND_LEN, session->hmac, HMAC_LEN);
        // response
        lan_send(session, frame->sequence, FRM_SECURITY_TYPE4, 0, frame_buffer, RAND_LEN + HMAC_LEN, true);
        tal_free(frame_buffer);
        break;

    case FRM_SECURITY_TYPE5:
        if (out_len < HMAC_LEN) {
            PR_ERR("len < HMAC_LEN, len=%d", out_len);
            break;
        }
        // hmac randB local
        tal_sha256_mac((const uint8_t *)s_lan_mgr->iot_client->activate.localkey,
                       strlen(s_lan_mgr->iot_client->activate.localkey), session->randB, RAND_LEN, session->hmac);
        // verify hmac
        if (memcmp(session->hmac, out, HMAC_LEN) != 0) {
            PR_ERR("verify hmac randB ERROR");
            lan_session_fault_set(session);
            break;
        }
        int i;
        for (i = 0; i < SESSIONKEY_LEN; i++) {
            session->secret_key[i] = session->randA[i] ^ session->randB[i];
        }
        size_t encrypt_olen = 0;
        uint8_t tag_tmp[LPV35_FRAME_TAG_SIZE];
        // encrytp data, make session key
        op_ret = mbedtls_cipher_auth_encrypt_wrapper(
            &(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                     .key = (unsigned char *)(lan->iot_client->activate.localkey),
                                     .key_len = 16,
                                     .nonce = session->randA,
                                     .nonce_len = LPV35_FRAME_NONCE_SIZE,
                                     .ad = NULL,
                                     .ad_len = 0,
                                     .data = session->secret_key,
                                     .data_len = SESSIONKEY_LEN},
            session->secret_key, &encrypt_olen, tag_tmp, LPV35_FRAME_TAG_SIZE);
        if (op_ret != OPRT_OK) {
            PR_ERR("aes128_gcm_encode error:%d", op_ret);
            lan_session_fault_set(session);
            break;
        }
        break;

    case FRM_QUERY_STAT:
    case FRM_QUERY_STAT_NEW: {
        cJSON *root = NULL;
        // PR_DEBUG("Rev Query Cmd %s", out);
        root = cJSON_Parse((char *)out);
        if (NULL == root) {
            PR_ERR("Json err");
            lan_send(session, frame->sequence, frame->type, 1, (uint8_t *)"data format error",
                     strlen("data format error"), true);
            goto FRM_QRY_STAT_ERR;
        }
        char *tmp_data = tuya_iot_dp_obj_dump(lan->iot_client, NULL, DP_APPEND_HEADER_FLAG);
        PR_DEBUG("dpobj str %s", tmp_data);
        if (NULL == tmp_data) {
            PR_DEBUG("nothing to report");
            lan_send(session, frame->sequence, frame->type, 1, (uint8_t *)"json obj data unvalid",
                     strlen("json obj data unvalid"), true);
            goto FRM_QRY_STAT_ERR;
        }

        PR_DEBUG("Send Query To App:%s", tmp_data);
        lan_send(session, frame->sequence, frame->type, 0, (uint8_t *)tmp_data, strlen(tmp_data), true);
        tal_free(tmp_data);
        cJSON_Delete(root);
        break;

    FRM_QRY_STAT_ERR:
        cJSON_Delete(root);
        break;
    } break;
    }
}

static void lan_tcp_client_sock_err(int fd)
{
    lan_session_t *session = lan_session_get_by_fd(fd);
    if (NULL == session) {
        PR_TRACE("session was null");
        return;
    }
    if (session->active != true) {
        return;
    }
    PR_DEBUG("socket fault, errno:%d", tal_net_get_errno());
    lan_session_fault_set(session);
    return;
}

static void lan_tcp_client_sock_read(int32_t fd)
{
    int ret = 0;
    uint8_t *frame_buffer = NULL;
    uint8_t *tmp_recv_buf = NULL;

    lan_mgr_t *lan = lan_mgr_get();
    lan_session_t *session = lan_session_get_by_fd(fd);

    if (NULL == lan || NULL == session || !session->active) {
        return;
    }

    uint32_t offset = 0;
    int recv_datalen = 0;

    lan->recv_offset = 0;

recv_again:
    recv_datalen = tal_net_recv(fd, lan->recv_buf + lan->recv_offset, lan->cfg->bufsize - lan->recv_offset);
    if (recv_datalen <= 0) {
        PR_ERR("net recv err fd:%d,errno:%d", fd, tal_net_get_errno());
        lan_session_fault_set(session);
        return;
    }
    // recv data process
    recv_datalen += lan->recv_offset;
    if (recv_datalen < LPV35_FRAME_MINI_SIZE) {
        PR_ERR("not enough data len:%d", recv_datalen);
        return;
    }

    offset = 0;
    frame_buffer = NULL;
    tmp_recv_buf = NULL;
    while (recv_datalen >= LPV35_FRAME_MINI_SIZE + offset) {
        if (tmp_recv_buf) {
            break;
        }
        if (memcmp(lan->recv_buf + offset, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE) != 0) {
            offset++;
            continue;
        }
        frame_buffer = lan->recv_buf + offset;
        lpv35_fixed_head_t *fixed_head = (lpv35_fixed_head_t *)(frame_buffer + LPV35_FRAME_HEAD_SIZE);
        // verify sequence
        uint32_t fr_sequence = UNI_NTOHL(fixed_head->sequence);
        if (fr_sequence <= session->sequence_in) {
            PR_ERR("fd:%d, sequence error in:%d, pre:%d", session->fd, fr_sequence, session->sequence_in);
            PR_ERR("threshold:%d", lan->cfg->sequence_err_threshold);
            if ((session->sequence_in - fr_sequence) >= lan->cfg->sequence_err_threshold) {
                lan_session_close(session);
            }
            break;
        }
        PR_TRACE("fr_num in:%u, pre:%u", fr_sequence, session->sequence_in);
        session->sequence_in = fr_sequence;
        // frame_len verify
        uint32_t frame_len =
            LPV35_FRAME_HEAD_SIZE + sizeof(lpv35_fixed_head_t) + UNI_NTOHL(fixed_head->length) + LPV35_FRAME_TAIL_SIZE;
        if (frame_len > (recv_datalen - offset)) { // recv data not enough buf
            if (frame_len >= LAN_FRAME_MAX_LEN) {
                PR_ERR("lan data len is out of limit");
                break;
            }
            tmp_recv_buf = tal_malloc(frame_len + 1);
            if (NULL == tmp_recv_buf) {
                PR_ERR("malloc error");
                break;
            }
            memset(tmp_recv_buf, 0, frame_len + 1);
            memcpy(tmp_recv_buf, lan->recv_buf + offset, recv_datalen - offset);
            recv_datalen = recv_datalen - offset;
            offset = 0;
            ret = tal_net_recv_nd_size(session->fd, tmp_recv_buf + recv_datalen, frame_len + 1 - recv_datalen,
                                       frame_len - recv_datalen);
            if (ret < 0) {
                PR_ERR("tuya_hal_net_recv_nd_size error ret:%d", ret);
                break;
            }
            frame_buffer = tmp_recv_buf;
        }

        uint32_t fr_type = UNI_NTOHL(fixed_head->type);
        uint8_t *key = NULL;

        //! TODO:
        if (lan->iot_client->is_activated) {
            if (fr_type == FRM_SECURITY_TYPE3 || fr_type == FRM_SECURITY_TYPE4 || fr_type == FRM_SECURITY_TYPE5) {
                lan->cfg->allow_no_session_key_num = ALLOW_NO_KEY_NUM;
                if (session->secret_key[0]) {
                    PR_WARN("already have the session_key, reset session..");
                    lan_session_close(session);
                    break;
                }
                key = (uint8_t *)lan->iot_client->activate.localkey;
            } else {
                if (0 == session->secret_key[0]) {
                    // fr_type come first than TYPE3,4,5, wait some packets
                    // before close(used in pressure test)
                    if (lan->cfg->allow_no_session_key_num > 0) {
                        PR_ERR("allow no seesion key %d", lan->cfg->allow_no_session_key_num);
                        lan->cfg->allow_no_session_key_num--;
                    } else {
                        PR_ERR("ERROR, no session_key");
                        lan_session_close(session);
                        lan->cfg->allow_no_session_key_num = ALLOW_NO_KEY_NUM;
                    }
                    break;
                }
                // PR_DEBUG("use session_key");
                key = (uint8_t *)session->secret_key;
            }
        } else {
            //! TODO:
            lan_session_close(session);
            break;
        }

        // Heartbeat packet has no data content and responds directly
        if (FRM_TP_HB == fr_type) {
            ret = lan_send(session, 0, FRM_TP_HB, 0, NULL, 0, false);
            PR_TRACE("lan heart beat:%d", ret);
            offset += frame_len;
            lan_session_time_update(session, tal_time_get_posix());
            continue;
        }
        //! TODO:
        lpv35_frame_object_t frame_out = {0};
        ret = lpv35_frame_parse(key, SESSIONKEY_LEN, frame_buffer, frame_len, &frame_out);
        if (ret != OPRT_OK) {
            PR_ERR("lpv35_frame_parse fail:%d", ret);
            break;
        }
        offset += frame_len;
        // update time
        lan_session_time_update(session, tal_time_get_posix());
        lan_protocol_process(lan, session, &frame_out);
        if (frame_out.data) {
            tal_free(frame_out.data);
        }
    }

    if (tmp_recv_buf) {
        tal_free(tmp_recv_buf);
    } else if (recv_datalen != offset) {
        PR_DEBUG("recv_datalen:%d, offset:%d", recv_datalen, offset);
        memmove(lan->recv_buf, lan->recv_buf + offset, recv_datalen - offset);
        lan->recv_offset = recv_datalen - offset;
        goto recv_again;
    }

    return;
}

static void lan_tcp_serv_sock_read(int32_t fd)
{
    int ret = OPRT_OK;
    TUYA_IP_ADDR_T addr = 0;

    int cfd = tal_net_accept(fd, &addr, NULL);
    if (cfd < 0) {
        PR_ERR("accept failed %d (errno: %d)", cfd, tal_net_get_errno());
        return;
    }
    lan_mgr_t *lan = lan_mgr_get();
    // out of limit
    if (lan_session_active_num_get() >= lan->cfg->client_num) {
        // PR_ERR("out of session limit");
        PR_ERR("out of session limit:0x%x", addr);
        tal_net_close(cfd);
        return;
    }
    // set reuse
    tal_net_set_reuse(cfd);
    // set no block
    tal_net_set_block(cfd, false);

    // add socket
    lan_sesison_add(cfd, tal_time_get_posix());
    PR_DEBUG("new session connect. nums:%d cfd:%d ip:0x%x", lan_session_active_num_get(), cfd, addr);
    // reg cfd to lan sock
    sloop_sock_t sock_info = {.sock = cfd,
                              .pre_select = NULL,
                              .read = lan_tcp_client_sock_read,
                              .err = lan_tcp_client_sock_err,
                              .quit = NULL};

    ret = tuya_reg_lan_sock(sock_info);
    if (OPRT_OK != ret) {
        tal_net_close(cfd);
        PR_ERR("register lan sock err");
    }
}

static void lan_tcp_serv_sock_err(int fd)
{
    PR_DEBUG("tcp serv sock err");
    lan_session_close_all();
}

static void lan_tcp_serv_sock_quit(void)
{
    tuya_lan_exit();
}

BOOL_T __udp_serv_is_in_packet_vaild(uint8_t *frame_buffer, uint32_t recv_datalen)
{
    // verify if it was app send
    //  recv data process
    if (recv_datalen < LPV35_FRAME_MINI_SIZE) {
        PR_ERR("not enough data len:%d", recv_datalen);
        return false;
    }

    if (memcmp(frame_buffer, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE) != 0) {
        PR_ERR("udp head err");
        return false;
    }

    lpv35_fixed_head_t *fixed_head = (lpv35_fixed_head_t *)(frame_buffer + LPV35_FRAME_HEAD_SIZE);
    uint32_t length = UNI_NTOHL(fixed_head->length);

    if (memcmp(frame_buffer + LPV35_FRAME_HEAD_SIZE + sizeof(lpv35_fixed_head_t) + length, LPV35_FRAME_TAIL,
               LPV35_FRAME_TAIL_SIZE) != 0) {
        PR_ERR("udp tail err");
        return false;
    }

    uint32_t fr_type = UNI_NTOHL(fixed_head->type);
    if (fr_type != FRM_TYPE_APP_UDP_BOARDCAST) {
        PR_ERR("fr_type:0x%x invaild", fr_type);
        return false;
    }

    return true;
}

static void lan_udp_serv_sock_read(int32_t fd)
{
    int op_ret = OPRT_OK;
    int recv_datalen = 0;
    TUYA_IP_ADDR_T addr = 0;
    TUYA_IP_ADDR_T addr_json = 0;
    uint16_t port = 0;

    lan_mgr_t *lan = lan_mgr_get();

    memset(lan->recv_buf, 0, s_lan_cfg.bufsize);
    recv_datalen = tal_net_recvfrom(fd, lan->recv_buf, s_lan_cfg.bufsize, &addr, &port);
    if (recv_datalen < 0) {
        PR_ERR("recvfrom err:%d len:%d", tal_net_get_errno(), recv_datalen);
        return;
    }
    // PR_DEBUG("udp recv port:%d,addr:0x%x", port, addr);
    uint8_t *frame_buffer = lan->recv_buf;
    if (!__udp_serv_is_in_packet_vaild(frame_buffer, recv_datalen)) {
        return;
    }
    lpv35_fixed_head_t *fixed_head = (lpv35_fixed_head_t *)(frame_buffer + LPV35_FRAME_HEAD_SIZE);
    uint32_t frame_len =
        LPV35_FRAME_HEAD_SIZE + sizeof(lpv35_fixed_head_t) + UNI_NTOHL(fixed_head->length) + LPV35_FRAME_TAIL_SIZE;
    lpv35_frame_object_t frame_out = {0};
    op_ret = lpv35_frame_parse(app_key2, APP_KEY_LEN, frame_buffer, frame_len, &frame_out);
    if (op_ret != OPRT_OK) {
        PR_ERR("lpv35_frame_parse fail:%d", op_ret);
        return;
    }
    cJSON *root = NULL;
    root = cJSON_Parse((char *)frame_out.data);
    if (NULL == root) {
        PR_ERR("Json err");
        tal_free(frame_out.data);
        return;
    }
    if ((NULL == cJSON_GetObjectItem(root, "ip")) || (NULL == cJSON_GetObjectItem(root, "from"))) {
        PR_ERR("json data invaild");
        cJSON_Delete(root);
        tal_free(frame_out.data);
        return;
    }
    addr_json = tal_net_str2addr(cJSON_GetObjectItem(root, "ip")->valuestring);
    // PR_DEBUG("ip:%s", cJSON_GetObjectItem(root, "ip")->valuestring);
    // PR_DEBUG("addr:0x%x, addr_json:0x%x", addr, addr_json);
    cJSON_Delete(root);
    tal_free(frame_out.data);

    int olen = 0;
    uint8_t *send_buf = NULL;
    lan_make_udp_packets(&send_buf, &olen);
    if (NULL == send_buf) {
        return;
    }
    int ret = tal_net_send_to(fd, send_buf, olen, addr_json, SERV_PORT_APP_UDP_BCAST);
    if (ret < 0) {
        if ((tal_net_get_errno() == UNW_EINTR) || (tal_net_get_errno() == UNW_EAGAIN)) {
            tal_system_sleep(100);
            ret = tal_net_send_to(fd, send_buf, olen, addr_json, SERV_PORT_APP_UDP_BCAST);
            if (ret < 0) {
                op_ret = OPRT_SVC_LAN_SEND_ERR;
            }
        } else {
            op_ret = OPRT_SVC_LAN_SEND_ERR;
        }
    }
    tal_free(send_buf);
    if (op_ret == OPRT_SVC_LAN_SEND_ERR) {
        PR_ERR("sendto Fail: len:%d ret:%d,errno:%d port:%d", olen, ret, tal_net_get_errno(), SERV_PORT_APP_UDP_BCAST);
    }
}

static void lan_udp_serv_sock_err(int fd)
{
    if (s_lan_mgr->udp_serv_fd != -1) {
        PR_DEBUG("udp serv sock err");
        tuya_unreg_lan_sock(s_lan_mgr->udp_serv_fd);
        s_lan_mgr->udp_serv_fd = -1;
    }
}

static int lan_udp_create_serv_socket(void)
{
    if (s_lan_mgr->udp_serv_fd != -1) {
        return s_lan_mgr->udp_serv_fd;
    }

    s_lan_mgr->udp_serv_fd = lan_setup_udp_serv_socket(SERV_PORT_APP_UDP_BCAST);
    if (s_lan_mgr->udp_serv_fd < 0) {
        PR_ERR("create udp serv fd err,%d", s_lan_mgr->udp_serv_fd);
        return -1;
    }

    sloop_sock_t udp_sock_info = {.sock = s_lan_mgr->udp_serv_fd,
                                  .pre_select = NULL,
                                  .read = lan_udp_serv_sock_read,
                                  .err = lan_udp_serv_sock_err,
                                  .quit = NULL};

    if (OPRT_OK != tuya_reg_lan_sock(udp_sock_info)) {
        PR_ERR("register lan sock err");
        tal_net_close(s_lan_mgr->udp_serv_fd);
        s_lan_mgr->udp_serv_fd = -1;
        return -2;
    }

    return s_lan_mgr->udp_serv_fd;
}

static void lan_tcp_serv_sock_pre_select(void)
{
    if (s_lan_mgr->serv_fd_switch) {
        s_lan_mgr->serv_fd_switch = false;
        PR_DEBUG("pre select close all");
        lan_session_close_all();
    }

    lan_session_time_check(tal_time_get_posix());
}

static int lan_tcp_create_serv_socket(lan_mgr_t *lan)
{
    lan->tcp_serv_fd = lan_tcp_setup_serv_socket(SERV_PORT_TCP);
    if (lan->tcp_serv_fd < 0) {
        PR_ERR("create server socket err %d", lan->tcp_serv_fd);
        lan->tcp_serv_fd = -1;
        return -1;
    }

    sloop_sock_t tcp_sock_info = {.sock = lan->tcp_serv_fd,
                                  .pre_select = lan_tcp_serv_sock_pre_select,
                                  .read = lan_tcp_serv_sock_read,
                                  .err = lan_tcp_serv_sock_err,
                                  .quit = lan_tcp_serv_sock_quit};

    if (OPRT_OK != tuya_reg_lan_sock(tcp_sock_info)) {
        tal_net_close(lan->tcp_serv_fd);
        lan->tcp_serv_fd = -1;
        PR_ERR("register lan sock err");
        return -2;
    }

    return lan->tcp_serv_fd;
}

/**
 * @brief Init and start LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_init(tuya_iot_client_t *iot_client)
{
    if (iot_client == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (s_lan_mgr) {
        return OPRT_OK;
    }

    lan_app_key_make();

    s_lan_mgr = tal_malloc(sizeof(lan_mgr_t) + s_lan_cfg.bufsize);
    if (NULL == s_lan_mgr) {
        PR_ERR("tal_malloc fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(s_lan_mgr, 0, sizeof(lan_mgr_t) + s_lan_cfg.bufsize);
    s_lan_mgr->tcp_serv_fd = -1;
    s_lan_mgr->udp_client_fd = -1;
    s_lan_mgr->udp_serv_fd = -1;
    s_lan_mgr->cfg = &s_lan_cfg;
    // INIT_LIST_HEAD(&s_lan_mgr->lan_ext_proto);

    int op_ret;
    op_ret = tuya_sock_loop_init();
    if (OPRT_OK != op_ret) {
        goto __exit;
    }
    op_ret = tal_mutex_create_init(&s_lan_mgr->mutex);
    if (OPRT_OK != op_ret) {
        goto __exit;
    }
    op_ret = tal_mutex_create_init(&s_lan_mgr->tcp_mutex);
    if (OPRT_OK != op_ret) {
        goto __exit;
    }

    uint32_t client_len = sizeof(lan_session_t) * s_lan_mgr->cfg->client_num;
    s_lan_mgr->session = tal_malloc(client_len);
    if (NULL == s_lan_mgr->session) {
        goto __exit;
    }
    memset(s_lan_mgr->session, 0, client_len);
    s_lan_mgr->iot_client = iot_client;

    if (lan_tcp_create_serv_socket(s_lan_mgr) < 0) {
        PR_ERR("init tcp serv fd err");
        goto __exit;
    }

    if (lan_udp_create_serv_socket() < 0) {
        PR_ERR("init udp serv fd err");
        goto __exit;
    }

    PR_DEBUG("lan init success");
    return OPRT_OK;

__exit:
    PR_DEBUG("init error");

    tuya_lan_exit();

    return op_ret;
}

/**
 * @brief Stop and uninit LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_exit(void)
{
    if (s_lan_mgr == NULL) {
        return OPRT_OK;
    }
    lan_session_close_all();
    if (s_lan_mgr->session) {
        tal_free(s_lan_mgr->session);
        s_lan_mgr->session = NULL;
    }
    if (s_lan_mgr->udp_client_fd >= 0) {
        tal_net_close(s_lan_mgr->udp_client_fd);
        s_lan_mgr->udp_client_fd = -1;
    }
    tal_mutex_release(s_lan_mgr->mutex);
    tal_mutex_release(s_lan_mgr->tcp_mutex);
    tal_free(s_lan_mgr);
    s_lan_mgr = NULL;

    PR_DEBUG("lan exit");

    return OPRT_OK;
}

/**
 * @brief distribute data to all connections
 *
 * @param[in] fr_type refer to LAN_PRO_HEAD_APP_S
 * @param[in] ret_code refer to LAN_PRO_HEAD_APP_S
 * @param[in] data refer to LAN_PRO_HEAD_APP_S
 * @param[in] len refer to LAN_PRO_HEAD_APP_S
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_data_report(uint32_t fr_type, uint32_t ret_code, uint8_t *data, uint32_t len)
{
    int op_ret = OPRT_OK;
    int i = 0;

    lan_mgr_t *lan = lan_mgr_get();
    if (NULL == lan) {
        return OPRT_COM_ERROR;
    }

    int num = lan_session_active_num_get();
    if (0 == num) {
        PR_ERR("lan no session");
        return OPRT_SVC_LAN_NO_CLIENT_CONNECTED;
    }

    lan_session_t *session = lan_sessions_get();

    for (i = 0; i < lan->cfg->client_num; i++) {
        if (session[i].active && session[i].fault == false) {
            op_ret = lan_send(&session[i], 0, fr_type, ret_code, data, len, true);
            if (OPRT_OK != op_ret) {
                PR_ERR("tcp_send op_ret:%d", op_ret);
            }
        }
    }

    return OPRT_OK;
}

/**
 * @brief get count of vaild connections
 *
 * @return vaild count
 */
int tuya_lan_get_connect_client_num(void)
{
    return lan_get_valid_socket_num(lan_mgr_get());
}

/**
 * @brief disconnect all connections
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_disconnect_all(void)
{
    lan_session_close_all();
    return OPRT_OK;
}

/**
 * @brief register callback
 *
 * @param[in] frame_type refer to LAN_PRO_HEAD_APP_S
 * @param[in] frame_type refer to lan_cmd_handler_cb
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_register_cb(uint32_t frame_type, lan_cmd_handler_cb handler)
{
    int ret = OPRT_EXCEED_UPPER_LIMIT;
    int i = 0;

    for (i = 0; i < LAN_CMD_EXT_COUNT; i++) {
        if ((handler == s_lan_cfg.cmd_ext[i].handler) && (frame_type == s_lan_cfg.cmd_ext[i].frame_type)) {
            break;
        }
    }

    if (i < LAN_CMD_EXT_COUNT) {
        return OPRT_OK;
    }

    for (i = 0; i < LAN_CMD_EXT_COUNT; i++) {
        if (NULL == s_lan_cfg.cmd_ext[i].handler) {
            s_lan_cfg.cmd_ext[i].handler = handler;
            s_lan_cfg.cmd_ext[i].frame_type = frame_type;
            ret = OPRT_OK;
            break;
        }
    }

    return ret;
}

/**
 * @brief unregister callback
 *
 * @param[in] frame_type refer to LAN_PRO_HEAD_APP_S
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_unregister_cb(uint32_t frame_type)
{
    int ret = OPRT_NOT_FOUND;
    int i = 0;

    for (i = 0; i < LAN_CMD_EXT_COUNT; i++) {
        if (frame_type == s_lan_cfg.cmd_ext[i].frame_type) {
            s_lan_cfg.cmd_ext[i].handler = NULL;
            s_lan_cfg.cmd_ext[i].frame_type = 0;
            ret = OPRT_OK;
            break;
        }
    }

    return ret;
}

/**
 * @brief get lan session number
 *
 * @return session number
 */
uint32_t tuya_lan_get_client_num(void)
{
    return s_lan_cfg.client_num;
}
