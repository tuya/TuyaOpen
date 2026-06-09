/**
 * @file ai_ui_stream_text.h
 * @brief Stream text display interface definitions.
 *
 * This header provides function declarations for streaming text display,
 * supporting incremental text updates for real-time AI responses.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
/**
 * @brief Initialize stream text display module.
 *
 * @param disp_cb Callback function for displaying text.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_stream_text_init(AI_UI_STREAM_TEXT_DISP_CB disp_cb);

/**
 * @brief Start streaming text display.
 */
void ai_ui_stream_text_start(void);

/**
 * @brief Write text data to stream display.
 *
 * @param text Pointer to the text string to display.
 */
void ai_ui_stream_text_write(const char *text);

/**
 * @brief End streaming text display.
 */
void ai_ui_stream_text_end(void);

/**
 * @brief Reset stream text display state.
 */
void ai_ui_stream_text_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_STREAM_TEXT_H__ */
