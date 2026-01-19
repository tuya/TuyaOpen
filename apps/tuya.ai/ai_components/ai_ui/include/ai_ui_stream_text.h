/**
 * @file ai_ui_stream_text.h
 * @brief ai_ui_stream_text module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_STREAM_TEXT_H__
#define __AI_UI_STREAM_TEXT_H__

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
typedef void (*AI_UI_STREAM_TEXT_DISP_CB)(char *string);

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET ai_ui_stream_text_init(AI_UI_STREAM_TEXT_DISP_CB disp_cb);

void ai_ui_stream_text_start(void);

void ai_ui_stream_text_write(const char *text);

void ai_ui_stream_text_end(void);

void ai_ui_stream_text_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_STREAM_TEXT_H__ */
