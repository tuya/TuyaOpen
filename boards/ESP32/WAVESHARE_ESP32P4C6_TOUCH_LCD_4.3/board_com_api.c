/**
 * @file board_com_api.c
 * @brief Board-level hardware registration for WAVESHARE ESP32-P4-WIFI6 Touch LCD 4.3.
 *
 * Hardware details (from Waveshare BSP):
 * - Main MCU: ESP32-P4NRW32 (RISC-V dual-core + single-core)
 * - WiFi/BLE: ESP32-C6-MINI module (SDIO hosted)
 * - Flash: 32MB external NOR Flash
 * - PSRAM: 32MB (die-stacked, OPI)
 * - Display: 4.3" IPS LCD, 480x800, MIPI DSI 2-lane
 *   - LCD driver IC: ST7701 (JD9365 compatible)
 *   - Backlight: GPIO26 (LEDC PWM)
 *   - Reset: GPIO27
 * - Touch: GT911 I2C capacitive touch
 *   - I2C addr: 0x5D (or 0x14)
 *   - RST: GPIO23, INT: NC
 * - Audio: ES7210 (ADC) + ES8311 (DAC), dual MEMS mic
 *   - I2S: MCLK=13, SCLK=12, LCLK=10, DOUT=9, DSIN=11
 *   - Power Amp: GPIO53
 * - I2C: SDA=GPIO7, SCL=GPIO8 (shared: audio codec + touch)
 * - SD Card: SDMMC (D0=39, D1=40, D2=41, D3=42, CMD=44, CLK=43)
 *
 * MIPI DSI video timing (from Waveshare BSP display.h):
 *   H=480, V=800, HSYNC=12, HBP=42, HFP=42, VSYNC=8, VBP=2, VFP=60
 *   DPI clock: 30 MHz, lane bitrate: 500 Mbps
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"

#include "tdd_button_gpio.h"

#if defined(ENABLE_AUDIO) && defined(ENABLE_AUDIO_CODECS)
#include "tdd_audio_8311_codec.h"
#endif

#if defined(ENABLE_DISPLAY)
#include "tdd_disp_esp_st7701_dpi.h"
#endif

#if defined(ENABLE_TP)
#include "tdd_tp_esp_gt911.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* I2C (shared: audio codec + touch) */
#define I2C_NUM    (1)
#define I2C_SCL_IO (TUYA_GPIO_NUM_8)
#define I2C_SDA_IO (TUYA_GPIO_NUM_7)

/* Audio: I2S */
#define I2S_NUM     (1)
#define I2S_MCLK_IO (TUYA_GPIO_NUM_13)
#define I2S_SCLK_IO (TUYA_GPIO_NUM_12)
#define I2S_LCLK_IO (TUYA_GPIO_NUM_10)
#define I2S_DOUT_IO (TUYA_GPIO_NUM_9)
#define I2S_DSIN_IO (TUYA_GPIO_NUM_11)

#define GPIO_OUTPUT_PA (TUYA_GPIO_NUM_53)

/* Audio sample rates */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* Audio codec */
#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR   (0x30)

/* Display: 4.3" IPS, 480x800, MIPI DSI 2-lane, ST7701/JD9365 */
#define DISPLAY_WIDTH  (480)
#define DISPLAY_HEIGHT (800)

/* Backlight enable: GPIO26, active high (driven as plain GPIO = full on) */
#define DISPLAY_BL_PIN (TUYA_GPIO_NUM_26)

/* Touch: GT911 I2C capacitive touch */
#define TOUCH_RST_IO (TUYA_GPIO_NUM_23)
#define TOUCH_INT_IO (-1)

/* On-board button (BOOT button) */
#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_0
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

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

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin                = BOARD_BUTTON_PIN,
        .level              = BOARD_BUTTON_ACTIVE_LV,
        .mode               = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    return OPRT_OK;
}

#if defined(ENABLE_AUDIO) && defined(ENABLE_AUDIO_CODECS)
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
    cfg.i2s_mck_io = I2S_MCLK_IO;
    cfg.i2s_bck_io = I2S_SCLK_IO;
    cfg.i2s_ws_io = I2S_LCLK_IO;
    cfg.i2s_do_io = I2S_DOUT_IO;
    cfg.i2s_di_io = I2S_DSIN_IO;
    cfg.gpio_output_pa = GPIO_OUTPUT_PA;
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.default_volume = 80;

    TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}
#endif /* ENABLE_AUDIO && ENABLE_AUDIO_CODECS */

#if defined(ENABLE_DISPLAY)
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    LCD_ST7701_DPI_HW_CFG_T hw = {
        /* Hardware pins */
        .rst_io = TUYA_GPIO_NUM_27,

        /* Panel orientation */
        .mirror_x = false,
        .mirror_y = false,
        .swap_xy  = false,

        /* MIPI DSI PHY power */
        .phy_pwr_ldo_chan = 3,
        .phy_pwr_ldo_mv   = 2500,
    };

    TDD_DISP_ESP_LCD_CFG_T lcd_cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = 0,
        .bl = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = { .pin = DISPLAY_BL_PIN, .active_level = TUYA_GPIO_LEVEL_HIGH },
        },
    };

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_st7701_dpi_register(DISPLAY_NAME, &hw, &lcd_cfg));

    return OPRT_OK;
}
#endif /* ENABLE_DISPLAY */

#if defined(ENABLE_TP)
static OPERATE_RET __board_register_touch(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TP_ESP_GT911_CFG_T tp_cfg = {
        .i2c_port   = I2C_NUM,
        .i2c_scl_io = I2C_SCL_IO,
        .i2c_sda_io = I2C_SDA_IO,
        .rst_io     = TOUCH_RST_IO,
        .int_io     = TOUCH_INT_IO,
        .tp = {
            .tp_cfg = {
                .x_max = DISPLAY_WIDTH,
                .y_max = DISPLAY_HEIGHT,
                .flags = {
                    .swap_xy  = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
            },
        },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_esp_i2c_gt911_register(DISPLAY_NAME, &tp_cfg));

    return rt;
}
#endif /* ENABLE_TP */

/**
 * @brief Registers all hardware peripherals on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_AUDIO) && defined(ENABLE_AUDIO_CODECS)
    TUYA_CALL_ERR_LOG(__board_register_audio());
#endif

#if defined(ENABLE_BUTTON)
    TUYA_CALL_ERR_LOG(__board_register_button());
#endif

#if defined(ENABLE_DISPLAY)
    TUYA_CALL_ERR_LOG(__board_register_display());
#endif

#if defined(ENABLE_TP)
    TUYA_CALL_ERR_LOG(__board_register_touch());
#endif

    return rt;
}
