/**
 * @file app_display.h
 * @brief Header file for Tuya Display System
 *
 * This header file provides the declarations for initializing the display system
 * and sending messages to the display. It includes the necessary data types and
 * function prototypes for interacting with the display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __APP_DISPLAY_H__
#define __APP_DISPLAY_H__

#include "tuya_cloud_types.h"

#include "lang_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// display network status
typedef uint8_t UI_WIFI_STATUS_E;
#define UI_WIFI_STATUS_DISCONNECTED 0
#define UI_WIFI_STATUS_GOOD         1
#define UI_WIFI_STATUS_FAIR         2
#define UI_WIFI_STATUS_WEAK         3

#define EMOJI_NEUTRAL      "NEUTRAL"
#define EMOJI_SAD          "SAD"
#define EMOJI_ANGRY        "ANGRY"
#define EMOJI_SURPRISE     "SURPRISE"
#define EMOJI_CONFUSED     "CONFUSED"
#define EMOJI_THINKING     "THINKING"
#define EMOJI_HAPPY        "HAPPY"
#define EMOJI_TOUCH        "TOUCH"
#define EMOJI_FEARFUL      "FEARFUL"
#define EMOJI_DISAPPOINTED "DISAPPOINTED"
#define EMOJI_ANNOYED      "ANNOYED"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    TY_DISPLAY_TP_USER_MSG,
    TY_DISPLAY_TP_ASSISTANT_MSG,
    TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_START,
    TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_DATA,
    TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END,
    TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_INTERRUPT,

    TY_DISPLAY_TP_SYSTEM_MSG,

    TY_DISPLAY_TP_EMOTION,

    // status bar
    TY_DISPLAY_TP_STATUS,
    TY_DISPLAY_TP_NOTIFICATION,
    TY_DISPLAY_TP_NETWORK,
    TY_DISPLAY_TP_CHAT_MODE,

    TY_DISPLAY_TP_MAX
} TY_DISPLAY_TYPE_E;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_display_init(void);

/**
 * @brief Send display message to the display system
 *
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H__ */
