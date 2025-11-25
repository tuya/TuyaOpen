/**
 * @file tal_cellular.c
 * @brief tal_cellular module is used to manage cellular network connections.
 *
 * This file provides the implementation of the tal_cellular module,
 * which is responsible for managing cellular network connections.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-10   yangjie     Initial version.
 */

#include "tal_cellular.h"

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)

#include "tkl_init_cellular.h"
#include "tkl_cellular.h"

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
static TKL_CELLULAR_DESC_T *sg_cellular = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET tal_cellular_init(TAL_CELLULAR_BASE_CFG_T *cfg)
{
    if (sg_cellular == NULL) {
        sg_cellular = tkl_cellular_desc_get();
    }

    if (sg_cellular == NULL || sg_cellular->init == NULL) {
        return OPRT_NOT_SUPPORTED;
    }

    return sg_cellular->init((TKL_CELLULAR_BASE_CFG_T *)cfg);
}

OPERATE_RET tal_cellular_get_status(TAL_CELLULAR_STAT_E *stat)
{
    if (sg_cellular == NULL || sg_cellular->get_status == NULL) {
        return OPRT_NOT_SUPPORTED;
    }

    return sg_cellular->get_status((TKL_CELLULAR_STAT_E *)stat);
}

OPERATE_RET tal_cellular_set_status_cb(TAL_CELLULAR_STATUS_CHANGE_CB cb)
{
    if (sg_cellular == NULL || sg_cellular->set_status_cb == NULL) {
        return OPRT_NOT_SUPPORTED;
    }

    return sg_cellular->set_status_cb((TKL_CELLULAR_STATUS_CHANGE_CB)cb);
}

OPERATE_RET tal_cellular_get_ip(NW_IP_S *ip)
{
    if (sg_cellular == NULL || sg_cellular->get_ip == NULL) {
        return OPRT_NOT_SUPPORTED;
    }

    return sg_cellular->get_ip((NW_IP_S *)ip);
}

OPERATE_RET tal_cellular_get_ipv6(NW_IP_TYPE type, NW_IP_S *ip)
{
    if (sg_cellular == NULL || sg_cellular->get_ipv6 == NULL) {
        return OPRT_NOT_SUPPORTED;
    }

    return sg_cellular->get_ipv6((NW_IP_TYPE)type, (NW_IP_S *)ip);
}

#endif // defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
