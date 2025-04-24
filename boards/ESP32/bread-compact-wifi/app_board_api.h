/**
 * @file app_board_api.h
 * @brief app_board_api module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __APP_BOARD_API_H__
#define __APP_BOARD_API_H__

#include "tuya_cloud_types.h"
#include "display_common.h"

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

int app_audio_driver_init(const char *name);

void app_display_init(void);

void app_display_set_status(const char *status);

void app_display_show_notification(const char *notification);

void app_display_set_emotion(const char *emotion);

void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content);

void app_display_set_wifi_status(DIS_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BOARD_API_H__ */
