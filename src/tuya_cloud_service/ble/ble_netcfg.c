/**
 * @file ble_netcfg.c
 * @brief BLE network configuration transport adapter.
 *
 * Handles BLE transparent subcommands belonging to provisioning:
 *   - 0x0001 (BT_NETCFG): receive SSID/password/token from the app
 *   - 0x0003 (WIFI_LST):  scan and return nearby WiFi hotspots
 *   - 0x0004 (NETCFG_STAT): answer status queries and proactively push stage/error changes
 *
 * Scanning, sorting, deduplication, JSON escaping, and provisioning state storage live in netcfg/.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "tal_api.h"
#include "tal_event_info.h"
#include "tal_workq_service.h"
#include "cJSON.h"
#include "netcfg.h"
#include "netcfg_wifi_list.h"
#include "netmgr.h"
#include "tuya_iot.h"
#include "tuya_register_center.h"
#include "ble_mgr.h"
#include "ble_channel.h"
#include "ble_netcfg.h"

#define BLE_WIFI_LIST_MAX_CNT 10

#ifndef BLE_WIFI_LIST_BUF_SIZE
#ifdef BT_WIFI_LIST_BUF_SIZE
#define BLE_WIFI_LIST_BUF_SIZE BT_WIFI_LIST_BUF_SIZE
#else
#define BLE_WIFI_LIST_BUF_SIZE 1024
#endif
#endif

typedef struct {
    netcfg_args_t      netcfg_args;
    netcfg_info_t      netcfg_info;
    netcfg_finish_cb_t netcfg_finish_cb;
} ble_netcfg_t;

typedef struct {
    uint32_t max_cnt;
    char     ccode[COUNTRY_CODE_LEN + 1];
} ble_wifi_list_req_t;

static ble_netcfg_t s_bt_netcfg_handle;

#if defined(ENABLE_BT_WIFI_LIST) && (ENABLE_BT_WIFI_LIST == 1)
static bool s_wifi_list_enable = true;
#else
static bool s_wifi_list_enable = false;
#endif
static uint32_t s_wifi_list_buf_size = BLE_WIFI_LIST_BUF_SIZE;
static bool     s_wifi_list_scanning = false;

/***********************************************************
 ***************** 0x0001: BLE Netcfg **********************
 ***********************************************************/
static void __handle_net_cfg(void *data, uint32_t len, void *user_data)
{
    uint8_t result = 0;
    uint8_t resp[5];

    (void)user_data;

    cJSON *json = cJSON_ParseWithLength((const char *)data, len);
    if (NULL == json) {
        PR_ERR("json parse error.");
        result = (uint8_t)OPRT_CJSON_PARSE_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }

    cJSON *ssid_item  = cJSON_GetObjectItem(json, "ssid");
    cJSON *token_item = cJSON_GetObjectItem(json, "token");
    if (!cJSON_IsString(ssid_item) || !cJSON_IsString(token_item)) {
        PR_ERR("json get error.");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }

    char  *ssid        = ssid_item->valuestring;
    char  *token       = token_item->valuestring;
    char  *passwd      = NULL;
    cJSON *passwd_item = cJSON_GetObjectItem(json, "pwd");
    if (passwd_item != NULL && cJSON_IsString(passwd_item)) {
        passwd = passwd_item->valuestring;
    }

    if (NULL == passwd) {
        PR_ERR("json get passwd error.");
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }

    memset(&s_bt_netcfg_handle.netcfg_info, 0, sizeof(s_bt_netcfg_handle.netcfg_info));

    size_t s_len = strnlen(ssid, WIFI_SSID_LEN + 1);
    if (s_len == 0 || s_len > WIFI_SSID_LEN) {
        PR_ERR("ssid len invalid:%zu", s_len);
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }
    memcpy(s_bt_netcfg_handle.netcfg_info.ssid, ssid, s_len);
    s_bt_netcfg_handle.netcfg_info.ssid[s_len] = '\0';
    s_bt_netcfg_handle.netcfg_info.s_len       = (uint8_t)s_len;

    size_t p_len = strnlen(passwd, WIFI_PASSWD_LEN + 1);
    if (p_len > WIFI_PASSWD_LEN) {
        PR_ERR("passwd len invalid:%zu", p_len);
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }
    if (p_len > 0) {
        memcpy(s_bt_netcfg_handle.netcfg_info.passwd, passwd, p_len);
    }
    s_bt_netcfg_handle.netcfg_info.passwd[p_len] = '\0';
    s_bt_netcfg_handle.netcfg_info.p_len         = (uint8_t)p_len;

    size_t t_len = strnlen(token, WL_TOKEN_LEN + 1);
    if (t_len == 0 || t_len > WL_TOKEN_LEN) {
        PR_ERR("token len invalid:%zu", t_len);
        result = (uint8_t)OPRT_CJSON_GET_ERR;
        netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_PCK);
        goto __exit;
    }
    memcpy(s_bt_netcfg_handle.netcfg_info.token, token, t_len);
    s_bt_netcfg_handle.netcfg_info.token[t_len] = '\0';
    s_bt_netcfg_handle.netcfg_info.t_len        = (uint8_t)t_len;

    netcfg_stat_set(NETCFG_STAGE_NETCFG, NETCFG_ERR_SUCC);
    if (NULL != s_bt_netcfg_handle.netcfg_finish_cb) {
        s_bt_netcfg_handle.netcfg_finish_cb(NETCFG_TUYA_BLE, &s_bt_netcfg_handle.netcfg_info);
    }

    cJSON *reg = cJSON_GetObjectItem(json, "reg");
    if (reg) {
        tuya_register_center_save(RCS_APP, reg);
    }

