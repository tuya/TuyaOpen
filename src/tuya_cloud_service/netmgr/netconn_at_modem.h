/**
 * @file netconn_at_modem.h
 * @brief netconn_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETCONN_AT_MODEM_H__
#define __NETCONN_AT_MODEM_H__

#include "tuya_cloud_types.h"

#include "netmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    netmgr_conn_base_t base; // connection base, keep first
} netmgr_conn_at_modem_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief open a at_modem connection
 *
 * @param config: at_modem connection configuration
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_open(void *config);

/**
 * @brief update at_modem connection
 *
 * @param none
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_close(void);

/**
 * @brief update at_modem connection configuration
 *
 * @param cmd: command to update configuration
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_set(netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief get at_modem connection attribute
 *
 * @param cmd: command to get attribute
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_get(netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif /* __NETCONN_AT_MODEM_H__ */
