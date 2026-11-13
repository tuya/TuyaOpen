/**
 * @file tdl_encoder_manage.h
 * @brief Tuya Driver Layer encoder management interface.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDL_ENCODER_MANAGE_H_
#define _TDL_ENCODER_MANAGE_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TDL_ENCODER_HANDLE;

typedef enum {
    TDL_ENCODER_EVENT_ROTATE = 0,
    TDL_ENCODER_EVENT_BUTTON_DOWN,
    TDL_ENCODER_EVENT_BUTTON_UP,
    TDL_ENCODER_EVENT_BUTTON_CLICK,
    TDL_ENCODER_EVENT_BUTTON_DOUBLE_CLICK,
    TDL_ENCODER_EVENT_BUTTON_LONG_START,
    TDL_ENCODER_EVENT_BUTTON_LONG_HOLD,
} TDL_ENCODER_EVENT_TYPE_E;

typedef struct {
    /* Application interaction timing. Hardware traits belong to TDD registration. */
    uint16_t click_ms;
    uint16_t long_press_ms;
    uint16_t long_hold_ms;
} TDL_ENCODER_CFG_T;

typedef struct {
    TDL_ENCODER_EVENT_TYPE_E type;
    int32_t position;
    int32_t delta;
    uint32_t timestamp_ms;
} TDL_ENCODER_EVENT_T;

typedef void (*TDL_ENCODER_CB)(TDL_ENCODER_HANDLE handle,
                               const TDL_ENCODER_EVENT_T *event,
                               void *user_data);

/**
 * @brief Find a registered encoder device by name.
 *
 * @param[in] name Registered encoder device name.
 *
 * @return Encoder handle on success, NULL if the device is not registered.
 */
TDL_ENCODER_HANDLE tdl_encoder_find_dev(char *name);

/**
 * @brief Open an encoder instance with application interaction configuration.
 *
 * The handle must first be obtained with tdl_encoder_find_dev(). Opening an
 * already opened instance returns an error. Hardware traits are supplied when
 * the TDD device is registered.
 */
OPERATE_RET tdl_encoder_open(TDL_ENCODER_HANDLE *handle, TDL_ENCODER_CFG_T *cfg);

/**
 * @brief Close an encoder instance and release its hardware resources.
 *
 * The registered device remains available and can be opened again later.
 */
OPERATE_RET tdl_encoder_close(TDL_ENCODER_HANDLE handle);

/**
 * @brief Set the event callback for an encoder instance.
 */
OPERATE_RET tdl_encoder_set_callback(TDL_ENCODER_HANDLE handle,
                                     TDL_ENCODER_CB cb,
                                     void *user_data);

/**
 * @brief Read the accumulated position in detents.
 *
 * The position saturates at INT32_MIN/INT32_MAX rather than wrapping.
 */
OPERATE_RET tdl_encoder_read_position(TDL_ENCODER_HANDLE handle, int32_t *position);

/**
 * @brief Read and clear the pending detent delta.
 *
 * The pending delta also saturates at INT32_MIN/INT32_MAX.
 */
OPERATE_RET tdl_encoder_read_delta(TDL_ENCODER_HANDLE handle, int32_t *delta);

/**
 * @brief Read the debounced encoder button state.
 */
OPERATE_RET tdl_encoder_read_button(TDL_ENCODER_HANDLE handle, uint8_t *pressed);

#ifdef __cplusplus
}
#endif

#endif /* _TDL_ENCODER_MANAGE_H_ */
