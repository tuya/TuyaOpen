/**
 * @file ai_ui_chat_oled.h
 * @brief OLED chat UI interface definitions.
 *
 * This header provides function declarations for registering OLED display
 * chat user interface implementation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_UI_CHAT_OLED_H__
#define __AI_UI_CHAT_OLED_H__

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
 * @brief Register OLED chat UI implementation.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_oled_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_CHAT_OLED_H__ */
