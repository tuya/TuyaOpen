
/**
 * @file netcfg.h
 * @brief Header file for network configuration management in Tuya devices.
 *
 * This file defines the constants, enums, and structures for managing various
 * network configuration modes such as Tuya WiFi AP, BLE, and others. It
 * includes the definition of the netcfg_type_t enum, which represents different
 * network configuration types that can be registered and managed within the
 * Tuya ecosystem. Additionally, it provides the foundation for network
 * configuration actions like stopping all configuration modules, setting
 * default SSID for AP mode, and handling token lengths for secure
 * communication.
 *
 * The network configuration management is crucial for initializing and setting
 * up Tuya devices to connect to different types of networks, ensuring seamless
 * communication with Tuya's cloud services and other devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __NETCFG__
#define __NETCFG__

#include "tal_wifi.h"
#include "tuya_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETCFG_STOP_ALL_CFG_MODULE (0)
#define TUYA_AP_SSID_DEFAULT       "SmartLife"
#define WL_TOKEN_LEN               (16)

typedef enum {
    NETCFG_TUYA_WIFI_AP      = 1 << 0,
    NETCFG_TUYA_BLE          = 1 << 1,
    NETCFG_TUYA_API_USER     = 1 << 2,
    NETCFG_TUYA_WIFI_PEGASUS = 1 << 3,
    NETCFG_AMAZON_WIFI_FFS   = 1 << 4,
} netcfg_type_t;

typedef struct {
    netcfg_type_t type;
    char         *uuid;
    char         *pincode;
} netcfg_args_t;

/**
 * @brief ssid password and token info
 */

typedef struct {
    /** ssid array */
    uint8_t ssid[WIFI_SSID_LEN + 1];
    /** length of ssid array */
    uint8_t s_len;
    /** password array */
    uint8_t passwd[WIFI_PASSWD_LEN + 1];
    /** length of password array */
    uint8_t p_len;
    /** token array */
    uint8_t token[WL_TOKEN_LEN + 1];
    /** length of token array */
    uint8_t t_len;
} netcfg_info_t;

/**
 * @brief netcfg finished callback function.
 *
 * @param[in] type refer to netcfg type definitions
 * @param[in] netcfg_info_t netcfg info: ssid,passwd,token
 *
 * @note netcfg finished callback function.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
typedef int (*netcfg_finish_cb_t)(int type, netcfg_info_t *info);

/**
 * @brief netcfg start function.
 *
 * @param[in] cb netcfg finished callback function
 * @param[in] args args for start
 * @param[in] type refer to netcfg type definitions
 *
 * @note netcfg start function.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
typedef int (*netcfg_start_cb_t)(int type, netcfg_finish_cb_t cb, void *args);

/**
 * @brief netcfg stop function.
 *
 * @param[in] type refer to netcfg type definitions
 *
 * @note netcfg stop function.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
typedef int (*netcfg_stop_cb_t)(int type);

/**
 * @brief netcfg handler
 */

typedef struct {
    /** node for list */
    LIST_HEAD node;
    /** netcfg type */
    int type;
    /** netcfg start function */
    netcfg_start_cb_t start;
    /** netcfg stop function */
    netcfg_stop_cb_t stop;
    /** netcfg finished function */
    netcfg_finish_cb_t netcfg_finish_cb;
    /** args for netcfg_finish_cb */
    void *args;
    /** netcfg is started or not */
    bool isStarted;
} netcfg_handler_t;

/**
 * @brief init netcfg module.
 *
 * @note init netcfg module.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_init(void);

/**
 * @brief uninit netcfg module.
 *
 *
 * @note release netcfg module resources.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_uninit(void);

/**
 * @brief register netcfg type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions
 * @param[in] start callback function to start netcfg
 * @param[in] stop callback function to stop netcfg
 *
 * @note register netcfg module start and stop callback.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_register(int type, netcfg_start_cb_t start, netcfg_stop_cb_t stop);

/**
 * @brief unregister netcfg type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions
 *
 * @note register netcfg module start and stop callback.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_unregister(int type);

/**
 * @brief start netcfg type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions
 * @param[in] netcfg_finish_cb callback function when netcfg finished
 * @param[in] args private data for netcfg_finish_cb
 *
 * @note start netcfg type.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int netcfg_start(int type, netcfg_finish_cb_t netcfg_finish_cb, void *args);

/**
 * @brief stop netcfg type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions 0 : stop all
 * netcfg type
 *
 * @note stop netcfg type.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_stop(int type);

/**
 * @brief stop all netcfg type except the specified type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions 0 : stop all
 * netcfg type
 *
 * @note stop all netcfg type except the specified type.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_stop_other_all(int type);

/**
 * @brief start all netcfg type except the specified type
 *
 * @param[in] type netcfg type,refer to netcfg type definitions 0 : stop all
 * netcfg type
 *
 * @note start all netcfg type except the specified type.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */

int netcfg_start_other_all(int type);

#ifdef __cplusplus
}
#endif

#endif
