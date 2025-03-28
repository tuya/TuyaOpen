/**
 * @file display_gui.h
 * @brief display_gui module is used to 
 * @version 0.1
 * @date 2025-03-28
 */

#ifndef __DISPLAY_GUI_H__
#define __DISPLAY_GUI_H__

#include "tuya_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    TY_DISPLAY_TYPE_E type;
    int               len;
    char             *data;
} DISP_CHAT_MSG_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize the GUI display
 * 
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET display_gui_init(void);

/**
 * @brief Display the homepage of the GUI
 * 
 * @param None
 * @return None
 */
void display_gui_homepage(void);

/**
 * @brief Initialize the chat frame in the GUI
 * 
 * @param None
 * @return None
 */
void display_gui_chat_frame_init(void);

/**
 * @brief Handle display messages for the GUI
 * 
 * @param msg Pointer to the chat message structure
 * @return None
 */
void display_gui_chat_msg_handle(DISP_CHAT_MSG_T *msg);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_GUI_H__ */
