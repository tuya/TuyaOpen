/**
 * @file tkl_video_enc.h
 * @brief video encode
 * @version 0.1
 * @date 2021-11-04
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 * Set video parameters and get video bitstream.
 */

#ifndef __TKL_VIDEO_ENC_H__
#define __TKL_VIDEO_ENC_H__

#include <time.h>
#include "tuya_cloud_types.h"
#include "tkl_video_in.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TKL_VENC_0 = 0,
    TKL_VENC_1,
    TKL_VENC_2,
    TKL_VENC_3,
    TKL_VENC_4,
    TKL_VENC_5,
    TKL_VENC_6,
    TKL_VENC_7,
    TKL_VENC_MAX,
} TKL_VENC_CHN_E;

typedef enum {
    TKL_VENC_MAIN = 0, // main stream
    TKL_VENC_SUB,      // sub stream
    TKL_VENC_3RD,      // 3rd stream
    TKL_VENC_4TH,      // 4th stream
    TKL_VENC_SNAP,     // snap
    TKL_VENC_MD,       // move detect
    TKL_VENC_HD,       // human detect
    TKL_VENC_QR,       // qrcode
    TKL_VENC_TYPE_MAX,
} TKL_VENC_TYPE_E; // stream work type

typedef struct {
    TKL_MEDIA_FRAME_TYPE_E frametype; // bitrate,kbps
    TKL_MEDIA_CODEC_TYPE_E codectype; // codec type
    char *pbuf;                       // frame buffer
    uint32_t buf_size;                // buffer size
    uint32_t used_size;               // used buffer size
    uint32_t width;                   // frame width
    uint32_t height;                  // frame height
    uint64_t pts;                     // sdk pts
    uint64_t timestamp;               // system utc time，unit: ms
    uint32_t seq;                     // frame sequence number
    uint32_t fragment;                // frame sequence's fragment  第几个分包
    uint8_t seq_error;                // frame sequence is error  这张图片是否错误
    uint8_t fragment_is_last;         // frame sequence's fragment is last fragment
} TKL_VENC_FRAME_T;                   // video frame

typedef int (*TKL_VENC_PUT_CB)(TKL_VENC_FRAME_T *pframe);

typedef int (*TKL_VENC_TIME_CB)(struct tm *ptm);

typedef enum {
    TKL_VENC_UPPER_REQUEST_FRAME = 0, // app get frame by using get frame function
    TKL_VENC_PUSH_FRAME_BY_CALLBACK,  // tkl push frame to app by callback when frame arrive
} TKL_VENC_DATA_TRANS_MODE_E;

typedef struct {
    uint32_t left;       // osd 左上角的位置x坐标
    uint32_t top;        // osd 左上角的位置y坐标
    uint32_t font_w;     // osd 字体大小,字体的宽
    uint32_t font_h;     // osd 字体大小,字体的高
} TKL_VENC_OSD_CONFIG_T; // osd 属性配置

typedef struct {
    uint32_t enable;                  // 1,enable,0,disable
    TKL_VENC_CHN_E chn;               // video encode channel
    TKL_VENC_TYPE_E type;             // stream work type
    TKL_MEDIA_CODEC_TYPE_E codectype; // codec type
    uint32_t fps;                     // fps
    uint32_t gop;                     // I Frame interval
    uint32_t bitrate;                 // bitrate,kbps
    uint32_t width;
    uint32_t height;
    uint32_t min_qp;
    uint32_t max_qp;
    TKL_VENC_DATA_TRANS_MODE_E trans_mode;
    TKL_VENC_PUT_CB put_cb;
    TKL_VENC_OSD_CONFIG_T osd;
} TKL_VENC_CONFIG_T;

typedef struct {
    int32_t enable;
    int32_t is_dls;
} TKL_VENC_OSD_T;

typedef enum {
    TKL_VENC_IDR_REQUEST = 0,  // 在最近的编码，请求IDR帧编码
    TKL_VENC_IDR_FLUSH_STREAM, // 刷掉编码器里残留的旧码流，并以IDR帧开始编码
    TKL_VENC_IDR_TYPE_MAX,
} TKL_VENC_IDR_E;

