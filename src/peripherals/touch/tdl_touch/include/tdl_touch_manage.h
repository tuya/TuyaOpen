/**
 * @file tdl_touch_manage.h
 * @brief Touch device management layer interface definitions
 *
 * This header file defines the TDL (Tuya Device Library) layer interface for touch
 * device management. It provides high-level API definitions for touch device operations
 * including device discovery, opening, reading touch coordinates, and closing operations.
 * This layer abstracts the underlying TDD drivers and provides a unified interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_TOUCH_MANAGE_H__
#define __TDL_TOUCH_MANAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TDL_TOUCH_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t x;
    uint16_t y;
} TDL_TOUCH_POS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
TDL_TOUCH_HANDLE_T tdl_touch_find_dev(char *name);

OPERATE_RET tdl_touch_dev_open(TDL_TOUCH_HANDLE_T touch_hdl);

OPERATE_RET tdl_touch_dev_read(TDL_TOUCH_HANDLE_T touch_hdl, uint8_t max_num, TDL_TOUCH_POS_T *point,
                               uint8_t *point_num);

OPERATE_RET tdl_touch_dev_close(TDL_TOUCH_HANDLE_T touch_hdl);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TOUCH_MANAGE_H__ */
