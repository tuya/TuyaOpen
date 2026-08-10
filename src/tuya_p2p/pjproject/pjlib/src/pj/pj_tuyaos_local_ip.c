/**
 * @file pj_tuyaos_local_ip.c
 * @brief Local IPv4 helper via TAL (WiFi STA or wired)
 * @version 1.2
 * @date 2026-08-06
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tal_local_ip.h"
#include "tuya_cloud_types.h"
#include "tuya_iot_config.h"

#include <stdio.h>
#include <string.h>

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tal_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "tal_wired.h"
#endif

/**
 * @brief Parse dotted IPv4 into network-byte-order u32
 * @param[in] s dotted string
 * @param[out] nbo network-order address
 * @return 0 on success, -1 on failure
 */
STATIC INT_T __parse_ipv4_nbo(CONST CHAR_T *s, UINT32_T *nbo)
{
    UINT32_T a, b, c, d;
    UINT8_T bytes[4];

    if (s == NULL || nbo == NULL) {
        return -1;
    }
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return -1;
    }
    if (a > 255U || b > 255U || c > 255U || d > 255U) {
        return -1;
    }
    bytes[0] = (UINT8_T)a;
    bytes[1] = (UINT8_T)b;
    bytes[2] = (UINT8_T)c;
    bytes[3] = (UINT8_T)d;
    memcpy(nbo, bytes, sizeof(bytes));
    return 0;
}

/**
 * @brief Reject loopback / empty after parsing NBO address
 * @param[in] a network-order IPv4
 * @return 0 if usable, -1 if reject
 */
STATIC INT_T __reject_bad_host(UINT32_T a)
{
    UINT32_T host;
    UINT8_T host_bytes[4];

    memcpy(host_bytes, &a, sizeof(host_bytes));
    host = ((UINT32_T)host_bytes[0] << 24) | ((UINT32_T)host_bytes[1] << 16) |
           ((UINT32_T)host_bytes[2] << 8) | (UINT32_T)host_bytes[3];
    if ((host >> 24) == 127U || (host >> 24) == 0U) {
        return -1;
    }
    return 0;
}

/**
 * @brief Get station/wired IPv4 in network byte order via TAL
 * @param[out] addr_nbo IPv4 address (network byte order)
 * @return 0 on success, -1 on failure / unavailable / loopback
 */
int tal_compat_get_sta_ipv4_nbo(unsigned int *addr_nbo)
{
    NW_IP_S ip;
    UINT32_T a;

    if (addr_nbo == NULL) {
        return -1;
    }

    memset(&ip, 0, sizeof(ip));

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    if (tal_wifi_get_ip(WF_STATION, &ip) == OPRT_OK && ip.ip[0] != '\0') {
        if (__parse_ipv4_nbo(ip.ip, &a) == 0 && __reject_bad_host(a) == 0) {
            *addr_nbo = (unsigned int)a;
            return 0;
        }
    }
#endif

#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    memset(&ip, 0, sizeof(ip));
    if (tal_wired_get_ip(&ip) == OPRT_OK && ip.ip[0] != '\0') {
        if (__parse_ipv4_nbo(ip.ip, &a) == 0 && __reject_bad_host(a) == 0) {
            *addr_nbo = (unsigned int)a;
            return 0;
        }
    }
#endif

    (void)ip;
    (void)a;
    return -1;
}
