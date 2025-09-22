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

#ifndef __TAL_CELLULAR_H__
#define __TAL_CELLULAR_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TAL_CELLULAR_APN_LEN         64
#define TAL_CELLULAR_USER_NAME_LEN   32
#define TAL_CELLULAR_USER_PASSWD_LEN 32
#define TAL_CELLULAR_DIAL_UP_CMD_LEN 32

typedef enum {
    TAL_CELLULAR_LINK_DOWN = 0, ///< the network cable is unplugged
    TAL_CELLULAR_LINK_UP,       ///< the network cable is plugged and IP is got
} TAL_CELLULAR_STAT_E;

typedef struct {
    char apn[TAL_CELLULAR_APN_LEN + 1]; ///< Access Point Name
} TAL_CELLULAR_BASE_CFG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief callback function: CELLULAR_STATUS_CHANGE_CB
 *        when cellular connect status changed, notify tuyaos
 *        with this callback.
 *
 * @param stat: the cellular connect status
 *              - TAL_CELLULAR_LINK_DOWN: the network cable is unplugged
 *              - TAL_CELLULAR_LINK_UP: the network cable is plugged and IP is got
 */
typedef void (*TAL_CELLULAR_STATUS_CHANGE_CB)(TAL_CELLULAR_STAT_E stat);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief  init create cellular link
 *
 * @param[in]   cfg: the configure for cellular link
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_init(TAL_CELLULAR_BASE_CFG_T *cfg);

/**
 * @brief  get the link status of celluar link
 *
 * @param[out]  stat: the celluar link status
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_status(TAL_CELLULAR_STAT_E *stat);

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_set_status_cb(TAL_CELLULAR_STATUS_CHANGE_CB cb);

/**
 * @brief  get the ip address of the cellular link
 *
 * @param[out]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_ip(NW_IP_S *ip);

/**
 * @brief  get the ipv6 address of the cellular link
 *
 * @param[in]   type: the ipv6 address type
 * @param[out]  ip: the ipv6 address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_ipv6(NW_IP_TYPE type, NW_IP_S *ip);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_CELLULAR_H__ */
