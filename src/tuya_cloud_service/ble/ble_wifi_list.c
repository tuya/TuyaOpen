/**
 * @file ble_wifi_list.c
 * @brief BLE WiFi hotspot list query over the large data transparent channel.
 *
 * The app asks for the surrounding hotspots on subcommand 0x0003 and gets a
 * {"wifi_list":[{"ssid","rssi","sec"}]} reply back on 0x801F. The scan blocks
 * for several seconds, so the request is only copied here and the scan runs on
 * WORKQ_SYSTEM.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_log.h"
#include "tal_workq_service.h"
#include "tkl_wifi.h"
#include "cJSON.h"
#include "netmgr.h"
#include "string.h"
#include "tal_memory.h"
#include "tal_wifi.h"
#include "tal_system.h"
#include "stdio.h"
#include "ble_channel.h"

#define BLE_WIFI_LIST_MAX_CNT      10
#define BLE_WIFI_LIST_BUF_SIZE     1024
#define BLE_WIFI_LIST_AP_RESERVE   128
#define BLE_WIFI_LIST_SSID_ESC_LEN ((WIFI_SSID_LEN * 2) + 1)

typedef struct {
    uint32_t max_cnt;
    char     ccode[COUNTRY_CODE_LEN + 1];
} ble_wifi_list_req_t;

static bool       s_wifi_list_enable = true;
static AP_IF_S   *s_ap_cache         = NULL;
static uint32_t   s_ap_cache_num     = 0;

static void __ble_wifi_list_sort(AP_IF_S *ap_arr, uint32_t ap_num)
{
    uint32_t i = 0;
    uint32_t j = 0;
    AP_IF_S  tmp;

    for (i = 0; i < ap_num; i++) {
        for (j = i + 1; j < ap_num; j++) {
            if (ap_arr[i].rssi < ap_arr[j].rssi) {
                memcpy(&tmp, &ap_arr[i], sizeof(AP_IF_S));
                memcpy(&ap_arr[i], &ap_arr[j], sizeof(AP_IF_S));
                memcpy(&ap_arr[j], &tmp, sizeof(AP_IF_S));
            }
        }
    }
}

static void __ble_wifi_list_cache_free_task(void *data)
{
    AP_IF_S *ap_arr = (AP_IF_S *)data;

    if (ap_arr) {
        tal_wifi_release_ap(ap_arr);
    }
}

void ble_wifi_list_cache_clear(void)
{
    AP_IF_S *stale = s_ap_cache;

    s_ap_cache      = NULL;
    s_ap_cache_num  = 0;

    if (NULL == stale) {
        return;
    }

    /* the walk runs on WORKQ_SYSTEM, so queue the free behind it; keep the reader on that queue */
    if (OPRT_OK != tal_workq_schedule(WORKQ_SYSTEM, __ble_wifi_list_cache_free_task, stale)) {
        PR_ERR("schedule ap cache free err");
    }
}

void ble_wifi_list_enable(bool enable)
{
    if (s_wifi_list_enable == enable) {
        return;
    }

    s_wifi_list_enable = enable;
    tuya_ble_adv_update();
}

bool ble_wifi_list_is_enable(void)
{
    return s_wifi_list_enable;
}

static OPERATE_RET __ble_wifi_list_ap_get(AP_IF_S **ap_arr, uint32_t *ap_num)
{
    AP_IF_S   *arr = NULL;
    uint32_t   num = 0;

    ble_wifi_list_cache_clear();

    if (OPRT_OK != tal_wifi_all_ap_scan(&arr, &num) || NULL == arr || 0 == num) {
        if (arr) {
            tal_wifi_release_ap(arr);
        }
        return OPRT_COM_ERROR;
    }

    __ble_wifi_list_sort(arr, num);

    s_ap_cache      = arr;
    s_ap_cache_num  = num;

    *ap_arr = s_ap_cache;
    *ap_num = s_ap_cache_num;

    return OPRT_OK;
}

