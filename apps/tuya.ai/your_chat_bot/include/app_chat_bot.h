/**
 * @file app_chat_bot.h
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_CHAT_BOT_H__
#define __APP_CHAT_BOT_H__

#include "tuya_cloud_types.h"
#include "ai_chat_main.h"

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
OPERATE_RET app_chat_bot_init(void);

/**
 * @brief Report cloud online/offline (from MQTT connect/disconnect) to the AP keep-alive
 *        power manager. No-op unless ENABLE_TUYA_PM.
 *
 * @param[in] online TRUE when MQTT-connected (AP associated); FALSE when offline.
 */
void app_chat_bot_set_online(BOOL_T online);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CHAT_BOT_H__ */
