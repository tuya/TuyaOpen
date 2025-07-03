/**
 * @file tkl_video_in.h
 * @brief video input
 * @version 0.1
 * @date 2021-11-04
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 * Video input configuration: for sensor, ISP or VI attribute configuration,
 * such as image flip, anti flicker, compensation mode, profile, etc.
 */

#ifndef __TKL_VIDEO_IN_H__
#define __TKL_VIDEO_IN_H__

#include "tuya_cloud_types.h"
#include "tkl_media.h"
#include "tkl_isp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TKL_VI_MD_ROI_RECT_MAX 4
#define TKL_VI_PERI_POINT_MAX  8

typedef struct {
    // 1个IIC+host 对应一个video设备   hardware_port[0] + host_port[0] 构造成1个video设备
    uint32_t *hardware_port;      // 存放iic
    uint32_t *host_port;          // 存放host dvp or mipi
    uint32_t haraware_source_num; // 一共有多少对 iic + host

    // 视频内存池的分配方法 在RTOS系统有片外的PSRAM 所以需要支持这个
    void (*v_mem_free)(void *);
    void *(*v_mem_malloc)(uint32_t size);

} TKL_VI_HARDWARE_SOURCE_T;

typedef struct {
    uint32_t page;
    uint32_t reg_index;
    uint32_t val;
} TKL_VI_SENSOR_REG_CONFIG_T;

typedef enum {
    TKL_VI_SENSOR_TYPE_IIC,
    TKL_VI_SENSOR_TYPE_SPI,
} TKL_VI_SENSOR_CBUS_TYPE_E;

typedef struct {
    char conf[128];                                  // isp conf path
    char conf1[128];                                 // isp conf1 path
    char conf2[128];                                 // isp conf2 path
    char conf3[128];                                 // isp conf3 path
    char version[32];                                // version num
    char name[16];                                   // isp sensor num
    TKL_ISP_DN_SWITCH_CONFIG_T isp_dn_switch_config; // ADN switch config
    TKL_VI_SENSOR_CBUS_TYPE_E sensor_type;           // sensor control bus type
    int32_t addr;                                    // sensor address
    int32_t width;                                   // sensor width
    int32_t height;                                  // sensor height
    TKL_ISP_FIG_STYLE_CONFIG_T isp_fig_style_day;    // isp fig style in day
    TKL_ISP_FIG_STYLE_CONFIG_T isp_fig_style_night;  // isp fig style in night
    int32_t fps;                                     // sensor fps
} TKL_VI_ISP_CONFIG_T;

typedef struct {
    int32_t enable;          // 1,enable,0,disable
    TKL_VI_CHN_E chn;        // video input channel
    int32_t mirror;          // mirror defaults
    int32_t filp;            // filp defaults
    TKL_VI_ISP_CONFIG_T isp; // isp config
    void *pdata;             // reserver data
} TKL_VI_CONFIG_T;

typedef struct {
    float x;      // 矩形框x坐标 | [0.0 - 1.0]
    float y;      // 矩形框y坐标 | [0.0 - 1.0]
    float width;  // 矩形框宽度　| [0.0 - 1.0]
    float height; // 矩形框高度　| [0.0 - 1.0]
} TKL_VI_RECT_T;

typedef struct {
    int32_t x;
    int32_t y;
} TKL_VI_POINT_T;

typedef struct {
    TKL_VI_RECT_T draw_rect;      // coordinate region
    float score;                // score       | [0.0 - 1.0]
    TKL_MEDIA_DETECT_TYPE_E type; // 目标类型，人形，宠物，火焰等
    uint32_t id;                  // 目标编号
} TKL_VI_DETECT_TARGET_T;

typedef struct {
    int value;                   // 0,无动检，1，检测到动检
    TKL_VI_POINT_T motion_point; // 动检中心点坐标, 矩形中心为（0, 0）
} TKL_VI_MD_RESULT_T;

typedef struct {
    int32_t count;
    TKL_VI_DETECT_TARGET_T target[TKL_VI_TARGET_MAX];
    union {
        TKL_VI_MD_RESULT_T md;
    };
    uint64_t timestamp; // utc time, 单位:ms
} TKL_VI_DETECT_RESULT_T;

typedef struct {
    uint32_t roi_count;                             // 检测有效区域个数
    TKL_VI_RECT_T roi_rect[TKL_VI_MD_ROI_RECT_MAX]; // 区域框
    int32_t track_enable;                           // 移动追踪使能
} TKL_VI_MD_PARAM_T;

typedef struct {
    uint32_t point_count;                        // 顶点个数
    TKL_VI_POINT_T point[TKL_VI_PERI_POINT_MAX]; // 点
} TKL_VI_PERI_PARAM_T;

typedef struct {
    int32_t sensitivity;
    union {
        TKL_VI_MD_PARAM_T md;
        TKL_VI_PERI_PARAM_T peri;
    };
} TKL_VI_DETECT_PARAM_T;

typedef struct {
    char *pmodel;      // 检测模型数据指针
    int32_t model_len; // 检测模型的数据长度
} TKL_VI_DETECT_CONFIG_T;

/**
 * @brief vi init
 *
 * @param[in] pconfig: vi config
 * @param[in] count: count of pconfig
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_init(TKL_VI_CONFIG_T *pconfig, int32_t count);

/**
 * @brief vi set mirror and flip
 *
 * @param[in] chn: vi chn
 * @param[in] flag: mirror and flip value
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_set_mirror_flip(TKL_VI_CHN_E chn, TKL_VI_MIRROR_FLIP_E flag);

/**
 * @brief vi get mirror and flip
 *
 * @param[in] chn: vi chn
 * @param[out] flag: mirror and flip value
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_get_mirror_flip(TKL_VI_CHN_E chn, TKL_VI_MIRROR_FLIP_E *flag);

/**
 * @brief vi uninit
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_uninit(void);

/**
 * @brief  set sensor reg value
 *
 * @param[in] chn: vi chn
 * @param[in] pparam: reg info
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_sensor_reg_set(TKL_VI_CHN_E chn, TKL_VI_SENSOR_REG_CONFIG_T *parg);

/**
 * @brief  get sensor reg value
 *
 * @param[in] chn: vi chn
 * @param[in] pparam: reg info
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_sensor_reg_get(TKL_VI_CHN_E chn, TKL_VI_SENSOR_REG_CONFIG_T *parg);

/**
 * @brief detect init
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 * @param[in] pconfig: config
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_init(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type, TKL_VI_DETECT_CONFIG_T *p_config);

/**
 * @brief detect start
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_start(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type);

/**
 * @brief detect stop
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_stop(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type);

/**
 * @brief get detection results
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 * @param[out] presult: detection results
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_get_result(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type, TKL_VI_DETECT_RESULT_T *presult);

/**
 * @brief set detection param
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 * @param[in] pparam: detection param
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_set(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type, TKL_VI_DETECT_PARAM_T *pparam);

/**
 * @brief detect uninit
 *
 * @param[in] chn: vi chn
 * @param[in] type: detect type
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_vi_detect_uninit(TKL_VI_CHN_E chn, TKL_MEDIA_DETECT_TYPE_E type);

#ifdef __cplusplus
}
#endif

#endif
