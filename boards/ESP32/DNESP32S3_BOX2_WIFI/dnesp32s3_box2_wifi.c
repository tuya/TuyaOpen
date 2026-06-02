/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8389_codec.h"
#include "tkl_gpio.h"
#include "tkl_adc.h"
#include "tdd_disp_esp_st7789_80.h"
#include "board_com_api.h"

#include "xl9555.h"
#include "tdl_audio_driver.h"
#include "tdl_audio_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* Audio sample rates */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (47)
#define I2C_SDA_IO (48)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (38)
#define I2S_BCK_IO (40)
#define I2S_WS_IO  (42)
#define I2S_DO_IO  (41)
#define I2S_DI_IO  (39)

/* Audio codec */
#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8389_ADDR   (0x10 << 1)

/* XL9555 IO expander */
#define IO_EXPANDER_XL9555_ADDR (0x20)

#define EX_IO_SBU2     (0x0001 << 3)
#define EX_IO_SBU1     (0x0001 << 4)
#define EX_IO_KEY_L    (0x0001 << 5)
#define EX_IO_KEY_Q    (0x0001 << 6)
#define EX_IO_KEY_M    (0x0001 << 7)
#define EX_IO_USB_SEL  (0x0001 << 8)
#define EX_IO_SPK_EN   (0x0001 << 9)
#define EX_IO_SYS_POW  (0x0001 << 10)
#define EX_IO_VBUS_EN  (0x0001 << 11)
#define EX_IO_4G_EN    (0x0001 << 12)
#define EX_IO_3V3A_EN  (0x0001 << 13)
#define EX_IO_CHG_CTRL (0x0001 << 14)
#define EX_IO_CHRG     (0x0001 << 15)

/* LCD (ST7789 over i80 8-bit bus) */
#define LCD_I80_CS  (14)
#define LCD_I80_DC  (12)
#define LCD_I80_RD  (10)
#define LCD_I80_WR  (11)
#define LCD_I80_RST (-1)
#define LCD_I80_BL  (21)

#define LCD_I80_D0 (13)
#define LCD_I80_D1 (9)
#define LCD_I80_D2 (8)
#define LCD_I80_D3 (7)
#define LCD_I80_D4 (6)
#define LCD_I80_D5 (5)
#define LCD_I80_D6 (4)
#define LCD_I80_D7 (3)

#define DISPLAY_BACKLIGHT_PIN           LCD_I80_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#define DISPLAY_WIDTH      (320)
#define DISPLAY_HEIGHT     (240)
#define DISPLAY_SWAP_XY    true
#define DISPLAY_MIRROR_X   true
#define DISPLAY_MIRROR_Y   false
#define DISPLAY_SWAP_BYTES 1

/***********************************************************
********************function declaration********************
***********************************************************/
int board_display_init(void);

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

    XL9555_HW_CFG_T xl9555_hw = {
        .i2c_port = I2C_NUM,
        .scl_io   = I2C_SCL_IO,
        .sda_io   = I2C_SDA_IO,
        .dev_addr = IO_EXPANDER_XL9555_ADDR,
    };
    rt = xl9555_init(&xl9555_hw);
    if (rt != OPRT_OK) {
        PR_ERR("xl9555_init failed: %d", rt);
        return rt;
    }

    uint32_t pin_out_mask = 0;
    pin_out_mask |= EX_IO_SPK_EN;
    pin_out_mask |= EX_IO_SYS_POW;
    pin_out_mask |= EX_IO_VBUS_EN;
    pin_out_mask |= EX_IO_4G_EN;
    pin_out_mask |= EX_IO_3V3A_EN;
    pin_out_mask |= EX_IO_USB_SEL;
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

    xl9555_set_dir(EX_IO_SPK_EN, 0);
    xl9555_set_level(EX_IO_SPK_EN, 1); // Enable speaker (active high)
    xl9555_set_dir(EX_IO_3V3A_EN, 0);
    xl9555_set_level(EX_IO_3V3A_EN, 1); // Enable 3.3A (for codec power)
    xl9555_set_dir(EX_IO_VBUS_EN, 0);
    xl9555_set_level(EX_IO_VBUS_EN, 1); // Enable VBUS
    // xl9555_set_dir(EX_IO_4G_EN, 0);
    // xl9555_set_level(EX_IO_4G_EN, 1); // Enable 4G
    // xl9555_set_dir(EX_IO_USB_SEL, 0);
    // xl9555_set_level(EX_IO_USB_SEL, 1); // Slect USB
    // xl9555_set_dir(EX_IO_SYS_POW, 0);
    // xl9555_set_level(EX_IO_SYS_POW, 1); // Enable system

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

    TDD_AUDIO_ES8389_CODEC_T codec = {
        .i2c_id = I2C_NUM,
        .i2c_handle = i2c_bus_handle,
        .i2s_id = I2S_NUM,
        .i2s_tx_handle = i2s_tx_handle,
        .i2s_rx_handle = i2s_rx_handle,
        .mic_sample_rate = I2S_INPUT_SAMPLE_RATE,
        .spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE,
        .es8389_addr = AUDIO_CODEC_ES8389_ADDR,
        .pa_pin = -1, /* The speaker power is controlled by XL9555. */
        .default_volume = 80,
    };
    TUYA_CALL_ERR_RETURN(tdd_audio_es8389_codec_register(AUDIO_CODEC_NAME, codec));
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
    TUYA_CALL_ERR_LOG(board_display_init());

    return rt;
}

int board_display_init(void)
{
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = DISPLAY_SWAP_BYTES,
        .bl = {
            .type = TUYA_DISP_BL_TP_GPIO,
            .gpio = { .pin = DISPLAY_BACKLIGHT_PIN, .active_level = TUYA_GPIO_LEVEL_HIGH },
        },
    };

    LCD_ST7789_80_HW_CFG_T hw = {
        .rd_io    = LCD_I80_RD,
        .dc_io    = LCD_I80_DC,
        .wr_io    = LCD_I80_WR,
        .cs_io    = LCD_I80_CS,
        .rst_io   = LCD_I80_RST,
        .data_io  = {LCD_I80_D0, LCD_I80_D1, LCD_I80_D2, LCD_I80_D3,
                     LCD_I80_D4, LCD_I80_D5, LCD_I80_D6, LCD_I80_D7},
        .swap_xy  = DISPLAY_SWAP_XY,
        .mirror_x = DISPLAY_MIRROR_X,
        .mirror_y = DISPLAY_MIRROR_Y,
    };

    return tdd_disp_esp_st7789_80_register(DISPLAY_NAME, &hw, &cfg);
}