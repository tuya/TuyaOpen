/**
 * @file tuya_display.h
 * @brief Header file for Tuya Display System
 *
 * This header file provides the declarations for initializing the display system
 * and sending messages to the display. It includes the necessary data types and
 * function prototypes for interacting with the display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_DISPLAY_H__
#define __TUYA_DISPLAY_H__

#include "tuya_cloud_types.h"

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
    TY_DISPLAY_TP_HUMAN_CHAT,
    TY_DISPLAY_TP_AI_CHAT,    
    TY_DISPLAY_TP_AI_THINKING,
    
    TY_DISPLAY_TP_STAT_LISTEN,
    TY_DISPLAY_TP_STAT_SPEAK, 
    TY_DISPLAY_TP_STAT_IDLE,

    TY_DISPLAY_TP_STAT_NETCFG,
    TY_DISPLAY_TP_STAT_POWERON,
    TY_DISPLAY_TP_STAT_ONLINE,

    TY_DISPLAY_TP_STAT_SLEEP, 
    TY_DISPLAY_TP_STAT_WAKEUP, 
    TY_DISPLAY_TP_MAX,  
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
OPERATE_RET tuya_display_init(void);

/**
 * @brief Send display message to the display system
 * 
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET tuya_display_send_msg(TY_DISPLAY_TYPE_E tp, char *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_DISPLAY_H__ */
