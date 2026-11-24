/**
 * @file tdd_audio_ty_uart.h
 * @brief Generic TDD audio driver for voice chips implementing the Tuya
 *        offline voice UART protocol.
 *
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_AUDIO_TY_UART_H__
#define __TDD_AUDIO_TY_UART_H__

#include "tuya_cloud_types.h"
#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Board configuration for the Tuya offline voice UART driver.
 *
 * UART pinmux must already be configured by the board before registration.
 */
typedef struct {
    TUYA_UART_NUM_E uart_port;       /**< UART controller used by the voice device. */
    TUYA_GPIO_NUM_E spk_flow_pin;    /**< SPK flow-control input, or TUYA_GPIO_NUM_MAX. */
    TUYA_GPIO_LEVEL_E spk_flow_active_level; /**< Level meaning SPK data may be sent. */
    uint32_t baudrate;               /**< UART baud rate, normally 921600. */
    uint16_t boot_delay_ms;          /**< Delay before link handshake in milliseconds. */
    uint8_t default_volume;          /**< Initial playback volume, 0..100. */
    uint8_t mic_gain_db;             /**< Microphone gain, 0..32 dB. */
    TDL_AUDIO_STREAM_INFO_T capture; /**< CI-to-host MIC stream capabilities. */
    TDL_AUDIO_STREAM_INFO_T playback;/**< Host-to-CI SPK stream capabilities. */
} TDD_AUDIO_TY_UART_CFG_T;

/**
 * @brief Register the Tuya offline voice UART audio driver.
 *
 * This function is called by board initialization code only. Application code
 * must use the tdl_audio_* abstraction.
 *
 * @param[in] name Unique TDL audio device name, normally AUDIO_CODEC_NAME.
 * @param[in] cfg  Board transport and PCM configuration.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdd_audio_ty_uart_register(char *name, const TDD_AUDIO_TY_UART_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_TY_UART_H__ */
