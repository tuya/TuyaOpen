/**
 * @file tdl_tp_driver.h
 * @brief Tp device driver interface definitions for TDL layer
 *
 * This header file defines the driver interface structures and function prototypes
 * for the TDL (Tuya Device Library) tp layer. It provides the interface bridge
 * between the TDD (Tuya Device Driver) layer and the TDL management layer, including
 * device registration and interface structure definitions.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_TP_DRIVER_H__
#define __TDL_TP_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_tp_manage.h"
#include "tdd_tp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TP_DEV_NAME_MAX_LEN 32

typedef uint8_t TDD_TP_DRIVER_TYPE_T;
#define TDD_TP_DRIVER_TYPE_I2C 0x01

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDD_TP_DEV_HANDLE_T;

typedef struct {
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E active_level;
} TDD_TP_IO_CTRL_T;

typedef struct {
    uint16_t x_max;
    uint16_t y_max;

    struct {
        uint32_t swap_xy : 1;
        uint32_t mirror_x : 1;
        uint32_t mirror_y : 1;
    } flags;
} TDD_TP_CONFIG_T;

typedef struct {
    OPERATE_RET (*open)(TDD_TP_DEV_HANDLE_T device);
    OPERATE_RET (*read)(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point, uint8_t *point_num);
    OPERATE_RET (*close)(TDD_TP_DEV_HANDLE_T device);
} TDD_TP_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdl_tp_device_register(char *name, TDD_TP_DEV_HANDLE_T tdd_hdl, TDD_TP_CONFIG_T *tp_cfg,
                                      TDD_TP_INTFS_T *intfs);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TP_DRIVER_H__ */
