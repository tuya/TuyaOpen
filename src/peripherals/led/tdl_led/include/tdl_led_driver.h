/**
 * @file tdl_led_driver.h
 * @brief LED Driver Interface Header File
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_LED_DRIVER_H__
#define __TDL_LED_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define LED_DEV_NAME_MAX_LEN   32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void* TDD_LED_HANDLE_T;

typedef struct {
    OPERATE_RET (*led_open )(TDD_LED_HANDLE_T dev);
    OPERATE_RET (*led_set  )(TDD_LED_HANDLE_T dev, bool is_on);
    OPERATE_RET (*led_close)(TDD_LED_HANDLE_T dev);
}TDD_LED_INTFS_T;
/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an LED driver with the system.
 * 
 * @param dev_name The name of the device to register.
 * @param handle the LED device handle.
 * @param p_intfs A pointer to the LED driver interface functions.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_driver_register(char *dev_name, TDD_LED_HANDLE_T handle, TDD_LED_INTFS_T *p_intfs);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_LED_DRIVER_H__ */
