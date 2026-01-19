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
#include "ai_ui_manage.h"

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
    POCKET_DISP_TP_RFID_SCAN_SUCCESS = AI_UI_DISP_SYS_MAX+1,
    POCKET_DISP_TP_AI_LOG,
    POCKET_DISP_TP_MAX,
} POCKET_DISP_TP_E;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_ui_chat_register(void);


#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H__ */
