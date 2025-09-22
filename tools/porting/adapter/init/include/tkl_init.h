/**
 * @file tkl_init.h
 * @brief Common process - tkl init
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_INIT_H__
#define __TKL_INIT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief the description of tuya kernel ability, it will used by auto test tool
 *
 */
typedef struct tkl_ability {
    BOOL_T wifi;
    BOOL_T wired;
    BOOL_T bt;
    BOOL_T zigbee;
    BOOL_T nbiot;
    BOOL_T cellular;
    BOOL_T cellular_mds;
    BOOL_T cellular_sms;
    BOOL_T cellular_vbat;
    BOOL_T flash;
    BOOL_T uart;
    BOOL_T watchdog;
    BOOL_T rtc;
    BOOL_T adc;
    BOOL_T pwm;
    BOOL_T i2c;
    BOOL_T spi;
    BOOL_T gpio;
    BOOL_T timer;
    BOOL_T media;
    BOOL_T display;
    BOOL_T pm;
} TKL_ABILITY_T;

/**
 * @brief tuya kernel adapter layer init
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tkl_init(void);

/**
 * @brief get tuya kernel adapter layer version
 *
 * @return the version of the tuya kernel adapter layer
 *
 */
char *tkl_get_version(void);

/**
 * @brief get tuya kernel adapter layer configure
 *
 * @return the tuyaos kernel ability object
 *
 */
TKL_ABILITY_T *tkl_get_ability(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __TKL_INIT_H__
