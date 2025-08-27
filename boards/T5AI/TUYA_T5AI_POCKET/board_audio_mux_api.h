/**
 * @file board_audio_mux_api.h
 * @author Tuya Inc.
 * @brief Header file for audio multiplexer control API for TUYA_T5AI_POCKET board.
 *
 * This module provides APIs to control the audio multiplexer that routes signals
 * to the MIC2 input of the audio codec. The mux can switch between microphone
 * input and speaker loopback for audio processing.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_AUDIO_MUX_API_H__
#define __BOARD_AUDIO_MUX_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/**
 * @brief GPIO pin used to control the audio multiplexer selection
 */
#define BOARD_AUDIO_MUX_SEL_PIN TUYA_GPIO_NUM_23

/**
 * @brief GPIO level to select microphone input route
 */
#define BOARD_AUDIO_MUX_SEL_MIC_LV TUYA_GPIO_LEVEL_LOW

/**
 * @brief GPIO level to select speaker loopback route
 */
#define BOARD_AUDIO_MUX_SEL_LOOPBACK_LV TUYA_GPIO_LEVEL_HIGH

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Audio mux route selection enumeration
 */
typedef enum {
    BOARD_AUDIO_MUX_ROUTE_MIC = 0,      /**< Route microphone input to MIC2 */
    BOARD_AUDIO_MUX_ROUTE_LOOPBACK = 1  /**< Route speaker loopback to MIC2 */
} BOARD_AUDIO_MUX_ROUTE_E;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the audio multiplexer GPIO and set default route
 *
 * This function initializes the GPIO pin used to control the audio multiplexer
 * and sets it to the default microphone input route.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_init(void);

/**
 * @brief Set the audio multiplexer to route microphone input to MIC2
 *
 * This function configures the audio multiplexer to route the microphone
 * input signal to the MIC2 input of the audio codec.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_set_mic_route(void);

/**
 * @brief Set the audio multiplexer to route speaker loopback to MIC2
 *
 * This function configures the audio multiplexer to route the speaker
 * loopback signal to the MIC2 input of the audio codec for echo
 * cancellation and audio processing.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_set_loopback_route(void);

/**
 * @brief Set the audio multiplexer route based on the provided route selection
 *
 * This function allows setting the audio multiplexer route using the
 * BOARD_AUDIO_MUX_ROUTE_E enumeration.
 *
 * @param[in] route The desired audio route (microphone or loopback)
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_set_route(BOARD_AUDIO_MUX_ROUTE_E route);

/**
 * @brief Get the current audio multiplexer route
 *
 * This function reads the current GPIO state to determine which route
 * is currently selected.
 *
 * @param[out] route Pointer to store the current route selection
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_get_route(BOARD_AUDIO_MUX_ROUTE_E *route);

/**
 * @brief Deinitialize the audio multiplexer GPIO
 *
 * This function deinitializes the GPIO pin used for audio multiplexer control.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_audio_mux_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_AUDIO_MUX_API_H__ */
