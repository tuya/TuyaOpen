/**
 * @file tkl_init_wired.c
 * @brief Common process - tkl init wired description
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_init_wired.h"

const TKL_WIRED_DESC_T c_wired_desc = {
    .get_status = tkl_wired_get_status,
    .set_status_cb = tkl_wired_set_status_cb,
    .get_ip = tkl_wired_get_ip,
    .get_mac = tkl_wired_get_mac,
    .set_mac = tkl_wired_set_mac,
};

/**
 * @brief register wired description to tuya object manage
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE TKL_WIRED_DESC_T *tkl_wired_desc_get()
{
    return (TKL_WIRED_DESC_T *)&c_wired_desc;
}
