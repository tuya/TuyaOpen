/**
 * @file tdl_transport_manage.h
 * @brief tdl_transport_manage module is used to manage transport layer
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_TRANSPORT_MANAGE_H__
#define __TDL_TRANSPORT_MANAGE_H__

#include "tuya_cloud_types.h"
#include "tdl_transport_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_TRANSPORT_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
// transport handler
typedef void *TDL_TRANSPORT_HANDLE;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdl_transport_find(const char *name, TDL_TRANSPORT_HANDLE *handle);

OPERATE_RET tdl_transport_open(TDL_TRANSPORT_HANDLE handle);

OPERATE_RET tdl_transport_send(TDL_TRANSPORT_HANDLE handle, const uint8_t *data, uint32_t len);

uint32_t tdl_transport_read(TDL_TRANSPORT_HANDLE handle, uint8_t *data, uint32_t len);

uint32_t tdl_transport_available(TDL_TRANSPORT_HANDLE handle);

OPERATE_RET tdl_transport_config(TDL_TRANSPORT_HANDLE handle, TDL_TRANSPORT_CMD_T cmd, void *param);

OPERATE_RET tdl_transport_close(TDL_TRANSPORT_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_TRANSPORT_MANAGE_H__ */