__exit:
    if (json) {
        cJSON_Delete(json);
    }
    resp[0] = 0x00;
    resp[1] = 0x00;
    resp[2] = 0x00;
    resp[3] = FRM_DATA_TRANS_SUBCMD_BT_NETCFG;
    resp[4] = result;
    tuya_ble_send(FRM_UPLINK_TRANSPARENT_REQ, 0, resp, 5);
}

/***********************************************************
 ***************** 0x0003: WiFi Hotspot List ***************
 ***********************************************************/
void ble_wifi_list_enable(bool enable)
{
    if (s_wifi_list_enable == enable) {
        return;
    }

    s_wifi_list_enable = enable;
    ble_channel_wifi_list_cap_set(enable);
    tuya_ble_adv_update();
}

bool ble_wifi_list_is_enable(void)
{
    return s_wifi_list_enable;
}

OPERATE_RET ble_wifi_list_buf_size_set(uint32_t size)
{
    if (size < BLE_WIFI_LIST_BUF_SIZE_MIN || size > BLE_WIFI_LIST_BUF_SIZE_MAX) {
        return OPRT_INVALID_PARM;
    }

    s_wifi_list_buf_size = size;
    return OPRT_OK;
}

uint32_t ble_wifi_list_buf_size_get(void)
{
    uint32_t size = s_wifi_list_buf_size;

    if (size < BLE_WIFI_LIST_BUF_SIZE_MIN) {
        size = BLE_WIFI_LIST_BUF_SIZE_MIN;
    } else if (size > BLE_WIFI_LIST_BUF_SIZE_MAX) {
        size = BLE_WIFI_LIST_BUF_SIZE_MAX;
    }

    return size;
}

static void __ble_wifi_list_task(void *data)
{
    ble_wifi_list_req_t *req      = (ble_wifi_list_req_t *)data;
    uint32_t             max_cnt  = BLE_WIFI_LIST_MAX_CNT;
    uint32_t             buf_size = ble_wifi_list_buf_size_get();
    uint32_t             json_len = 0;
    uint8_t             *resp     = NULL;

    if (req) {
        max_cnt = req->max_cnt;
        if (req->ccode[0]) {
            netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_COUNTRYCODE, req->ccode);
        }
        tal_free(req);
    }

    resp = (uint8_t *)tal_malloc(buf_size + BLE_CHANNEL_REPLY_HDR_LEN);
    if (NULL == resp) {
        PR_ERR("malloc err");
        s_wifi_list_scanning = false;
        return;
    }

    if (s_wifi_list_enable) {
        json_len = netcfg_wifi_list_build((char *)(resp + BLE_CHANNEL_REPLY_HDR_LEN), buf_size, max_cnt);
    } else {
        int n = snprintf((char *)(resp + BLE_CHANNEL_REPLY_HDR_LEN), buf_size, "{\"wifi_list\":[]}");
        json_len = (n > 0 && (uint32_t)n < buf_size) ? (uint32_t)n : 0;
    }

    s_wifi_list_scanning = false;
    ble_channel_reply_buf(FRM_DATA_TRANS_SUBCMD_WIFI_LST, resp, json_len);
}

