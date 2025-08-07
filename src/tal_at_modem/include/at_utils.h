/**
 * @file at_utils.h
 * @brief at_utils module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_UTILS_H__
#define __AT_UTILS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

TUYA_IP_ADDR_T at_utils_str2addr(const char *ip_str);

char *at_utils_addr2str(TUYA_IP_ADDR_T ipaddr);

uint8_t at_utils_is_ipv4(const char *ip_str);

uint8_t at_utils_is_ipv6(const char *ip_str);

int at_utils_hex_char_to_byte(const char *hex, size_t hex_len, uint8_t *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __AT_UTILS_H__ */