/* escape quotes, backslashes and control chars so an ssid cannot break the json */
static uint32_t __ble_wifi_list_escape(const uint8_t *src, uint32_t src_len, char *dst, uint32_t dst_size)
{
    uint32_t i;
    uint32_t offset = 0;

    for (i = 0; i < src_len; i++) {
        uint8_t ch = src[i];

        if ('"' == ch || '\\' == ch) {
            if (offset + 2 >= dst_size) {
                break;
            }
            dst[offset++] = '\\';
            dst[offset++] = (char)ch;
        } else if (ch < 0x20) {
            if (offset + 6 >= dst_size) {
                break;
            }
            offset += (uint32_t)snprintf(dst + offset, dst_size - offset, "\\u%04x", (unsigned)ch);
        } else {
            if (offset + 1 >= dst_size) {
                break;
            }
            dst[offset++] = (char)ch;
        }
    }
    dst[offset] = '\0';

    return offset;
}

static uint32_t __ble_wifi_list_make(char *json, uint32_t size, uint32_t max_cnt)
{
    AP_IF_S *ap_arr = NULL;
    uint32_t ap_num = 0;
    uint32_t offset = 0;
    uint32_t loop   = 0;
    bool     first  = true;
    char     ssid[BLE_WIFI_LIST_SSID_ESC_LEN];
    int      n = 0;

    if (!s_wifi_list_enable || OPRT_OK != __ble_wifi_list_ap_get(&ap_arr, &ap_num) || NULL == ap_arr || 0 == ap_num) {
        n = snprintf(json, size, "{\"wifi_list\":[]}");
        return (n > 0) ? (uint32_t)n : 0;
    }

    offset = (uint32_t)snprintf(json, size, "{\"wifi_list\":[");
    if (max_cnt > ap_num) {
        max_cnt = ap_num;
    }

    for (loop = 0; loop < max_cnt; loop++) {
        if (0 == strlen((char *)ap_arr[loop].ssid)) {
            continue;
        }
        if (offset + BLE_WIFI_LIST_AP_RESERVE > size) {
            break;
        }
        if (!first) {
            offset += (uint32_t)snprintf(json + offset, size - offset, ",");
        }
        __ble_wifi_list_escape(ap_arr[loop].ssid, (uint32_t)strlen((char *)ap_arr[loop].ssid), ssid, sizeof(ssid));
        offset += (uint32_t)snprintf(json + offset, size - offset, "{\"ssid\":\"%s\",\"rssi\":%d,\"sec\":%u}", ssid,
                                     ap_arr[loop].rssi, (unsigned)ap_arr[loop].security);
        first = false;
    }
    offset += (uint32_t)snprintf(json + offset, size - offset, "]}");

    return offset;
}

static void __ble_wifi_list_task(void *data)
{
    ble_wifi_list_req_t *req      = (ble_wifi_list_req_t *)data;
    uint32_t             max_cnt  = BLE_WIFI_LIST_MAX_CNT;
    uint32_t             json_len = 0;
    uint32_t             resp_len = 0;
    char                *json     = NULL;
    uint8_t             *resp     = NULL;

    if (req) {
        max_cnt = req->max_cnt;
        if (req->ccode[0]) {
            netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_COUNTRYCODE, req->ccode);
        }
        tal_free(req);
    }

    json = (char *)tal_malloc(BLE_WIFI_LIST_BUF_SIZE);
    if (NULL == json) {
        PR_ERR("malloc err");
        return;
    }
    json_len = __ble_wifi_list_make(json, BLE_WIFI_LIST_BUF_SIZE, max_cnt);

    resp_len = json_len + 4;
    resp     = (uint8_t *)tal_malloc(resp_len);
    if (NULL == resp) {
        PR_ERR("malloc err");
        tal_free(json);
        return;
    }
    resp[0] = 0x00;
    resp[1] = 0x00;
    resp[2] = 0x00;
    resp[3] = FRM_DATA_TRANS_SUBCMD_WIFI_LST;
    memcpy(resp + 4, json, json_len);
    tal_free(json);

    ble_channle_ack(FRM_UPLINK_TRANSPARENT_SPEC_REQ, resp, resp_len);
}

void ble_wifi_list_query(void *data, uint32_t len, void *user_data)
{
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

    if (OPRT_OK != tal_workq_schedule(WORKQ_SYSTEM, __ble_wifi_list_task, req)) {
        PR_ERR("schedule workq err");
        tal_free(req);
    }
}
