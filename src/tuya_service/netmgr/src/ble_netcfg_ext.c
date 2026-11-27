/**
 * @file ble_netcfg_ext.c
 * @brief BLE combos provisioning extensions over tuyaopen-core's transparent channel.
 *
 * Registers WiFi-dependent provisioning subcommands on tuyaopen-core's BLE channel:
 *   - 0x0003 (WIFI_LST):    scan and return nearby WiFi hotspots
 *   - 0x0004 (NETCFG_STAT): answer status queries and proactively push stage/error changes
 * Also hooks ble_netcfg_parse_cb_register() so 0x0001 parse outcomes update netcfg_stat.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>

#include "tal_api.h"
#include "tal_workq_service.h"
#include "cJSON.h"
#include "netcfg.h"
#include "netcfg_wifi_list.h"
#include "netmgr.h"
#include "ble_mgr.h"
#include "ble_channel.h"
#include "ble_netcfg.h"
#include "ble_netcfg_ext.h"

#define BLE_WIFI_LIST_MAX_CNT 10

#ifndef BLE_WIFI_LIST_BUF_SIZE
#ifdef BT_WIFI_LIST_BUF_SIZE
#define BLE_WIFI_LIST_BUF_SIZE BT_WIFI_LIST_BUF_SIZE
#else
#define BLE_WIFI_LIST_BUF_SIZE 1024
#endif
#endif

typedef struct {
    uint32_t max_cnt;
    char     ccode[COUNTRY_CODE_LEN + 1];
} ble_wifi_list_req_t;

#if defined(ENABLE_BT_WIFI_LIST) && (ENABLE_BT_WIFI_LIST == 1)
static bool s_wifi_list_enable = true;
#else
static bool s_wifi_list_enable = false;
#endif
static uint32_t s_wifi_list_buf_size = BLE_WIFI_LIST_BUF_SIZE;
static bool     s_wifi_list_scanning = false;

/***********************************************************
 ***************** 0x0001 Parse Observer *******************
 ***********************************************************/
static void __ble_netcfg_on_parse(bool ok)
{
    netcfg_stat_set(NETCFG_STAGE_NETCFG, ok ? NETCFG_ERR_SUCC : NETCFG_ERR_PCK);
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
        int n    = snprintf((char *)(resp + BLE_CHANNEL_REPLY_HDR_LEN), buf_size, "{\"wifi_list\":[]}");
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
    ble_netcfg_parse_cb_register(__ble_netcfg_on_parse);
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
    ble_netcfg_parse_cb_register(NULL);
}
