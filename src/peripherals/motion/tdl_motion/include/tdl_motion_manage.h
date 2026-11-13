/**
 * @file tdl_motion_manage.h
 * @brief Tuya Driver Layer motion sensor management interface.
 *
 * A motion device may expose any combination of accelerometer, gyroscope,
 * magnetometer and temperature data. Applications use the capability bits to
 * determine which fields in TDL_MOTION_DATA_T are valid.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDL_MOTION_MANAGE_H_
#define _TDL_MOTION_MANAGE_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TDL_MOTION_HANDLE;

typedef enum {
    TDL_MOTION_CAP_ACCEL  = (1U << 0),
    TDL_MOTION_CAP_GYRO   = (1U << 1),
    TDL_MOTION_CAP_MAG    = (1U << 2),
    TDL_MOTION_CAP_TEMP   = (1U << 3),
    TDL_MOTION_CAP_FUSION = (1U << 4),
    TDL_MOTION_CAP_DATA_READY = (1U << 5),
    TDL_MOTION_CAP_MOTION     = (1U << 6),
    TDL_MOTION_CAP_FREEFALL   = (1U << 7),
    TDL_MOTION_CAP_TAP        = (1U << 8),
} TDL_MOTION_CAPABILITY_E;

typedef enum {
    TDL_MOTION_ACCEL_RANGE_2G = 0,
    TDL_MOTION_ACCEL_RANGE_4G,
    TDL_MOTION_ACCEL_RANGE_8G,
    TDL_MOTION_ACCEL_RANGE_16G,
} TDL_MOTION_ACCEL_RANGE_E;

typedef enum {
    TDL_MOTION_GYRO_RANGE_125DPS = 0,
    TDL_MOTION_GYRO_RANGE_250DPS,
    TDL_MOTION_GYRO_RANGE_500DPS,
    TDL_MOTION_GYRO_RANGE_1000DPS,
    TDL_MOTION_GYRO_RANGE_2000DPS,
} TDL_MOTION_GYRO_RANGE_E;

typedef struct {
    TDL_MOTION_ACCEL_RANGE_E accel_range;
    TDL_MOTION_GYRO_RANGE_E gyro_range;
    uint16_t sample_rate_hz;
    bool low_power;
} TDL_MOTION_CFG_T;

typedef struct {
    TDL_MOTION_CFG_T sample_cfg;
    bool events_enabled;
} TDL_MOTION_OPEN_CFG_T;

typedef struct {
    float x;
    float y;
    float z;
} TDL_MOTION_VECTOR_T;

typedef struct {
    TDL_MOTION_VECTOR_T accel_mps2;
    TDL_MOTION_VECTOR_T gyro_dps;
    TDL_MOTION_VECTOR_T mag_ut;
    float temperature_c;

    bool accel_valid;
    bool gyro_valid;
    bool mag_valid;
    bool temp_valid;

    uint32_t timestamp_ms;
} TDL_MOTION_DATA_T;

typedef enum {
    TDL_MOTION_EVENT_DATA_READY = 0,
    TDL_MOTION_EVENT_MOTION,
    TDL_MOTION_EVENT_FREEFALL,
    TDL_MOTION_EVENT_TAP,
} TDL_MOTION_EVENT_E;

typedef void (*TDL_MOTION_EVENT_CB)(TDL_MOTION_HANDLE handle,
                                    TDL_MOTION_EVENT_E event,
                                    void *user_data);

/**
 * @brief Find a registered motion device by name.
 */
TDL_MOTION_HANDLE tdl_motion_find_dev(char *name);

/**
 * @brief Open a motion device and apply runtime configuration.
 *
 * The handle must first be obtained with tdl_motion_find_dev().
 */
OPERATE_RET tdl_motion_open(TDL_MOTION_HANDLE *handle, TDL_MOTION_OPEN_CFG_T *cfg);

/**
 * @brief Close a motion device and release its hardware resources.
 *
 * The registered device remains available and can be opened again.
 */
OPERATE_RET tdl_motion_close(TDL_MOTION_HANDLE handle);

/**
 * @brief Update measurement parameters of an opened motion device.
 *
 * This changes range, ODR and low-power mode. It does not change IRQ or event
 * enable state; close/open the device to change those lifecycle settings.
 */
OPERATE_RET tdl_motion_config(TDL_MOTION_HANDLE handle, TDL_MOTION_CFG_T *cfg);

/**
 * @brief Read normalized motion data.
 *
 * The driver sets the corresponding *_valid fields for data it supports.
 */
OPERATE_RET tdl_motion_read(TDL_MOTION_HANDLE handle, TDL_MOTION_DATA_T *data);

/**
 * @brief Get the capability bits supported by the device.
 */
OPERATE_RET tdl_motion_get_caps(TDL_MOTION_HANDLE handle, uint32_t *caps);

/**
 * @brief Set the optional motion event callback.
 */
OPERATE_RET tdl_motion_set_callback(TDL_MOTION_HANDLE handle,
                                    TDL_MOTION_EVENT_CB cb,
                                    void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* _TDL_MOTION_MANAGE_H_ */
