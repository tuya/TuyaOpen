/**
 * @file dnesp32s3.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "board_config.h"

#include "tal_api.h"

#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8388_codec.h"

#include "lcd_st7789_spi.h"
#include "board_com_api.h"

#include "xl9555.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static TDD_AUDIO_I2C_HANDLE i2c_bus_handle = NULL;
static TDD_AUDIO_I2S_TX_HANDLE i2s_tx_handle = NULL;
static TDD_AUDIO_I2S_RX_HANDLE i2s_rx_handle = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __io_expander_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = xl9555_init();
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_init failed: %d", rt);
        return rt;
    }

    uint32_t pin_out_mask = 0;
    pin_out_mask |= EX_IO_SPK_EN;
    pin_out_mask |= EX_IO_BEEP;
    pin_out_mask |= EX_IO_OV_PWDN;
    pin_out_mask |= EX_IO_OV_RESET;
    pin_out_mask |= EX_IO_GBC_LED;
    pin_out_mask |= EX_IO_GBC_KEY;
    pin_out_mask |= EX_IO_LCD_BL;
    pin_out_mask |= EX_IO_CTP_RST;
    pin_out_mask |= EX_IO_SLCD_RST;
    pin_out_mask |= EX_IO_SLCD_PWR;
    rt = xl9555_set_dir(pin_out_mask, 0); // Set output direction
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_set_dir out failed: %d", rt);
        return rt;
    }
    uint32_t pin_in_mask = ~pin_out_mask;
    rt = xl9555_set_dir(pin_in_mask, 1); // Set input direction
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_set_dir in failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_CODEC_BUS_CFG_T bus_cfg = {
        .i2c_id = I2C_NUM,
        .i2c_sda_io = I2C_SDA_IO,
        .i2c_scl_io = I2C_SCL_IO,
        .i2s_id = I2S_NUM,
        .i2s_mck_io = I2S_MCK_IO,
        .i2s_bck_io = I2S_BCK_IO,
        .i2s_ws_io = I2S_WS_IO,
        .i2s_do_io = I2S_DO_IO,
        .i2s_di_io = I2S_DI_IO,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .sample_rate = I2S_OUTPUT_SAMPLE_RATE,
    };

    tdd_audio_codec_bus_i2c_new(bus_cfg, &i2c_bus_handle);
    tdd_audio_codec_bus_i2s_new(bus_cfg, &i2s_tx_handle, &i2s_rx_handle);

    TDD_AUDIO_ES8388_CODEC_T codec = {
        .i2c_id = I2C_NUM,
        .i2c_handle = i2c_bus_handle,
        .i2s_id = I2S_NUM,
        .i2s_tx_handle = i2s_tx_handle,
        .i2s_rx_handle = i2s_rx_handle,
        .mic_sample_rate = I2S_INPUT_SAMPLE_RATE,
        .spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE,
        .es8388_addr = AUDIO_CODEC_ES8388_ADDR,
        .pa_pin = -1, /* The speaker power is controlled by XL9555. */
        .default_volume = 80,
    };
    TUYA_CALL_ERR_RETURN(tdd_audio_es8388_codec_register(AUDIO_CODEC_NAME, codec));

    xl9555_set_dir(EX_IO_SPK_EN, 0);
    xl9555_set_level(EX_IO_SPK_EN, 0); // Enable speaker
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

    TUYA_CALL_ERR_LOG(__io_expander_init());

    TUYA_CALL_ERR_LOG(__board_register_audio());

    return rt;
}

int board_display_init(void)
{
    return lcd_st7789_spi_init();
}

void *board_display_get_panel_io_handle(void)
{
    return lcd_st7789_spi_get_panel_io_handle();
}

void *board_display_get_panel_handle(void)
{
    return lcd_st7789_spi_get_panel_handle();
}