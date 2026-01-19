/**
 * @file ai_chat_main.h
 * @brief AI chat main module header
 *
 * This header file defines the types and functions for the AI chat main module,
 * which manages the overall AI chat functionality including mode configuration
 * and volume control.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_CHAT_MODE_H__
#define __TUYA_AI_CHAT_MODE_H__

#include "tuya_cloud_types.h"
#include "ai_user_event.h"
#include "ai_manage_mode.h"
#include "lang_config.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_player.h"
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#endif

#if defined(ENABLE_COMP_AI_MODE_HOLD) && (ENABLE_COMP_AI_MODE_HOLD == 1)
#include "ai_mode_hold.h"
#endif
#if defined(ENABLE_COMP_AI_MODE_ONESHOT) && (ENABLE_COMP_AI_MODE_ONESHOT == 1)
#include "ai_mode_oneshot.h"
#endif
#if defined(ENABLE_COMP_AI_MODE_WAKEUP) && (ENABLE_COMP_AI_MODE_WAKEUP == 1)
#include "ai_mode_wakeup.h"
#endif
#if defined(ENABLE_COMP_AI_MODE_FREE) && (ENABLE_COMP_AI_MODE_FREE == 1)
#include "ai_mode_free.h"
#endif

#if defined(ENABLE_COMP_AI_MCP) && (ENABLE_COMP_AI_MCP == 1)
#include "ai_mcp_server.h"
#include "ai_mcp.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    AI_CHAT_MODE_E        default_mode;
    int                   default_vol;
    AI_USER_EVENT_NOTIFY  evt_cb;
}AI_CHAT_MODE_CFG_T;


/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Initialize AI chat module
@param cfg Chat mode configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_chat_init(AI_CHAT_MODE_CFG_T *cfg);

/**
@brief Set chat volume
@param volume Volume value (0-100)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_chat_set_volume(int volume);

/**
@brief Get chat volume
@return int Volume value (0-100)
*/
int ai_chat_get_volume(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AI_CHAT_MODE_H__ */