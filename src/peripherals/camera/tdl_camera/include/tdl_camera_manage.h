/**
 * @file tdl_camera_manage.h
 * @brief Camera device management header
 *
 * This header file defines the interface for camera device management,
 * including device discovery, configuration, frame format definitions,
 * and callback functions. It provides structures and enumerations for
 * managing camera devices and processing camera frames.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_CAMERA_MANAGE_H__
#define __TDL_CAMERA_MANAGE_H__

#include "tuya_cloud_types.h"
#include "tuya_media_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_IMG_FMT_RAW_MASK       0x00FF
#define TDL_IMG_FMT_ENCODED_MASK   0xFF00
#define ENCODED_SHIFT(value)      ((value) << 8)
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum  {
    TDL_CAMERA_DVP= 0,
}TDL_CAMERA_TYPE_E;

typedef enum {
    TDL_CAMERA_FMT_YUV422 = 1,
    TDL_CAMERA_FMT_JPEG   = ENCODED_SHIFT(1),
    TDL_CAMERA_FMT_H264   = ENCODED_SHIFT(2),
    TDL_CAMERA_FMT_JPEG_YUV422_BOTH =  (TDL_CAMERA_FMT_JPEG | TDL_CAMERA_FMT_YUV422),
    TDL_CAMERA_FMT_H264_YUV422_BOTH =  (TDL_CAMERA_FMT_H264 | TDL_CAMERA_FMT_YUV422),
} TDL_CAMERA_FMT_E;

typedef void*  TDL_CAMERA_HANDLE_T;

typedef struct {
    TDL_CAMERA_TYPE_E         type;
    uint16_t                  fps;
    uint16_t                  width;
    uint16_t                  height;
    TDL_CAMERA_FMT_E          out_fmt;
    uint16_t                  max_fps;
    uint16_t                  max_width;
    uint16_t                  max_height;
    TDL_CAMERA_FMT_E          supported_fmts; /**< Bitmask of output formats this camera
                                                *  supports (see TDL_CAMERA_FMT_E). Query via
                                                *  tdl_camera_dev_get_info() before open to
                                                *  pick a format. 0 = unspecified. */
    TUYA_YUV422_ORDER_E       yuv_order; /**< YUV422 byte order: 0=UYVY(default), 1=YUYV */
} TDL_CAMERA_DEV_INFO_T;

typedef struct
{
    uint16_t            id;
    uint8_t             is_i_frame;
    uint8_t             is_complete;
    TUYA_FRAME_FMT_E    fmt;
    uint16_t            width;
	uint16_t            height;
    uint32_t            data_len;
    uint8_t            *data;
    uint32_t            total_frame_len;
} TDL_CAMERA_FRAME_T;

typedef OPERATE_RET (*TDL_CAMERA_GET_FRAME_CB)(TDL_CAMERA_HANDLE_T hdl,  TDL_CAMERA_FRAME_T *frame);

typedef struct {
    uint16_t                  fps;
    uint16_t                  width;
    uint16_t                  height;
    TDL_CAMERA_FMT_E          out_fmt;
    TUYA_DVP_ENCODED_QUALITY  encoded_quality;
    TDL_CAMERA_GET_FRAME_CB   get_frame_cb;
    TDL_CAMERA_GET_FRAME_CB   get_encoded_frame_cb;
    /* Target bitrate for drivers that drive a hardware video encoder. Only the
     * caller knows the budget the stream has to fit (P2P send window, storage),
     * so a driver default is a guess. 0 keeps the driver's own default. */
    uint32_t                  bitrate_kbps;
    /* Frames between I-frames. Sets how long a lossy link stays frozen after it
     * has to resync, so the caller that knows the link picks it. 0 = default. */
    uint32_t                  gop;
}TDL_CAMERA_CFG_T;


/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Find camera device by name
 * @param name Camera device name
 * @return Camera handle if found, NULL otherwise
 */
TDL_CAMERA_HANDLE_T tdl_camera_find_dev(char *name);

/**
 * @brief Get camera device information
 * @param camera_hdl Camera handle
 * @param dev_info Pointer to store device information
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
OPERATE_RET tdl_camera_dev_get_info(TDL_CAMERA_HANDLE_T camera_hdl, TDL_CAMERA_DEV_INFO_T *dev_info);

/**
 * @brief Open camera device with specified configuration
 * @param camera_hdl Camera handle
 * @param cfg Pointer to camera configuration structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         or other error codes on initialization failure
 */
OPERATE_RET tdl_camera_dev_open(TDL_CAMERA_HANDLE_T camera_hdl,  TDL_CAMERA_CFG_T *cfg);

/**
 * @brief Close camera device
 * @param camera_hdl Camera handle
 * @return OPRT_NOT_SUPPORTED (function not implemented)
 */
OPERATE_RET tdl_camera_dev_close(TDL_CAMERA_HANDLE_T camera_hdl);

/**
 * @brief Ask the camera's encoder for an immediate key frame
 *
 * A decoder can only begin, or resume after loss, at a key frame. Waiting for
 * the next scheduled one costs up to a full GOP of blank video.
 *
 * @param camera_hdl camera handle
 * @return OPRT_OK when the request was accepted, OPRT_NOT_SUPPORTED when this
 *         camera has no encoder to ask
 */
OPERATE_RET tdl_camera_dev_request_i_frame(TDL_CAMERA_HANDLE_T camera_hdl);

/**
 * @brief Change the target bitrate of the camera's encoder
 *
 * @param camera_hdl camera handle
 * @param kbps new target bitrate
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED when this camera has no
 *         encoder whose rate can be changed
 */
OPERATE_RET tdl_camera_dev_set_bitrate(TDL_CAMERA_HANDLE_T camera_hdl, uint32_t kbps);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_CAMERA_MANAGE_H__ */
