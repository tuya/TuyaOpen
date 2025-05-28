/**
 * @file tdd_audio_atk_no_codec.h
 * @brief tdd_audio_atk_no_codec module is used to
 * @version 1.0.0
 * @date 2025-04-29
 */

#ifndef __TDD_AUDIO_ATK_NO_CODEC_H__
#define  __TDD_AUDIO_ATK_NO_CODEC_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    uint8_t i2c_id;
    TUYA_GPIO_NUM_E i2c_sda_io;                /*!< GPIO number of I2C SDA signal, pulled-up internally */
    TUYA_GPIO_NUM_E i2c_scl_io;                /*!< GPIO number of I2C SCL signal, pulled-up internally */
    uint32_t mic_sample_rate;
    uint32_t spk_sample_rate;
    uint8_t i2s_id;
    TUYA_GPIO_NUM_E i2s_mck_io;
    TUYA_GPIO_NUM_E i2s_bck_io;
    TUYA_GPIO_NUM_E i2s_ws_io;
    TUYA_GPIO_NUM_E i2s_do_io;
    TUYA_GPIO_NUM_E i2s_di_io;
    TUYA_GPIO_NUM_E gpio_output_pa;
    uint8_t es8311_addr;
    uint32_t dma_desc_num;
    uint32_t dma_frame_num;
    int default_volume;
} TDD_AUDIO_ATK_NO_CODEC_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_audio_atk_no_codec_register(char *name, TDD_AUDIO_ATK_NO_CODEC_T cfg);

#ifdef __cplusplus
}
#endif

#endif /*  __TDD_AUDIO_ATK_NO_CODEC_H__ */
