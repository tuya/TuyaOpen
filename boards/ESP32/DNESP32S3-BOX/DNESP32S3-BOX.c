/**
 * @file dnesp32s3-box.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#if USE_8311
#include "tdd_audio_8311_codec.h"
#else
#include "tdd_audio_atk_no_codec.h"
#endif

#include "board_config.h"
#include "lcd_st7789_80.h"
#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)

#if USE_8311
    TDD_AUDIO_8311_CODEC_T cfg = {0};
#else
    TDD_AUDIO_ATK_NO_CODEC_T cfg = {0};
#endif

    cfg.i2c_id = I2C_NUM;
    cfg.i2c_scl_io = I2C_SCL_IO;
    cfg.i2c_sda_io = I2C_SDA_IO;
    cfg.mic_sample_rate = I2S_INPUT_SAMPLE_RATE;
    cfg.spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE;
    cfg.i2s_id = I2S_NUM;
    cfg.i2s_mck_io = I2S_MCK_IO;
    cfg.i2s_bck_io = I2S_BCK_IO;
    cfg.i2s_ws_io = I2S_WS_IO;
    cfg.i2s_do_io = I2S_DO_IO;
    cfg.i2s_di_io = I2S_DI_IO;
    cfg.gpio_output_pa = GPIO_OUTPUT_PA;
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.default_volume = 80;

#if USE_8311
    TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
#else
    TUYA_CALL_ERR_RETURN(tdd_audio_atk_no_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

#endif
    return rt;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_audio());

    return rt;
}

int board_display_init(void)
{
    return lcd_st7789_80_init();
}

void *board_display_get_panel_io_handle(void)
{
    return lcd_st7789_80_get_panel_io_handle();
}

void *board_display_get_panel_handle(void)
{
    return lcd_st7789_80_get_panel_handle();
}