static void __ble_wifi_list_query(void *data, uint32_t len, void *user_data)
{
    (void)user_data;

    if (s_wifi_list_scanning) {
        PR_DEBUG("ble wifi list scan already in progress, ignore duplicate request");
        return;
    }

    ble_wifi_list_req_t *req = (ble_wifi_list_req_t *)tal_malloc(sizeof(ble_wifi_list_req_t));
    if (NULL == req) {
        PR_ERR("malloc err");
        return;
    }

    memset(req, 0, sizeof(ble_wifi_list_req_t));
    req->max_cnt = BLE_WIFI_LIST_MAX_CNT;

    if (data && len) {
        cJSON *root = cJSON_ParseWithLength((const char *)data, len);
        if (root) {
            cJSON *cnt   = cJSON_GetObjectItem(root, "cnt");
            cJSON *ccode = cJSON_GetObjectItem(root, "ccode");
            if (cJSON_IsNumber(cnt) && cnt->valueint > 0) {
                req->max_cnt = (uint32_t)cnt->valueint;
            }
            if (cJSON_IsString(ccode)) {
                size_t ccode_len = strlen(ccode->valuestring);
                if (ccode_len > 0 && ccode_len <= COUNTRY_CODE_LEN) {
                    memcpy(req->ccode, ccode->valuestring, ccode_len);
                }
            }
            cJSON_Delete(root);
        }
    }

    s_wifi_list_scanning = true;
    if (OPRT_OK != tal_workq_schedule(WORKQ_SYSTEM, __ble_wifi_list_task, req)) {
        PR_ERR("schedule workq err");
        s_wifi_list_scanning = false;
        tal_free(req);
    }
}

/***********************************************************
 ***************** 0x0004: Netcfg Status *******************
 ***********************************************************/
OPERATE_RET ble_netcfg_stat_report(uint32_t type)
{
    char json[64];
    int  n = 0;

    if (0 == type && !tuya_ble_is_paired()) {
        return OPRT_OK;
    }

    n = netcfg_stat_format(json, sizeof(json), type);
    if (n <= 0) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    return ble_channel_reply(FRM_DATA_TRANS_SUBCMD_NETCFG_STAT, (const uint8_t *)json, (uint32_t)n);
}

static void __ble_netcfg_stat_push_work(void *data)
{
    (void)data;
    (void)ble_netcfg_stat_report(0);
}

static void __ble_netcfg_stat_on_change(netcfg_stage_e stage, netcfg_err_code_e status)
{
    (void)stage;
    (void)status;

    if (!tuya_ble_is_paired()) {
        return;
    }

    (void)tal_workq_schedule(WORKQ_SYSTEM, __ble_netcfg_stat_push_work, NULL);
}

static void __ble_netcfg_stat_query(void *data, uint32_t len, void *user_data)
{
    (void)data;
    (void)len;
    (void)user_data;

    (void)ble_netcfg_stat_report(1);
}

/***********************************************************
 ***************** Lifecycle / Registration ****************
 ***********************************************************/
void ble_netcfg_combos_register(void)
{
    ble_channel_wifi_list_cap_set(s_wifi_list_enable);
    ble_channel_add(BLE_CHANNLE_WIFI_LST, __ble_wifi_list_query, NULL);
    ble_channel_add(BLE_CHANNLE_NETCFG_STAT, __ble_netcfg_stat_query, NULL);
    netcfg_stat_cb_register(__ble_netcfg_stat_on_change);
}

void ble_netcfg_combos_unregister(void)
{
    netcfg_stat_cb_register(NULL);
    ble_channel_del(BLE_CHANNLE_WIFI_LST);
    ble_channel_del(BLE_CHANNLE_NETCFG_STAT);
    ble_channel_wifi_list_cap_set(false);
}

int ble_netcfg_start(int type, netcfg_finish_cb_t cb, void *args)
{
    (void)type;
    (void)args;

    PR_DEBUG("bt netcfg start");
    s_bt_netcfg_handle.netcfg_finish_cb = cb;
    ble_channel_add(BLE_CHANNLE_NETCFG, __handle_net_cfg, NULL);
    tuya_ble_adv_update();

    return OPRT_OK;
}

int ble_netcfg_stop(int type)
{
    (void)type;

    PR_DEBUG("bt netcfg stop");
    ble_channel_del(BLE_CHANNLE_NETCFG);

    return OPRT_OK;
}

int ble_netcfg_init(netcfg_args_t *netcfg_args)
{
    (void)netcfg_args;

    memset(&s_bt_netcfg_handle, 0, sizeof(s_bt_netcfg_handle));

    return netcfg_register(NETCFG_TUYA_BLE, ble_netcfg_start, ble_netcfg_stop);
}
