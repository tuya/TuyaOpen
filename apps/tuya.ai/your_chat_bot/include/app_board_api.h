/**
 * @file app_board_api.h
 * @brief app_board_api module is used to
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __APP_BOARD_API_H__
#define __APP_BOARD_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef uint8_t CHAT_ROLE_E;
#define CHAT_ROLE_USER      0
#define CHAT_ROLE_ASSISTANT 1
#define CHAT_ROLE_SYSTEM    2

// display network status
typedef uint8_t DIS_WIFI_STATUS_E;
#define DIS_WIFI_STATUS_DISCONNECTED 0
#define DIS_WIFI_STATUS_GOOD         1
#define DIS_WIFI_STATUS_FAIR         2
#define DIS_WIFI_STATUS_WEAK         3

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

int app_audio_driver_init(const char *name);

int app_display_init(void);

void app_display_set_status(const char *status);

void app_display_show_notification(const char *notification);

void app_display_set_emotion(const char *emotion);

void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content);

void app_display_set_wifi_status(DIS_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BOARD_API_H__ */
