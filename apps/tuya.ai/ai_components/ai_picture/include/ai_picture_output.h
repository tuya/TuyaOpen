/**
 * @file ai_picture_output.h
 * @brief Picture output module for receiving AI-generated images.
 *        Accumulates streamed JPEG chunks and saves the completed
 *        picture to the album, then notifies via AI_AI_EVENT_ACCEPT_PICTURE.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_OUTPUT_H__
#define __AI_PICTURE_OUTPUT_H__

#include "tuya_cloud_types.h"
#include "ai_picture.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_OUTPUT_MAX_NUM 12

#ifndef COMP_AI_PICTURE_DLD_MAX_FILE_SIZE
#define COMP_AI_PICTURE_DLD_MAX_FILE_SIZE (2 * 1024 * 1024)
#endif

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Set the desired output picture dimensions for AI image generation
 * @param[in] width desired width in pixels
 * @param[in] height desired height in pixels
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_set_size(uint16_t width, uint16_t height);

/**
 * @brief Accumulate a JPEG chunk and save to album when all chunks are received
 * @param[in] data JPEG chunk data
 * @param[in] len chunk length in bytes
 * @param[in] total_len total expected JPEG size in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_save_to_album(uint8_t *data, uint32_t len, uint32_t total_len);

#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)
/**
 * @brief Register file-storage download handler to receive cloud-pushed images.
 *        Must be called after iot init (e.g. in pre_device_init).
 *        Max file size is controlled by COMP_AI_PICTURE_DLD_MAX_FILE_SIZE (default 2MB).
 * @param[in] width desired output width in pixels
 * @param[in] height desired output height in pixels
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_dld_init(uint16_t width, uint16_t height);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_OUTPUT_H__ */
