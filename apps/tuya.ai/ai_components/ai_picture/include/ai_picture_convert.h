/**
 * @file ai_picture_convert.h
 * @brief Picture format conversion interface definitions.
 *
 * This header provides function declarations and type definitions for converting
 * picture formats, supporting JPEG to YUV422, RGB565, and RGB888 conversions.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_PICTURE_CONVERT_H__
#define __AI_PICTURE_CONVERT_H__

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
typedef struct {
    TUYA_FRAME_FMT_E in_fmt;
    uint32_t         in_frame_size;
    TUYA_FRAME_FMT_E out_fmt;
} AI_PICTURE_CONVERT_CFG_T;

typedef struct {
    TUYA_FRAME_FMT_E fmt;
    uint16_t         width;
    uint16_t         height;
    uint8_t         *frame;
    uint32_t         frame_size;
} AI_PICTURE_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Start picture format conversion with specified configuration.
 *
 * @param cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_start(AI_PICTURE_CONVERT_CFG_T *cfg);

/**
 * @brief Feed input data for conversion.
 *
 * @param data Pointer to the input data buffer.
 * @param len Length of the input data in bytes.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_feed(uint8_t *data, uint32_t len);

/**
 * @brief Perform picture format conversion and get output information.
 *
 * @param out_info Pointer to store the output picture information.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert(AI_PICTURE_INFO_T *out_info);

/**
 * @brief Stop picture format conversion and free resources.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_convert_stop(void);


#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_CONVERT_H__ */
