/**
 * @file at_utils.c
 * @brief at_utils module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_utils.h"

#include "tal_api.h"

#include <ctype.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

TUYA_IP_ADDR_T at_utils_str2addr(const char *ip_str)
{
    TUYA_IP_ADDR_T rt_ip_addr = -1;

    if (ip_str == NULL) {
        return rt_ip_addr;
    }

    const char *start = ip_str;
    const char *end = ip_str + strlen(ip_str);

    // Skip head " and tail "
    if ('"' == *start) {
        start++;
    }
    if (end > start && '"' == *(end - 1)) {
        end--;
    }

    uint8_t addr[4];
    int seg = 0;
    int val = 0;
    int digit_count = 0;
    const char *str = start;

    while (str < end && *str) {
        if (!isdigit((unsigned char)*str)) {
            if (*str == '.' && digit_count > 0) {
                if (seg >= 3)
                    return rt_ip_addr;
                addr[seg++] = (uint8_t)val;
                val = 0;
                digit_count = 0;
                str++;
                continue;
            } else {
                return rt_ip_addr;
            }
        }

        val = val * 10 + (*str - '0');
        if (val > 255)
            return rt_ip_addr;
        digit_count++;
        if (digit_count > 3)
            return rt_ip_addr;

        str++;
    }

    if (seg != 3 || digit_count == 0) {
        return rt_ip_addr;
    }
    addr[3] = (uint8_t)val;

    // Assemble into uint32_t (network byte order: high byte first)
    rt_ip_addr = ((uint32_t)addr[0] << 24) | ((uint32_t)addr[1] << 16) | ((uint32_t)addr[2] << 8) | ((uint32_t)addr[3]);

    PR_DEBUG("Converted IP string '%s' to address: 0x%08X", ip_str, rt_ip_addr);

    return rt_ip_addr;
}

char *at_utils_addr2str(TUYA_IP_ADDR_T ipaddr)
{
    static char buf[16];

    unsigned char b0 = (ipaddr >> 24) & 0xFF;
    unsigned char b1 = (ipaddr >> 16) & 0xFF;
    unsigned char b2 = (ipaddr >> 8) & 0xFF;
    unsigned char b3 = ipaddr & 0xFF;

    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b0, b1, b2, b3);

    PR_DEBUG("Converted IP address 0x%08X to string: %s", ipaddr, buf);

    return buf;
}

uint8_t at_utils_is_ipv4(const char *ip_str)
{
    if (ip_str == NULL) {
        return 0;
    }

    PR_TRACE("Checking if '%s' is a valid IPv4 address", ip_str);

    const char *start = ip_str;
    const char *end = ip_str + strlen(ip_str);

    // Skip head " and tail "
    if ('"' == *start) {
        start++;
    }
    if (end > start && '"' == *(end - 1)) {
        end--;
    }

    int len = end - start;
    if (len <= 0) {
        return 0;
    }

    int dot_count = 0;

    for (int i = 0; i < len; i++) {
        if (start[i] == '.') {
            dot_count++;
        } else if (!isdigit((unsigned char)start[i])) {
            return 0; // Invalid character
        }
    }

    // Valid IPv4 should have exactly 3 dots
    return (dot_count == 3) ? 1 : 0;
}

uint8_t at_utils_is_ipv6(const char *ip_str)
{
    if (ip_str == NULL) {
        return 0;
    }

    // A very basic check for IPv6 format
    const char *p = ip_str;
    int colon_count = 0;

    while (*p) {
        if (*p == ':') {
            colon_count++;
        } else if (!isxdigit((unsigned char)*p) && *p != ':') {
            return 0; // Invalid character
        }
        p++;
    }

    // Valid IPv6 should have at least one colon
    return (colon_count > 0) ? 1 : 0;
}

int at_utils_hex_char_to_byte(const char *hex, size_t hex_len, uint8_t *out_buf, size_t max_len)
{
    if (hex_len % 2 != 0)
        return -1;

    size_t byte_len = hex_len / 2;
    if (byte_len > max_len)
        return -2;

    for (size_t i = 0; i < byte_len; i++) {
        char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        out_buf[i] = (uint8_t)strtoul(byte_str, NULL, 16);
    }

    return (int)byte_len;
}
