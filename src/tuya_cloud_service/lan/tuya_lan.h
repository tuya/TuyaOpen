/**
 * @file tuya_lan.h
 * @brief Header file for Tuya LAN communication protocol definitions and
 * utilities.
 *
 * This header file contains definitions for frame types used in the Tuya LAN
 * communication protocol, including network configuration, command
 * transmission, status reporting, heartbeat, and more. It also defines security
 * types for encrypted communication. The constants and structures defined
 * herein are used for managing LAN-based interactions between Tuya IoT devices
 * and the Tuya cloud platform, facilitating secure and efficient local network
 * communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_LAN_H__
#define __TUYA_LAN_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"
#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

// LAN protocol frame type
#define FRM_TP_CFG_WF       0x01 // only used for ap 3.0 network config
#define FRM_TP_CMD          0x07 //
#define FRM_TP_STAT_REPORT  0x08 //
#define FRM_TP_HB           0x09 //
#define FRM_QUERY_STAT      0x0a //
#define FRM_SSID_QUERY      0x0b // discard
#define FRM_USER_BIND_REQ   0x0c // {"token","yyyyyyyyyyyyyy"}
#define FRM_TP_NEW_CMD      0x0d //
#define FRM_ADD_SUB_DEV_CMD 0x0e //
#define FRM_CFG_WIFI_INFO   0x0f // {"ssid":"xxxx","passwd":"yyyyy"}
#define FRM_QUERY_STAT_NEW  0x10 //
#define FRM_SCENE_EXEC      0x11 //
#define FRM_LAN_QUERY_DP    0x12 //

#define FRM_SECURITY_TYPE3 0x03
#define FRM_SECURITY_TYPE4 0x04
#define FRM_SECURITY_TYPE5 0x05

#define FRM_TYPE_REG_CENTER        0x24
#define FRM_TYPE_APP_UDP_BOARDCAST 0x25

#define FRM_LAN_EXT_STREAM          0x40
#define FRM_LAN_EXT_BEFORE_ACTIVATE 0x42
#define FRM_LAN_UPD_LOG             0x30

/**
 * @brief Init and start LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_init(tuya_iot_client_t *client);

/**
 * @brief Stop and uninit LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_exit(void);

/**
 * @brief Disable LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_disable(void);

/**
 * @brief Enable LAN service
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_enable(void);

/**
 * @brief LAN dp report
 *
 * @param[in] data data buf
 * @param[in] len buf length
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
// int tuya_lan_dp_report(IN void *data, const uint32_t len);

/**
 * @brief distribute data to all connections
 *
 * @param[in] fr_type refer to LAN_PRO_HEAD_APP_S
 * @param[in] ret_code refer to LAN_PRO_HEAD_APP_S
 * @param[in] data refer to LAN_PRO_HEAD_APP_S
 * @param[in] len refer to LAN_PRO_HEAD_APP_S
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_data_report(uint32_t fr_type, uint32_t ret_code, uint8_t *data, uint32_t len);

/**
 * @brief disconnect all connections
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_disconnect_all(void);

/**
 * @brief lan cmd extersion
 *
 * @param[in] data data of frame type
 * @param[out] out data to send back (shall be string const)
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
typedef int (*lan_cmd_handler_cb)(const uint8_t *data, uint8_t **out);

/**
 * @brief register callback
 *
 * @param[in] frame_type refer to LAN_PRO_HEAD_APP_S
 * @param[in] frame_type refer to lan_cmd_handler_cb
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_register_cb(uint32_t frame_type, lan_cmd_handler_cb handler);

/**
 * @brief unregister callback
 *
 * @param[in] frame_type refer to LAN_PRO_HEAD_APP_S
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tuya_lan_unregister_cb(uint32_t frame_type);

/**
 * @brief get lan client number
 *
 * @return client number
 */
uint32_t tuya_lan_get_client_num(void);
int tuya_lan_get_connect_client_num(void);

int tuya_lan_data_com_send(const int32_t socket, const uint32_t fr_num, const uint32_t fr_type, const uint32_t ret_code,
                           const uint8_t *data, const uint32_t len);

int tuya_lan_dp_report(char *dpstr);

/**
 * @brief judge if lan connect
 *
 * @return TRUE/FALSE
 */
#define tuya_lan_is_connected() (tuya_lan_get_connect_client_num() != 0)

#ifdef __cplusplus
}
#endif
#endif //__TUYA_SVC_LAN_H__
