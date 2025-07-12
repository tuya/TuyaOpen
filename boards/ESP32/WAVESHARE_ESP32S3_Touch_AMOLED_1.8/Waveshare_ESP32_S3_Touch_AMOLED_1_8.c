/**
 * @file waveshare_esp32_s3_touch_amoled_1_8.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"

#include "tdd_audio_8311_codec.h"

#include "board_config.h"
#include "tca9554.h"
#include "lcd_sh8601.h"
#include "touch_ft5x06.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
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

    TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
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
    int rt = 0;

    rt = tca9554_init();
    if (rt != 0) {
        PR_ERR("tca9554_init failed");
        return rt;
    }
    uint32_t in_pin_mask = (1ULL << 0);   // io_0
    in_pin_mask |= (1ULL << 1);           // io_1
    in_pin_mask |= (1ULL << 2);           // io_2
    rt = tca9554_set_dir(in_pin_mask, 0); // set io_0, io_1, io_2 as output
    if (rt != 0) {
        PR_ERR("tca9554_set_dir failed");
        return rt;
    }
    uint32_t out_pin_mask = (1ULL << 4);
    rt = tca9554_set_dir(out_pin_mask, 1); // set io_4 as input
    if (rt != 0) {
        PR_ERR("tca9554_set_dir failed");
        return rt;
    }

    tca9554_set_level(in_pin_mask, 1); // set io_0, io_1, io_2 as high
    tal_system_sleep(100);
    tca9554_set_level(in_pin_mask, 0); // set io_0, io_1, io_2 as low
    tal_system_sleep(300);
    tca9554_set_level(in_pin_mask, 1); // set io_0, io_1, io_2 as high

    PR_DEBUG("tca9554_init success");

    rt = lcd_sh8601_init();
    if (rt != 0) {
        PR_ERR("lcd_sh8601_init failed");
        return rt;
    }

#if defined(LVGL_ENABLE_TOUCH) && LVGL_ENABLE_TOUCH
    rt = touch_ft5x06_init();
    if (rt != 0) {
        PR_ERR("touch_ft5x06_init failed");
        return rt;
    }
#endif // LVGL_ENABLE_TOUCH

    return 0;
}

void *board_display_get_panel_io_handle(void)
{
    return lcd_sh8601_get_panel_io_handle();
}

void *board_display_get_panel_handle(void)
{
    return lcd_sh8601_get_panel_handle();
}

void *board_touch_get_handle(void)
{
    return touch_ft5x06_get_handle();
}
