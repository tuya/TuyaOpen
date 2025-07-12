/**
 * @file tdl_touch_driver.h
 * @brief Touch device driver interface definitions for TDL layer
 *
 * This header file defines the driver interface structures and function prototypes
 * for the TDL (Tuya Device Library) touch layer. It provides the interface bridge
 * between the TDD (Tuya Device Driver) layer and the TDL management layer, including
 * device registration and interface structure definitions.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_TOUCH_DRIVER_H__
#define __TDL_TOUCH_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_touch_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TOUCH_DEV_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDD_TOUCH_DEV_HANDLE_T;

typedef struct {
    OPERATE_RET (*open)(TDD_TOUCH_DEV_HANDLE_T device);
    OPERATE_RET (*read)(TDD_TOUCH_DEV_HANDLE_T device, uint8_t max_num, TDL_TOUCH_POS_T *point, uint8_t *point_num);
    OPERATE_RET (*close)(TDD_TOUCH_DEV_HANDLE_T device);
} TDD_TOUCH_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdl_touch_device_register(char *name, TDD_TOUCH_DEV_HANDLE_T tdd_hdl, TDD_TOUCH_INTFS_T *intfs);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TOUCH_DRIVER_H__ */
