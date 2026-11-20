/**
 * @file netconn_wired.c
 * @brief Implementation of wired network connection management for Tuya
 * devices.
 *
 * This file provides the implementation for managing wired network connections
 * on Tuya devices, including opening and closing connections, getting and
 * setting network parameters, and handling network events. It utilizes the TAL
 * for wired network communication and integrates with the MQTT binding for
 * network event notifications.
 *
 * The wired network connection management is essential for devices that support
 * Ethernet connectivity, ensuring reliable and stable network communication for
 * Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __NETCONN_WIRED_H___
#define __NETCONN_WIRED_H___

#include "tuya_cloud_types.h"
#include "tal_wired.h"
#include "netmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief the wired connect object
 *
 */
typedef struct {
    netmgr_conn_base_t base;
} netmgr_conn_wired_t;

/**
 * @brief open wired connection
 *
 * @param config: wired connection config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_open(void *config);

/**
 * @brief close wired connection
 *
 * @param config: wired connection config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_close(void);

/**
 * @brief update wired connection
 *
 * @param config: the new config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_set(netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief get wired connection attribte
 *
 * @param type
 * @param cmd
 * @param param
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_get(netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif
