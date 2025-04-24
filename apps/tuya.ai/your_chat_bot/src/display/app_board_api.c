/**
 * @file app_board_api.c
 * @brief app_board_api module is used to
 * @version 0.1
 * @date 2025-04-24
 */

#include "app_board_api.h"

#include "tuya_display.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

int app_display_init(void)
{
    return tuya_display_init();
}

void app_display_set_status(const char *status)
{
    if (NULL == status) {
        return;
    }

    if (strcmp(status, "STANDBY") == 0) {
        tuya_display_send_msg(TY_DISPLAY_TP_STAT_IDLE, NULL, 0);
    } else if (strcmp(status, "LISTEN") == 0) {
        tuya_display_send_msg(TY_DISPLAY_TP_STAT_LISTEN, NULL, 0);
    }

    return;
}

void app_display_show_notification(const char *notification)
{
    return;
}

void app_display_set_emotion(const char *emotion)
{
    return;
}

void app_display_set_chat_massage(CHAT_ROLE_E role, const char *content)
{
    if (NULL == content) {
        return;
    }

    switch (role) {
    case CHAT_ROLE_USER: {
        tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, content, strlen(content));
    } break;
    case CHAT_ROLE_ASSISTANT: {
        tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, content, strlen(content));
    } break;
    case CHAT_ROLE_SYSTEM: {
        if (strcmp(content, "Device Online") == 0) {
            tuya_display_send_msg(TY_DISPLAY_TP_STAT_ONLINE, NULL, 0);
        } else if (strcmp(content, "Device Bind Start") == 0) {
            tuya_display_send_msg(TY_DISPLAY_TP_STAT_NETCFG, NULL, 0);
        }
    } break;

    default:
        break;
    }

    return;
}

void app_display_set_wifi_status(DIS_WIFI_STATUS_E status)
{
    return;
}