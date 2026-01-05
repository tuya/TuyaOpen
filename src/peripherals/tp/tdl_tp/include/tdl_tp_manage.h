/**
 * @file tdl_tp_manage.h
 * @brief Tp device management layer interface definitions
 *
 * This header file defines the TDL (Tuya Device Library) layer interface for tp
 * device management. It provides high-level API definitions for tp device operations
 * including device discovery, opening, reading tp coordinates, and closing operations.
 * This layer abstracts the underlying TDD drivers and provides a unified interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_TP_MANAGE_H__
#define __TDL_TP_MANAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TDL_TP_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t x;
    uint16_t y;
} TDL_TP_POS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
TDL_TP_HANDLE_T tdl_tp_find_dev(char *name);

OPERATE_RET tdl_tp_dev_open(TDL_TP_HANDLE_T tp_hdl);

OPERATE_RET tdl_tp_dev_read(TDL_TP_HANDLE_T tp_hdl, uint8_t max_num, TDL_TP_POS_T *point,
                               uint8_t *point_num);

OPERATE_RET tdl_tp_dev_close(TDL_TP_HANDLE_T tp_hdl);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TP_MANAGE_H__ */
