/**
 * @file app_mcp.h
 * @brief app_mcp module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_MCP_H__
#define __APP_MCP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_mcp_init(void);

OPERATE_RET app_mcp_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MCP_H__ */
