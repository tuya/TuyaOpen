/**
 * @file tkl_init_wired.h
 * @brief Common process - tkl init wired description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_WIRED_H__
#define __TKL_INIT_WIRED_H__

#include "tkl_wired.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief the description of tuya kernel adapter layer wired
 *
 */
typedef struct {
    OPERATE_RET (*get_status)(TKL_WIRED_STAT_E *is_up);
    OPERATE_RET (*set_status_cb)(TKL_WIRED_STATUS_CHANGE_CB cb);
    OPERATE_RET (*get_ip)(NW_IP_S *ip);
    OPERATE_RET (*get_ipv6)(NW_IP_TYPE type, NW_IP_S *ip);
    OPERATE_RET (*get_mac)(NW_MAC_S *mac);
    OPERATE_RET (*set_mac)(const NW_MAC_S *mac);
} TKL_WIRED_DESC_T;

/**
 * @brief register wired description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TKL_WIRED_DESC_T *tkl_wired_desc_get(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_INIT_WIRED_H__
