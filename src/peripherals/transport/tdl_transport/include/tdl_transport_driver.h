/**
 * @file tdl_transport_driver.h
 * @brief tdl_transport_driver module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_TRANSPORT_DRIVER_H__
#define __TDL_TRANSPORT_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t TDL_TRANSPORT_CMD_T;
#define TDL_TRANSPORT_CMD_RX_BUFFER_RESET 0x01 // Reset RX buffer command

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDD_TRANSPORT_HANDLE_T;

typedef struct {
    OPERATE_RET (*open)(TDD_TRANSPORT_HANDLE_T handle);
    OPERATE_RET (*send)(TDD_TRANSPORT_HANDLE_T handle, const uint8_t *data, uint32_t len);
    uint32_t (*read)(TDD_TRANSPORT_HANDLE_T handle, uint8_t *data, uint32_t len);
    uint32_t (*available)(TDD_TRANSPORT_HANDLE_T handle);
    OPERATE_RET (*config)(TDD_TRANSPORT_HANDLE_T handle, TDL_TRANSPORT_CMD_T cmd, void *param);
    OPERATE_RET (*close)(TDD_TRANSPORT_HANDLE_T handle);
} TDD_TRANSPORT_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdl_transport_driver_register(char *name, TDD_TRANSPORT_INTFS_T *intfs, TDD_TRANSPORT_HANDLE_T tdd_hdl);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TRANSPORT_DRIVER_H__ */
