/**
 * @file ai_ui_chat_chatbot.h
 * @brief Chatbot-style chat UI interface definitions.
 *
 * This header provides function declarations for registering chatbot-style
 * chat user interface implementation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_UI_CHAT_CHATBOT_H__
#define __AI_UI_CHAT_CHATBOT_H__

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
 * @brief Register chatbot-style chat UI implementation.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_chatbot_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_CHAT_CHATBOT_H__ */
