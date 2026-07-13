/**
 * @file tuya_media_types.h
 * @brief Common multimedia data type definitions.
 *
 * This header defines enumerations and types for multimedia data formats,
 * including pixel formats, color spaces, and byte-order conventions used
 * across camera, display, and image-processing modules.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_MEDIA_TYPES_H__
#define __TUYA_MEDIA_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
 * YUV422 packed byte-order variants
 *
 * Different hardware outputs YUV422 data in different byte orders.
 * These enumerations allow drivers to declare their native order
 * so that conversion functions can parse pixels correctly.
 ***********************************************************/
typedef enum {
    TUYA_YUV422_UYVY = 0,   /**< U0 Y0 V0 Y1 — default (e.g. T5AI DVP) */
    TUYA_YUV422_YUYV = 1,   /**< Y0 U0 Y1 V0 — e.g. ESP32-S3 LCD_CAM DVP */
} TUYA_YUV422_ORDER_E;

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_MEDIA_TYPES_H__ */
