/**
 * @file ai_picture_output.h
 * @brief Picture output interface definitions.
 *
 * This header provides function declarations and type definitions for downloading
 * and outputting pictures from URLs via HTTP, with event notification support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_PICTURE_OUTPUT_H__
#define __AI_PICTURE_OUTPUT_H__

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
    AI_PICTURE_OUTPUT_START = 0,
    AI_PICTURE_OUTPUT_SUCCESS,
    AI_PICTURE_OUTPUT_FAILED,
} AI_PICTURE_OUTPUT_EVENT_E;

typedef struct {
    AI_PICTURE_OUTPUT_EVENT_E     event;
    uint32_t                      total_size;
} AI_PICTURE_OUTPUT_NOTIFY_T;

typedef void (*AI_PICTURE_OUTPUT_NOTIFY_CB)(AI_PICTURE_OUTPUT_NOTIFY_T *info);

typedef void (*AI_PICTURE_OUTPUT_CB)(uint8_t *data, uint32_t len, bool is_eof);

typedef struct {
    AI_PICTURE_OUTPUT_NOTIFY_CB notify_cb;
    AI_PICTURE_OUTPUT_CB        output_cb;
} AI_PICTURE_OUTPUT_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize the picture output module.
 *
 * @param cfg Pointer to the output configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_init(AI_PICTURE_OUTPUT_CFG_T *cfg);

/**
 * @brief Start downloading and outputting picture from URL.
 *
 * @param url Pointer to the picture URL string.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_start(char *url);

/**
 * @brief Stop picture output and cleanup resources.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_stop(void);

/**
 * @brief Check if picture output module is initialized.
 *
 * @return true if initialized, false otherwise.
 */
bool ai_picture_is_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_OUTPUT_H__ */
