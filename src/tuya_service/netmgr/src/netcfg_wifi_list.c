/**
 * @file netcfg_wifi_list.c
 * @brief Shared WiFi AP scanning, RSSI sorting, SSID deduplication, and JSON serialization.
 *
 * Used by both AP provisioning (ap_netcfg.c) and BLE provisioning (ble_netcfg.c).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "netcfg_wifi_list.h"

#include <stdio.h>
#include <string.h>

#include "tal_log.h"
#include "tal_wifi.h"

static void __sort_by_rssi_desc(AP_IF_S *ap_arr, uint32_t ap_num)
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

static bool __is_duplicate_ssid(const AP_IF_S *ap_arr, uint32_t idx)
{
    uint32_t i = 0;

    for (i = 0; i < idx; i++) {
        if ('\0' == ap_arr[i].ssid[0]) {
            continue;
        }
        if (0 == strncmp((const char *)ap_arr[i].ssid, (const char *)ap_arr[idx].ssid, WIFI_SSID_LEN)) {
            return true;
        }
    }

    return false;
}

static bool __escape_json_str(char *dst, size_t dst_size, const char *src, size_t src_max_len)
{
    size_t i = 0;
    size_t o = 0;

    if (NULL == dst || 0 == dst_size || NULL == src) {
        return false;
    }

    for (i = 0; i < src_max_len && '\0' != src[i]; i++) {
        unsigned char ch = (unsigned char)src[i];

        if ('"' == ch || '\\' == ch) {
            if (o + 2 >= dst_size) {
                return false;
            }
            dst[o++] = '\\';
            dst[o++] = (char)ch;
        } else if (ch < 0x20) {
            if (o + 6 >= dst_size) {
                return false;
            }
            int n = snprintf(dst + o, dst_size - o, "\\u%04x", (unsigned int)ch);
            if (6 != n) {
                return false;
            }
            o += 6;
        } else {
            if (o + 1 >= dst_size) {
                return false;
            }
            dst[o++] = (char)ch;
        }
    }

    dst[o] = '\0';
    return true;
}

uint32_t netcfg_wifi_list_build(char *json, uint32_t size, uint32_t max_cnt)
{
    AP_IF_S *ap_arr  = NULL;
    uint32_t ap_num  = 0;
    uint32_t offset  = 0;
    uint32_t loop    = 0;
    uint32_t emitted = 0;
    int      n       = 0;

    if (NULL == json || size < 18) {
        return 0;
    }

    if (OPRT_OK != tal_wifi_all_ap_scan(&ap_arr, &ap_num) || NULL == ap_arr || 0 == ap_num) {
        if (NULL != ap_arr) {
            tal_wifi_release_ap(ap_arr);
        }
        n = snprintf(json, size, "{\"wifi_list\":[]}");
        return (n > 0 && (uint32_t)n < size) ? (uint32_t)n : 0;
    }

    __sort_by_rssi_desc(ap_arr, ap_num);

    n = snprintf(json, size, "{\"wifi_list\":[");
    if (n <= 0 || (uint32_t)n >= size) {
        tal_wifi_release_ap(ap_arr);
        return 0;
    }
    offset = (uint32_t)n;

    for (loop = 0; loop < ap_num && emitted < max_cnt; loop++) {
        char escaped_ssid[WIFI_SSID_LEN * 6 + 1];
        char entry[sizeof(escaped_ssid) + 64];

        if ('\0' == ap_arr[loop].ssid[0]) {
            continue;
        }
        if (__is_duplicate_ssid(ap_arr, loop)) {
            continue;
        }
        if (!__escape_json_str(escaped_ssid, sizeof(escaped_ssid), (const char *)ap_arr[loop].ssid, WIFI_SSID_LEN)) {
            continue;
        }

        n = snprintf(entry, sizeof(entry), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"sec\":%u}",
                     (0 == emitted) ? "" : ",", escaped_ssid, (int)ap_arr[loop].rssi,
                     (unsigned int)ap_arr[loop].security);
        if (n <= 0 || (size_t)n >= sizeof(entry)) {
            continue;
        }

        /* Keep 3 bytes for "]}" and NUL terminator */
        if (offset + (uint32_t)n + 3 > size) {
            break;
        }

        memcpy(json + offset, entry, (uint32_t)n);
        offset += (uint32_t)n;
        emitted++;
    }

    tal_wifi_release_ap(ap_arr);

    if (offset + 3 <= size) {
        json[offset++] = ']';
        json[offset++] = '}';
        json[offset]   = '\0';
    }

    return offset;
}
