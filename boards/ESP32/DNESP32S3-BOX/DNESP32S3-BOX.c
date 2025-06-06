/**
 * @file dnesp32s3-box.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_config.h"
#include "lcd_st7789_80.h"
#include "board_com_api.h"

#include "xl9555.h"

#include "tdd_audio_8311_codec.h"
#include "tdd_audio_atk_no_codec.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_es8311; // 1-ES8311, 0-NS4168
} DNSESP32S3_BOX_CONFIG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static DNSESP32S3_BOX_CONFIG_T sg_dnesp32s3_box = {0};

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
    pin_out_mask |= EX_IO_BEEP;
    pin_out_mask |= EX_IO_CTP_RST;
    pin_out_mask |= EX_IO_LCD_BL;
    pin_out_mask |= EX_IO_LED_R;
    pin_out_mask |= EX_IO_1_2;
    pin_out_mask |= EX_IO_1_3;
    pin_out_mask |= EX_IO_1_4;
    pin_out_mask |= EX_IO_1_5;
    pin_out_mask |= EX_IO_1_6;
    pin_out_mask |= EX_IO_1_7;
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
    xl9555_set_dir(EX_IO_SPK_CTRL, 1);
    uint32_t read_level = 0;
    xl9555_get_level(EX_IO_SPK_CTRL, &read_level);
    PR_DEBUG("Speaker control level: 0x%04x", read_level);
    if (EX_IO_SPK_CTRL & read_level) {
        sg_dnesp32s3_box.is_es8311 = 1; // ES8311 codec
        PR_DEBUG("ES8311 codec is enabled");
    } else {
        sg_dnesp32s3_box.is_es8311 = 0; // NS4168 codec
        PR_DEBUG("NS4168 codec is enabled");
    }

    TDD_AUDIO_8311_CODEC_T cfg = {0};

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

    if (sg_dnesp32s3_box.is_es8311) {
        TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
    } else {
        TDD_AUDIO_ATK_NO_CODEC_T NS4168_cfg = {0};
        memcpy(&NS4168_cfg, &cfg, sizeof(TDD_AUDIO_ATK_NO_CODEC_T));
        TUYA_CALL_ERR_RETURN(tdd_audio_atk_no_codec_register(AUDIO_CODEC_NAME, NS4168_cfg));
    }

    xl9555_set_dir(EX_IO_SPK_CTRL, 0);
    xl9555_set_level(EX_IO_SPK_CTRL, 1); // Enable speaker
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
    int rt = lcd_st7789_80_init();
    if (rt != OPRT_OK) {
        PR_ERR("lcd_st7789_80_init failed: %d", rt);
        return rt;
    }

    xl9555_set_dir(EX_IO_LCD_BL, 0);
    xl9555_set_level(EX_IO_LCD_BL, 1); // Enable LCD backlight

    return 0;
}

void *board_display_get_panel_io_handle(void)
{
    return lcd_st7789_80_get_panel_io_handle();
}

void *board_display_get_panel_handle(void)
{
    return lcd_st7789_80_get_panel_handle();
}