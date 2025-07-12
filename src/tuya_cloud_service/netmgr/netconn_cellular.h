/**
 * @file netconn_cellular.c
 * @brief netconn_cellular module is used to manage cellular network connections.
 *
 * This file provides the implementation of the netconn_cellular module,
 * which is responsible for managing cellular network connections.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-10   yangjie     Initial version.
 */

#ifndef __NETCONN_CELLULAR_H__
#define __NETCONN_CELLULAR_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "tal_cellular.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CELLULAR_STAT_E TAL_CELLULAR_STAT_E

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    netmgr_conn_base_t base; // connection base, keep first
} netmgr_conn_cellular_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief open a cellular connection
 *
 * @param config: cellular connection configuration
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_cellular_open(void *config);

/**
 * @brief update cellular connection
 *
 * @param none
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_cellular_close(void);

/**
 * @brief update cellular connection configuration
 *
 * @param cmd: command to update configuration
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_cellular_set(netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief get cellular connection attribute
 *
 * @param cmd: command to get attribute
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_cellular_get(netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif /* __NETCONN_CELLULAR_H__ */
