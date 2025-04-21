/**
 * @file app_chat_bot.h
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_CHAT_BOT_H__
#define __APP_CHAT_BOT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t APP_WORK_MODE_E;
// Press and hold button to talk.
#define APP_CHAT_BOT_WORK_MODE_HOLD     0  
// Press the button once to start or stop the free conversation.
#define APP_CHAT_BOT_WORK_MODE_ONE_SHOT 1 
/*Say the wake-up word to initiate a single conversation, similar to a smart speaker. 
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_BOT_WORK_MODE_WAKEUP   2 
/*Saying the wake-up word, you can have a free conversation.
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_BOT_WORK_MODE_FREE     3

#define APP_CHAT_BOT_WORK_MODE_MAX      4
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_chat_bot_init(void);

APP_WORK_MODE_E app_chat_bot_get_work_mode(void);

OPERATE_RET app_chat_bot_enable_set(uint8_t enable);

uint8_t app_chat_bot_is_enable(void);

OPERATE_RET app_chat_bot_set_next_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CHAT_BOT_H__ */
