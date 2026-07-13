/**
 * @file tdd_camera_esp_csi.h
 * @brief MIPI-CSI camera driver for ESP32-P4 using the esp_video (V4L2) stack
 *        and the P4 hardware JPEG encoder.
 *
 * The OV5647 sensor is driven through esp_video's MIPI-CSI device. The ISP
 * pipeline converts the sensor RAW output into YUV422, which is posted as the
 * raw frame (for on-screen preview) and, on demand, encoded to JPEG by the
 * hardware JPEG engine (for photo capture / AI vision recognition).
 *
 * The camera SCCB (sensor control I2C) is shared with the touch / audio codec
 * bus, so the bus is NOT created here; the existing master bus handle is
 * retrieved by port number and reused (init_sccb = false).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_ESP_CSI_H__
#define __TDD_CAMERA_ESP_CSI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int      i2c_port;      /* I2C/SCCB port shared with the sensor (already initialized) */
    uint32_t sccb_freq_hz;  /* SCCB clock in Hz (e.g. 100000) */
    int      reset_pin;     /* sensor reset GPIO, -1 if wired to the board reset */
    int      pwdn_pin;      /* sensor power-down GPIO, -1 if not used */
} TDD_CAMERA_ESP_CSI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register an ESP32-P4 MIPI-CSI camera (OV5647) with the TDL camera layer.
 *
 * @param[in] name Camera device name (must match CONFIG_CAMERA_NAME, default "camera").
 * @param[in] cfg  CSI/SCCB configuration.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_camera_esp_csi_register(const char *name, const TDD_CAMERA_ESP_CSI_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_ESP_CSI_H__ */
