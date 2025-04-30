/**
 * @file tdd_audio_codec_bus.h
 * @brief Audio codec bus interface definition
 */

#ifndef __TDD_AUDIO_CODEC_BUS_H__
#define __TDD_AUDIO_CODEC_BUS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * TDD_AUDIO_I2C_HANDLE;
typedef void * TDD_AUDIO_I2S_TX_HANDLE;
typedef void * TDD_AUDIO_I2S_RX_HANDLE;

/**
 * @brief Audio codec bus configuration structure
 */
typedef struct {
    uint8_t i2c_id;                            /*!< I2C bus ID */
    TUYA_GPIO_NUM_E i2c_sda_io;                /*!< GPIO number for I2C SDA signal, internal pullup */
    TUYA_GPIO_NUM_E i2c_scl_io;                /*!< GPIO number for I2C SCL signal, internal pullup */
    uint8_t i2s_id;                            /*!< I2S bus ID */
    TUYA_GPIO_NUM_E i2s_mck_io;                /*!< GPIO number for I2S master clock (MCK) signal */
    TUYA_GPIO_NUM_E i2s_bck_io;                /*!< GPIO number for I2S bit clock (BCK) signal */
    TUYA_GPIO_NUM_E i2s_ws_io;                 /*!< GPIO number for I2S word select (WS) signal */
    TUYA_GPIO_NUM_E i2s_do_io;                 /*!< GPIO number for I2S data output (DO) signal */
    TUYA_GPIO_NUM_E i2s_di_io;                 /*!< GPIO number for I2S data input (DI) signal */
    uint32_t dma_desc_num;
    uint32_t dma_frame_num;
    uint32_t sample_rate;
} TDD_AUDIO_CODEC_BUS_CFG_T;

/**
 * @brief Create a new audio codec I2C bus
 *
 * @param[in] cfg I2C bus configuration parameters
 * @param[out] handle Returns the created I2C bus handle
 *
 * @return OPERATE_RET 
 * @retval OPRT_OK Success
 * @retval Others Failure, see Tuya error codes for details
 */
OPERATE_RET tdd_audio_codec_bus_i2c_new(TDD_AUDIO_CODEC_BUS_CFG_T cfg, TDD_AUDIO_I2C_HANDLE *handle);

/**
 * @brief Create a new audio codec I2S bus
 *
 * @param[in] cfg I2S bus configuration parameters
 * @param[out] tx_handle Returns the created I2S transmit bus handle
 * @param[out] rx_handle Returns the created I2S receive bus handle
 *
 * @return OPERATE_RET 
 * @retval OPRT_OK Success
 * @retval Others Failure, see Tuya error codes for details
 */
OPERATE_RET tdd_audio_codec_bus_i2s_new(TDD_AUDIO_CODEC_BUS_CFG_T cfg, TDD_AUDIO_I2S_TX_HANDLE *tx_handle, TDD_AUDIO_I2S_RX_HANDLE *rx_handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_CODEC_BUS_H__ */
