/**
 * @file tkl_media.h
 * @brief Audio and video generic type definition header file
 * @version 0.1
 * @date 2021-11-04
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_MEDIA_H__
#define __TKL_MEDIA_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TKL_VI_TARGET_MAX 3

typedef enum {
    TKL_VIDEO_PB_FRAME = 0,
    TKL_VIDEO_I_FRAME,
    TKL_VIDEO_TS_FRAME,
    TKL_AUDIO_FRAME,
    TKL_CMD_FRAME,
    TKL_MEDIA_FRAME_MAX,
} TKL_MEDIA_FRAME_TYPE_E;

typedef enum {
    TKL_CODEC_VIDEO_MPEG4 = 0,
    TKL_CODEC_VIDEO_H263,
    TKL_CODEC_VIDEO_H264,
    TKL_CODEC_VIDEO_MJPEG,
    TKL_CODEC_VIDEO_H265,
    TKL_CODEC_VIDEO_YUV420,
    TKL_CODEC_VIDEO_YUV422,
    TKL_CODEC_VIDEO_MAX = 99,

    TKL_CODEC_AUDIO_ADPCM,
    TKL_CODEC_AUDIO_PCM,
    TKL_CODEC_AUDIO_AAC_RAW,
    TKL_CODEC_AUDIO_AAC_ADTS,
    TKL_CODEC_AUDIO_AAC_LATM,
    TKL_CODEC_AUDIO_G711U,
    TKL_CODEC_AUDIO_G711A,
    TKL_CODEC_AUDIO_G726,
    TKL_CODEC_AUDIO_SPEEX,
    TKL_CODEC_AUDIO_MP3,
    TKL_CODEC_AUDIO_MAX = 199,
    TKL_CODEC_TYPE_MAX
} TKL_MEDIA_CODEC_TYPE_E;

// VI
typedef enum {
    TKL_VI_0 = 0,
    TKL_VI_1,
    TKL_VI_2,
    TKL_VI_3,
    TKL_VI_MAX,
} TKL_VI_CHN_E;

typedef enum {
    TKL_VI_MIRROR_FLIP_NONE = 0, // default
    TKL_VI_MIRROR,               // mirror enable
    TKL_VI_FLIP,                 // flip enable
    TKL_VI_MIRROR_FLIP,          // flip and mirror enable
} TKL_VI_MIRROR_FLIP_E;

typedef enum {
    TKL_MEDIA_DETECT_TYPE_HUMAN = 0, // 人形，human
    TKL_MEDIA_DETECT_TYPE_PET,       // 宠物，一般指猫狗，cat，dog
    TKL_MEDIA_DETECT_TYPE_CAR,       // 车形，car
    TKL_MEDIA_DETECT_TYPE_FACE,      // 人脸，face
    TKL_MEDIA_DETECT_TYPE_FLAME,     // 火焰，flame
    TKL_MEDIA_DETECT_TYPE_BABY_CRY,  // 婴儿哭声，baby cry
    TKL_MEDIA_DETECT_TYPE_DB,        // 声音分贝，audio decibel
    TKL_MEDIA_DETECT_TYPE_MOTION,    // 移动
    TKL_MEDIA_DETECT_TYPE_TRIP,      // 拌线
    TKL_MEDIA_DETECT_TYPE_PERI,      // 周界
} TKL_MEDIA_DETECT_TYPE_E;

#ifdef __cplusplus
}
#endif

#endif
