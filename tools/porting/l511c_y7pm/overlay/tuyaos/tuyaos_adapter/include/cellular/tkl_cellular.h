/**
 * @file tkl_cellular.h
 * @brief Common process - adapter the cellular api
 * @version 0.1
 * @date 2025-04-21
 *
 * @copyright Copyright 2020-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_CELLULAR_H__
#define __TKL_CELLULAR_H__

#include "tuya_cloud_types.h"
#include "l511c_cellular_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tuyaos definition of cellular network status */
typedef enum {
    TKL_CELLULAR_LINK_DOWN = 0, ///< the network cable is unplugged
    TKL_CELLULAR_LINK_UP,       ///< the network cable is plugged and IP is got
} TKL_CELLULAR_STAT_E;

#define TKL_CELLULAR_APN_LEN         64
#define TKL_CELLULAR_CCID_LEN        20
#define TKL_CELLULAR_USER_NAME_LEN   32
#define TKL_CELLULAR_USER_PASSWD_LEN 32
#define TKL_CELLULAR_DIAL_UP_CMD_LEN 32

#define TKL_CELLULAR_IMEI_LEN   15
#define TKL_CELLULAR_SN_LEN     10
#define TKL_CELLULAR_SW_VER_LEN 22

typedef struct {
    char apn[TKL_CELLULAR_APN_LEN + 1]; ///< Access Point Name
    //  char username[TKL_CELLULAR_USER_NAME_LEN+1];                    ///< User Name
    //  char password[TKL_CELLULAR_USER_PASSWD_LEN+1];                  ///< User Password
    //  char dial_up_phone_num[TKL_CELLULAR_DIAL_UP_CMD_LEN+1];         ///< dial-up phone number
    TUYA_CELLULAR_IF_E       iface;
    TUYA_CELLULAR_PROTOCOL_E protocol;
} TKL_CELLULAR_BASE_CFG_T;

/**
 * @brief  init create cellular link
 *
 * @param[in]   cfg: the configure for cellular link
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_init(TKL_CELLULAR_BASE_CFG_T *cfg);

/**
 * @brief callback function: TKL_CELLULAR_STATUS_CHANGE_CB
 *        when cellular connect status changed, notify tuyaos
 *        with this callback.
 *
 * @param[out]       is_up         the cellular link status is up or not
 */
typedef void (*TKL_CELLULAR_STATUS_CHANGE_CB)(TKL_CELLULAR_STAT_E status);

/**
 * @brief  get the link status of cellular link
 *
 * @param[out]  is_up: the cellular link status is up or not
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_status(TKL_CELLULAR_STAT_E *status);

/**
 * @brief  set the status change callback
 *
 * @param[in]   cb: the callback when link status changed
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_set_status_cb(TKL_CELLULAR_STATUS_CHANGE_CB cb);

/**
 * @brief  get the ip address of the cellular link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_ip(NW_IP_S *ip);

/**
 * @brief  get the ip address of the cellular link
 *
 * @param[in]   ip: the ip address
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_ipv6(NW_IP_TYPE type, NW_IP_S *ip);

/**
 * @brief  get the ccid of the cellular link
 *
 * @param[out]   ccid: ccid string
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_ccid(char *ccid);

/**
 * @brief  get the rssi of the cellular link
 *
 * @param[out]   rssi: rssi value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_rssi(char *rssi);

/**
 * @brief  get the voltage of the cellular module
 *
 * @param[out]   volt: voltage value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_volt(uint32_t *volt);

/**
 * @brief  get the IMEI of the cellular module
 *
 * @param[out]   imei: IMEI value
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_imei(char *imei);

/**
 * @brief  get the Serial Number of the cellular module
 *
 * @param[out]   sn: Serial Number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_sn(char *sn);

/**
 * @brief  get the Software version of the cellular module
 *
 * @param[out]   ver: Software version
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_get_sw_ver(char *ver);

/**
 * @brief  start cellular mf test
 *
 * @param[in]   cfg: the configure for cellular mf test
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_mf_test_start(TKL_CELLULAR_BASE_CFG_T *cfg);

/**
 * @brief  stop cellular mf test
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_cellular_mf_test_stop(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_CELLULAR_H__