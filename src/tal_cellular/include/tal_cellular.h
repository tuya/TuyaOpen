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
#define TAL_CELLULAR_CCID_LEN        20
#define TAL_CELLULAR_USER_NAME_LEN   32
#define TAL_CELLULAR_USER_PASSWD_LEN 32
#define TAL_CELLULAR_DIAL_UP_CMD_LEN 32
#define TAL_CELLULAR_IMEI_LEN        15
#define TAL_CELLULAR_SN_LEN          10
#define TAL_CELLULAR_SW_VER_LEN      40 ///< 40, not the 22-char version string: the TKL layer writes all 40

typedef enum {
    TAL_CELLULAR_LINK_DOWN = 0, ///< the network cable is unplugged
    TAL_CELLULAR_LINK_UP,       ///< the network cable is plugged and IP is got
} TAL_CELLULAR_STAT_E;

typedef enum {
    TAL_CELLULAR_NO_SLEEP = 0,
    TAL_CELLULAR_IDLE,
    TAL_CELLULAR_MODE1,
    TAL_CELLULAR_MODE2,
    TAL_CELLULAR_HIBERNATE,
} TAL_CELLULAR_SLEEP_MODE_E;

typedef struct {
    char apn[TAL_CELLULAR_APN_LEN + 1]; ///< Access Point Name
    TUYA_CELLULAR_IF_E iface;
    TUYA_CELLULAR_PROTOCOL_E protocol;
    TAL_CELLULAR_SLEEP_MODE_E sleep_mode;
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

/**
 * @brief  get the ICCID of the SIM card
 *
 * @param[out]  ccid: the ICCID string, buffer must be TAL_CELLULAR_CCID_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_ccid(char *ccid);

/**
 * @brief  get the signal strength of the cellular link
 *
 * @param[out]  rssi: the AT+CSQ RSSI index, 0..31, or 99 when unknown.
 *                    Note the TKL layer types this as char *, but it is a single
 *                    scalar byte rather than a string.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_rssi(uint8_t *rssi);

/**
 * @brief  get the supply voltage of the cellular module, in millivolts
 *
 * @param[out]  volt: the voltage value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_volt(uint32_t *volt);

/**
 * @brief  get the IMEI of the cellular module
 *
 * @param[out]  imei: the IMEI string, buffer must be TAL_CELLULAR_IMEI_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_imei(char *imei);

/**
 * @brief  get the serial number of the cellular module
 *
 * @param[out]  sn: the serial number string, buffer must be TAL_CELLULAR_SN_LEN + 1 bytes
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_sn(char *sn);

/**
 * @brief  get the firmware version of the cellular module
 *
 * @param[out]  ver: the version string, buffer must be TAL_CELLULAR_SW_VER_LEN + 1 bytes.
 *                   The module reports at most 22 characters, but tkl_cellular_get_sw_ver()
 *                   strncpy()s a full TAL_CELLULAR_SW_VER_LEN bytes and NUL-pads the rest,
 *                   so a buffer sized to the string length instead overflows.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_cellular_get_sw_ver(char *ver);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_CELLULAR_H__ */
