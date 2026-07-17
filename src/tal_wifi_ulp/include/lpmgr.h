/**
 * @file lpmgr.h
 * @brief lp manager - Ultra Low Power reference counting manager
 * @version 0.1
 * @date 2024-06-06
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#ifndef _TUYA_LPMGR_H
#define _TUYA_LPMGR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TY_LP_DITM_10 10
#define TY_LP_DITM_20 20
#define TY_LP_DITM_30 30

#define LONG_MAX_TIMEOUT 0xFFFFFFFFFFFFFFFF
#define MAX_TIMEOUT 0xFFFFFFFF
#define MAX_REGISTER_CNT 0xFF

typedef enum {
    TY_LP_NETCFG = 0,
    TY_LP_AP_REDUCE_CONN,
    TY_LP_NET_REDUCE_CONN,
    TY_LP_NETCONN,
    TY_LP_TLSCONN,
    TY_LP_HTTP,
    TY_LP_HTTP_CONN,
    TY_LP_HTTP_SEND,
    TY_LP_MQTT,
    TY_LP_MQTT_CONN,
    TY_LP_MQTT_PUBLISH,
    TY_LP_MQTT_PING,
    TY_LP_TCP_CONN,
    TY_LP_DNS,
    TY_LP_DHCP_RENEW,
    TY_LP_OTA,
    TY_LP_UART,
    TY_LP_I2C,
    TY_LP_SPI,
    TY_LP_PIR,
    TY_LP_CRG,
    TY_LP_BTN,
    TY_LP_PWM,
    TY_LP_KEY,
    TY_LP_NORMAL,
    TY_LP_DEEP,
    TY_LP_APP_USED,
    TY_LP_BLE,
    TY_LP_LONG_NETCFG,
    TY_LP_DISABLE,
    TY_LP_DP,
    TY_LP_MAX
} TY_LP_TYPE;

typedef enum {
    OS_LP_SLEEP_EXPECTED = 0,
    OS_LP_CLOSE,
    OS_LP_SLEEP_200ms = 200,
    OS_LP_SLEEP_1S = 1 * 1000,
    OS_LP_SLEEP_2S = 2 * 1000,
    OS_LP_SLEEP_3S = 3 * 1000,
    OS_LP_SLEEP_3_5S = 3500,
    OS_LP_SLEEP_5S = 5 * 1000,
    OS_LP_SLEEP_10S = 10 * 1000,
    OS_LP_SLEEP_30S = 30 * 1000,
    OS_LP_SLEEP_125S = 125 * 1000,
} SYS_LP_TYPE;

#define OS_LP_SLEEP_MAX OS_LP_SLEEP_30S
#define REDUCE_SLEEP_TIME OS_LP_SLEEP_MAX
#define MIN_TY_LP_REDUCE_SLEEP_TIME(a) ((a) > (REDUCE_SLEEP_TIME / 1000)) ? (REDUCE_SLEEP_TIME / 1000) : (a)

typedef struct {
    const TY_LP_TYPE type;
    SYS_LP_TYPE mcu_lp_type;
    unsigned char dtim; // wifi_lp=1,dtim=0代表不改变dtim值
    uint32_t timeout_range; // timeout_point = timeout_range + current_time, 0xffffffff never timeout
    uint8_t max_cnt;
    uint64_t timeout_point; // 0(disable), timeout_point > current_time enable, 0xffffffffffffffff permanently.
    uint8_t cnt; //同一个类型被注册多次
} lp_info_t;

typedef struct {
    const TY_LP_TYPE type;
    SYS_LP_TYPE mcu_lp_type;
    unsigned char dtim; // wifi_lp=1,dtim=0代表不改变dtim值
    uint32_t timeout_range; // timeout_point = timeout_range + current_time
} user_lp_info_t;

typedef struct {
    unsigned char num;
    user_lp_info_t* lp_modify_info_map;
} user_set_map_t;

/********************************************************************************
 ********************************lpmgr.h*****************************************
 ********************************************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/**
 * @brief unregister CPU or wifi power mode for feature(HTTP_CONN, OTA, MQTT_CONN, UART, U2C, SPI, PWM, KEY)
 *
 * @param[in] type    function module
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_unregister(TY_LP_TYPE type);

/**
 * @brief check if mode(http/mqtt/spi) is registered.
 *
 * @param[in] type    function module
 * @retval 0          not registered
 * @retval Other      registered.
 */
int lpmgr_is_registered(TY_LP_TYPE type);

/**
 * @brief register CPU or wifi power mode for feature(HTTP_CONN, OTA, MQTT_CONN, UART, U2C, SPI, PWM, KEY)
 *
 * @param[in] type    function module
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_register(TY_LP_TYPE type);

/**
 * @brief lpmgr_updata_map_info
 *
 * @param[in] type    function module
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_updata_map_info(TY_LP_TYPE type, uint32_t sleep_time_s, uint32_t* cpu_sleep_time_s);

/**
 * @brief init lpmgr.
 *
 * @param[] no param.
 * @retval no return.
 */
int lpmgr_init(void);

/**
 * @brief show registered power mode and current power type.
 *
 * @param[] no param.
 * @retval no return.
 */
void lpmgr_show_power_mode(void);

/**
 * @brief lp map info table define by user's APP.
 *
 * @param[in] max_dtim   dtim value
 * @param[in] lp_set_info_map lp map info table
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_default_set(unsigned char max_dtim, user_set_map_t* lp_set_info_map);

/**
 * @brief set low power mode enforced
 * @param mcu_lp_type: mcu sleep cycle
 * @param dtim: Delivery Traffic Indication Message; if dtim == 0, close wifi lp
 * @retval 0: success other: fail
 */
int lpmgr_enforce_mode(SYS_LP_TYPE mcu_lp_type, unsigned char dtim);

/**
 * @brief restore current type
 */
void lpmgr_restore_mode(void);

void lpmgr_iot_clear(void);

/**
 * @brief lpmgr_set_connect_info
 *
 * @param[in] type    function module
 * @param[in] state   state
 * @param[in] time    time
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_set_connect_info(TY_LP_TYPE type, uint8_t state, uint32_t time);

/**
 * @brief lpmgr_set_cpu_fix_sleep_time
 *
 * @param[in] type    time_ms
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_set_cpu_fix_sleep_time(uint32_t time_ms);

/**
 * @brief lpmgr_get_cpu_max_sleep_time
 *
 * @param[out] type   cpu_sleep_time_ms
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_get_cpu_max_sleep_time(uint32_t* cpu_sleep_time_ms);

/**
 * @brief set lps dtim value to TuyaOS SDK
 *
 * @param[in] dtim   dtim value
 * @retval 0          success
 * @retval Other      fail
 */
int lpmgr_set_lps_dtim(uint32_t dtim);

#ifdef __cplusplus
}
#endif

#endif /* _TUYA_LPMGR_H */
