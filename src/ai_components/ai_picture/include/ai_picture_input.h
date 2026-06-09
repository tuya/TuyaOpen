/**
 * @file ai_picture_input.h
 * @brief Picture input queue for sending album photos to the AI agent.
 *        Supports queuing up to AI_PICTURE_INPUT_MAX_NUM pictures
 *        with optional text, then batch-sending them during a conversation.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_INPUT_H__
#define __AI_PICTURE_INPUT_H__

#include "tuya_cloud_types.h"
#include "ai_picture.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_INPUT_MAX_NUM 3

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Add a picture from album to the input queue (retain-lock only, data read deferred)
 * @param[in] filename album picture filename
 * @param[in] text optional text to send along with the picture (NULL if none)
 * @return OPRT_OK on success, OPRT_EXCEED_UPPER_LIMIT if queue is full
 */
OPERATE_RET ai_picture_input_add_from_album(char *filename, char *text);

/**
 * @brief Remove a queued picture by filename and release its retain-lock
 * @param[in] filename album picture filename to remove
 * @return OPRT_OK on success, OPRT_NOT_FOUND if not in queue
 */
OPERATE_RET ai_picture_input_del_from_album(char *filename);

/**
 * @brief Send all queued pictures to the AI agent and clear the queue.
 *        Each picture is read, sent, and freed one at a time to save memory.
 *        Fires AI_AI_EVENT_SEND_PICTURE_END when at least one picture was sent.
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_input_from_album(void);

/**
 * @brief Get the number of pictures currently queued
 * @return number of queued pictures
 */
uint32_t ai_picture_input_get_num(void);

/**
 * @brief Send a single image with a recognition prompt to the AI agent
 * @param[in] data JPEG image data
 * @param[in] len JPEG data length in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_input_recognize(uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_INPUT_H__ */
