/**
 * @file tal_image_frame_to_rgb.h
 * @brief Camera frame to RGB565 conversion interface.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_IMAGE_FRAME_TO_RGB_H__
#define __TAL_IMAGE_FRAME_TO_RGB_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB565 buffer size the converter needs for one frame.
 *
 * @param[in] fmt     Frame format, as carried by TDL_CAMERA_FRAME_T.fmt.
 * @param[in] width   Frame width in pixels.
 * @param[in] height  Frame height in pixels.
 * @return Required buffer size in bytes, 0 for zero geometry.
 */
uint32_t tal_image_frame_to_rgb565_size(TUYA_FRAME_FMT_E fmt, uint16_t width, uint16_t height);

/**
 * @brief Convert one camera frame to RGB565, picking the converter by format.
 *
 * @param[in]  fmt     Frame format, as carried by TDL_CAMERA_FRAME_T.fmt.
 * @param[in]  in_buf  Source frame buffer.
 * @param[in]  in_len  Source buffer length in bytes, 0 when unknown.
 * @param[in]  width   Frame width in pixels.
 * @param[in]  height  Frame height in pixels.
 * @param[out] out_buf RGB565 destination, sized by tal_image_frame_to_rgb565_size().
 * @param[in]  out_len Destination capacity in bytes.
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED for an unsupported format.
 */
OPERATE_RET tal_image_frame_to_rgb565(TUYA_FRAME_FMT_E fmt, const uint8_t *in_buf, uint32_t in_len,
                                      uint16_t width, uint16_t height, uint8_t *out_buf, uint32_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_FRAME_TO_RGB_H__ */
