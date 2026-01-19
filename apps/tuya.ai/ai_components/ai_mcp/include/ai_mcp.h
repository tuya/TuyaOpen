/**
 * @file ai_mcp.h
 * @brief AI MCP (Model Context Protocol) module header
 *
 * This header file defines the functions for initializing and deinitializing
 * the MCP module, which provides tool discovery and execution capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MCP_H__
#define __AI_MCP_H__

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
/**
@brief Initialize MCP module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mcp_init(void);

/**
@brief Deinitialize MCP module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mcp_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MCP_H__ */
