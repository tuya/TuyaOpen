/**
 * @file ble_wifi_list.c
 * @brief BLE WiFi hotspot list query over the large data transparent channel.
 */

#include "tal_log.h"
#include "tal_workq_service.h"
#include "tkl_wifi.h"
#include "cJSON.h"
#include "netmgr.h"
#include "string.h"
#include "tal_memory.h"
#include "tal_wifi.h"
#include "stdio.h"
#include "ble_channel.h"

#define BLE_WIFI_LIST_MAX_CNT    10
#define BLE_WIFI_LIST_BUF_SIZE   1024
#define BLE_WIFI_LIST_AP_RESERVE 96

typedef struct {
    uint32_t max_cnt;
    char     ccode[COUNTRY_CODE_LEN + 1];
} ble_wifi_list_req_t;

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

static uint32_t __ble_wifi_list_make(char *json, uint32_t size, uint32_t max_cnt)
{
    AP_IF_S *ap_arr = NULL;
    uint32_t ap_num = 0;
    uint32_t offset = 0;
    uint32_t loop   = 0;
    bool     first  = true;
    int      n      = 0;

    if (OPRT_OK != tal_wifi_all_ap_scan(&ap_arr, &ap_num) || NULL == ap_arr || 0 == ap_num) {
        if (ap_arr) {
            tal_wifi_release_ap(ap_arr);
        }
        n = snprintf(json, size, "{\"wifi_list\":[]}");
        return (n > 0) ? (uint32_t)n : 0;
    }

    __ble_wifi_list_sort(ap_arr, ap_num);
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
        offset += (uint32_t)snprintf(json + offset, size - offset, "{\"ssid\":\"%s\",\"rssi\":%d,\"sec\":%u}",
                                     (char *)ap_arr[loop].ssid, ap_arr[loop].rssi, (unsigned)ap_arr[loop].security);
        first = false;
    }
    offset += (uint32_t)snprintf(json + offset, size - offset, "]}");

    tal_wifi_release_ap(ap_arr);

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

void ble_wifi_list_query(void *data, void *user_data)
{
    ble_wifi_list_req_t *req = (ble_wifi_list_req_t *)tal_malloc(sizeof(ble_wifi_list_req_t));

    if (NULL == req) {
        PR_ERR("malloc err");
        return;
    }

    memset(req, 0, sizeof(ble_wifi_list_req_t));
    req->max_cnt = BLE_WIFI_LIST_MAX_CNT;

    if (data) {
        cJSON *root = cJSON_Parse((char *)data);
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
