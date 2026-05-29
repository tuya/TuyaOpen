/**
 * @file dnesp32s3.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8388_codec.h"

#include "tdd_disp_esp_st7789_spi.h"
#include "board_com_api.h"

#include "xl9555.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* Audio sample rates */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (42)
#define I2C_SDA_IO (41)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (3)
#define I2S_BCK_IO (46)
#define I2S_WS_IO  (9)
#define I2S_DO_IO  (10)
#define I2S_DI_IO  (14)

/* Audio codec */
#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8388_ADDR   (0x20)

/* XL9555 IO expander */
#define IO_EXPANDER_XL9555_ADDR (0x20)

#define EX_IO_AP_INT   (0x0001 << 0)
#define EX_IO_QMA_INT  (0x0001 << 1)
#define EX_IO_SPK_EN   (0x0001 << 2)
#define EX_IO_BEEP     (0x0001 << 3)
#define EX_IO_OV_PWDN  (0x0001 << 4)
#define EX_IO_OV_RESET (0x0001 << 5)
#define EX_IO_GBC_LED  (0x0001 << 6)
#define EX_IO_GBC_KEY  (0x0001 << 7)
#define EX_IO_LCD_BL   (0x0001 << 8)
#define EX_IO_CTP_RST  (0x0001 << 9)
#define EX_IO_SLCD_RST (0x0001 << 10)
#define EX_IO_SLCD_PWR (0x0001 << 11)
#define EX_IO_KEY_3    (0x0001 << 12)
#define EX_IO_KEY_2    (0x0001 << 13)
#define EX_IO_KEY_1    (0x0001 << 14)
#define EX_IO_KEY_0    (0x0001 << 15)

/* LCD (ST7789 over single-line SPI) */
#define LCD_SPI_HOST (1) /* SPI2_HOST in ESP-IDF host enum */
#define LCD_SCLK_PIN (12)
#define LCD_MOSI_PIN (11)
#define LCD_DC_PIN   (40)
#define LCD_CS_PIN   (21)

#define DISPLAY_WIDTH                   (320)
#define DISPLAY_HEIGHT                  (240)
#define DISPLAY_SWAP_XY                 true
#define DISPLAY_MIRROR_X                true
#define DISPLAY_MIRROR_Y                false
#define DISPLAY_SWAP_BYTES              1
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

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
        .bl.type   = TUYA_DISP_BL_TP_NONE,
    };

    LCD_ST7789_SPI_HW_CFG_T hw = {
        .spi_host     = LCD_SPI_HOST,
        .sclk_io      = LCD_SCLK_PIN,
        .mosi_io      = LCD_MOSI_PIN,
        .cs_io        = LCD_CS_PIN,
        .dc_io        = LCD_DC_PIN,
        .rst_io       = -1,
        .invert_color = DISPLAY_BACKLIGHT_OUTPUT_INVERT,
        .swap_xy      = DISPLAY_SWAP_XY,
        .mirror_x     = DISPLAY_MIRROR_X,
        .mirror_y     = DISPLAY_MIRROR_Y,
    };

    return tdd_disp_esp_st7789_spi_register(DISPLAY_NAME, &hw, &cfg);
}