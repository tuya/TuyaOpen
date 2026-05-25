/**
 * @file otto_mcp_tools.h
 * @brief Otto robot MCP tool registration
 * @version 1.0
 * @date 2025-05-22
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __OTTO_MCP_TOOLS_H__
#define __OTTO_MCP_TOOLS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register Otto robot MCP tools after MQTT connects
 * @return OPRT_OK on success
 * @note Subscribe after ai_mcp_init(); tools are added when MCP server is ready
 */
OPERATE_RET otto_mcp_tools_init(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /* __OTTO_MCP_TOOLS_H__ */
