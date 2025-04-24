/**
 * @file oled_display.h
 * @brief oled_display module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __OLED_DISPLAY_H__
#define __OLED_DISPLAY_H__

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

int oled_ssd1306_init(int scl, int sda, int width, int height);

void oled_setup_ui_128x32(void);

void oled_setup_ui_128x64(void);

void oled_set_status(const char *status);

void oled_show_notification(const char *notification);

void oled_set_emotion(const char *emotion);

void oled_set_chat_message(CHAT_ROLE_E role, const char *content);

void oled_set_wifi_status(DIS_WIFI_STATUS_E status);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_DISPLAY_H__ */
