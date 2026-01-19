/**
 * @file smart_frame.h
 * @brief smart_frame module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __SMART_FRAME_H__
#define __SMART_FRAME_H__

#include "tuya_cloud_types.h"
#include "dp_schema.h"

#include "tuya_ai_types.h"
#include "ty_cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/**
 * @brief tuya sdk dp cmd type
 */
typedef dp_cmd_type_t DP_CMD_TYPE_E;

/**
 * @brief info of dp command
 */
typedef struct {
    DP_CMD_TYPE_E tp; //command source
    ty_cJSON *cmd_js; //command content
} SF_GW_DEV_CMD_S;

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Send dp command
 *
 * @param[in] gd_cmd: dp command information
 *
 * @note This API is used for sending dp command from mqtt/lan/bt
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET sf_send_gw_dev_cmd(IN SF_GW_DEV_CMD_S *gd_cmd);

#ifdef __cplusplus
}
#endif

#endif /* __SMART_FRAME_H__ */
