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

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    POCKET_DISP_TP_MENU_UP,
    POCKET_DISP_TP_MENU_DOWN,
    POCKET_DISP_TP_MENU_RIGHT,
    POCKET_DISP_TP_MENU_LEFT,
    POCKET_DISP_TP_MENU_ENTER,
    POCKET_DISP_TP_MENU_ESC,
    POCKET_DISP_TP_MENU_JOYCON_BTN,

    POCKET_DISP_TP_AI,

    POCKET_DISP_TP_EMOJ_HAPPY,
    POCKET_DISP_TP_EMOJ_ANGRY,
    POCKET_DISP_TP_EMOJ_CRY,

    POCKET_DISP_TP_WIFI_OFF,
    POCKET_DISP_TP_WIFI_FIND,
    POCKET_DISP_TP_WIFI_ADD,
    POCKET_DISP_TP_WIFI_CONNECTED,
    
    POCKET_DISP_TP_BATTERY_STATUS,
    POCKET_DISP_TP_BATTERY_CHARGING,

    POCKET_DISP_TP_RFID_SCAN_SUCCESS,
    POCKET_DISP_TP_AI_LOG,

    POCKET_DISP_TP_MAX,
} POCKET_DISP_TP_E;

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
OPERATE_RET app_display_send_msg(POCKET_DISP_TP_E tp, uint8_t *data, int len);



#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H__ */
