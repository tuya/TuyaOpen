/**
 * @file tdd_camera_esp_video_dvp.h
 * @brief ESP32-S31 DVP camera TDD driver using Espressif esp_video.
 */

#ifndef __TDD_CAMERA_ESP_VIDEO_DVP_H__
#define __TDD_CAMERA_ESP_VIDEO_DVP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int i2c_port;
    int sccb_freq_hz;
    int reset_pin;
    int pwdn_pin;
    int xclk_pin;
    int xclk_freq_hz;
    int data_io[8];
    int vsync_io;
    int de_io;
    int pclk_io;
} TDD_CAMERA_ESP_VIDEO_DVP_CFG_T;

OPERATE_RET tdd_camera_esp_video_dvp_register(const char *name,
                                              const TDD_CAMERA_ESP_VIDEO_DVP_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_ESP_VIDEO_DVP_H__ */
