/**
 * @file ai_ui_chat_wechat.h
 * @brief WeChat-style chat UI interface definitions.
 *
 * This header provides function declarations for registering WeChat-style
 * chat user interface implementation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_CHAT_UI_WECHAT_H__
#define __AI_CHAT_UI_WECHAT_H__

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
 * @brief Register WeChat-style chat UI implementation.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_wechat_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_UI_WECHAT_H__ */