typedef struct {
    uint32_t enable; // 0, 不显示遮挡; 1, 区域遮挡
    uint32_t id;     // 遮挡编号
    uint32_t color;  // 遮挡的颜色，ARGB8888类型
    float x;       // 矩形区域左上点水平方向上的坐标, [0~100]
    float y;       // 矩形区域左上点垂直方向上的坐标, [0~100]
    float width;   // 矩形区域的宽度, [0~100]
    float height;  // 矩形区域的高度, [0~100]
} TKL_VENC_MASK_T;

typedef struct {
    uint32_t pool_buff_num;
    uint32_t pool_buff_size;
} TKL_VENC_STREAM_BUFF_T;

typedef enum {
    TKL_VENC_H264_PROFILE_BASELINE = 0,
    TKL_VENC_H264_PROFILE_MAIN,
    TKL_VENC_H264_PROFILE_HIGH,
    TKL_VENC_H265_PROFILE_MAIN,
    TKL_VENC_PROFILE_MAX,
} TKL_VENC_PROFILE_E;

typedef struct {
    int32_t bitrate; // 单位kbps
    int32_t fps;
    int32_t gop;
    TKL_VENC_PROFILE_E profile;
} TKL_VENC_H264_ATTR_T;

typedef struct {
    int32_t bitrate; // 单位kbps
    int32_t fps;
    int32_t gop;
    TKL_VENC_PROFILE_E profile;
} TKL_VENC_H265_ATTR_T;

typedef struct {
    uint32_t qp; // 编码质量,值越小，编码质量越高 | [0-100]
} TKL_VENC_JPEG_ATTR_T;

typedef struct {
    int32_t width;
    int32_t height;
    TKL_MEDIA_CODEC_TYPE_E codectype;
    union {
        TKL_VENC_H264_ATTR_T h264_attr;
        TKL_VENC_H265_ATTR_T h265_attr;
        TKL_VENC_JPEG_ATTR_T jpeg_attr;
    };
} TKL_VENC_FORMAT_T;

/**
 * @brief video encode init
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] pconfig: venc config
 * @param[in] count: count of pconfig
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_init(int32_t vi_chn, TKL_VENC_CONFIG_T *pconfig, int32_t count);

/**
 * @brief video encode get frame
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[out] pframe:  output frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_get_frame(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_FRAME_T *pframe);

/**
 * @brief video first snap
 *
 * @param[in] vi_chn: vi channel number
 * @param[out] pframe: output frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_get_first_snap(TKL_VI_CHN_E vi_chn, TKL_VENC_FRAME_T *pframe);

/**
 * @brief video encode set osd
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[out] posd:  osd config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_osd(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_OSD_T *posd);

/**
 * @brief video encode set osd
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[in] idr_type: request idr type
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_IDR(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_IDR_E idr_type);

/**
 * @brief video encode set mask
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[out] pmask: mask config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_mask(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_MASK_T *pmask);

/**
 * @brief video settings format
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[in] pformat: format config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_format(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_FORMAT_T *pformat);

/**
 * @brief video get format
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[out] pformat: format config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_get_format(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn, TKL_VENC_FORMAT_T *pformat);

/**
 * @brief video encode stream buff pool set
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 * @param[in] parg:  buff pool config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_video_stream_buffer(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn,
                                             TKL_VENC_STREAM_BUFF_T *parg);

/**
 * @brief video time callback
 *        Used to set osd time
 * @param[in] cb: time callback api
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_set_time_cb(TKL_VENC_TIME_CB cb);

/**
 * @brief video encode  start
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_start(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn);

/**
 * @brief video encode  stop
 *
 * @param[in] vi_chn: vi channel number
 * @param[in] venc_chn: venc channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_stop(TKL_VI_CHN_E vi_chn, TKL_VENC_CHN_E venc_chn);

/**
 * @brief video encode uninit
 *
 * @param[in] vi_chn: vi channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_venc_uninit(TKL_VI_CHN_E vi_chn);

#ifdef __cplusplus
}
#endif

#endif
