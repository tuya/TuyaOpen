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
#define APP_CHAT_BOT_WORK_MODE_HOLD     0
#define APP_CHAT_BOT_WORK_MODE_ONE_SHOT 1
// TODO: wait add
#define APP_CHAT_BOT_WORK_MODE_FREE 2

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_chat_bot_init(void);

OPERATE_RET app_chat_bot_volume_set(uint8_t volume);

uint8_t app_chat_bot_volume_get(void);

APP_WORK_MODE_E app_chat_bot_get_work_mode(void);

OPERATE_RET app_chat_bot_enable_set(uint8_t enable);

uint8_t app_chat_bot_is_enable(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CHAT_BOT_H__ */
