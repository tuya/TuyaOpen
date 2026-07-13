/**
 * @file dnesp32s3.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_gpio.h"

#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8388_codec.h"

#if defined(DNESP32S3_LCD_MD0240_SPI) && (DNESP32S3_LCD_MD0240_SPI == 1)
#include "tdd_disp_esp_st7789_spi.h"

#elif defined(DNESP32S3_LCD_MD0430R_RGB) && (DNESP32S3_LCD_MD0430R_RGB == 1)
#include "tdd_disp_esp_rgb.h"
#include "tdd_tp_esp_gt911.h"

#endif

#include "board_com_api.h"
#include "xl9555.h"
#include "tkl_pinmux.h"
#include "tkl_fs.h"
#include "tdd_button_esp_io_expander.h"
#include "tdd_led_esp_io_expander.h"
#include "tdd_led_gpio.h"

#if defined(ENABLE_DNESP32S3_CAMERA) && (ENABLE_DNESP32S3_CAMERA == 1)
#include "tdd_camera_esp_dvp.h"
#endif

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

/* SD card SPI3 */
#define SD_SPI_MOSI_IO 11
#define SD_SPI_SCLK_IO 12
#define SD_SPI_MISO_IO 13
#define SD_SPI_CS_IO   2

/* OV2640 DVP camera */
#define OV_I2C_NUM    (1)
#define OV_I2C_SCL_IO (38)
#define OV_I2C_SDA_IO (39)
#define OV_CAMERA_CLK (24000000) /* matches onboard 24 MHz crystal; feeds S3 DMA sample mode */

#if defined(DNESP32S3_LCD_MD0430R_RGB) && (DNESP32S3_LCD_MD0430R_RGB == 1)
/* LCD (ATK-MD0430R-800480 over RGB parallel, DE mode) */
#define DISPLAY_WIDTH   (800)
#define DISPLAY_HEIGHT  (480)
#define DISPLAY_PCLK_HZ (18 * 1000 * 1000) /* 18MHz - match Alientek for PSRAM bandwidth */
#define LCD_RGB_PCLK    (5)
#define LCD_RGB_DE      (4)
#define LCD_RGB_HSYNC   (-1) /* DE mode, no HSYNC */
#define LCD_RGB_VSYNC   (-1) /* DE mode, no VSYNC */
/* Data pins: D[0..4]=B3~B7, D[5..10]=G2~G7, D[11..15]=R3~R7 */
#define LCD_RGB_D0  (17)  /* B3 */
#define LCD_RGB_D1  (16)  /* B4 */
#define LCD_RGB_D2  (15)  /* B5 */
#define LCD_RGB_D3  (7)   /* B6 */
#define LCD_RGB_D4  (6)   /* B7 */
#define LCD_RGB_D5  (10)  /* G2 */
#define LCD_RGB_D6  (9)   /* G3 */
#define LCD_RGB_D7  (46)  /* G4 */
#define LCD_RGB_D8  (3)   /* G5 */
#define LCD_RGB_D9  (8)   /* G6 */
#define LCD_RGB_D10 (18)  /* G7 */
#define LCD_RGB_D11 (45)  /* R3 */
#define LCD_RGB_D12 (48)  /* R4 */
#define LCD_RGB_D13 (47)  /* R5 */
#define LCD_RGB_D14 (21)  /* R6 */
#define LCD_RGB_D15 (14)  /* R7 */
#else
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
#endif /* DNESP32S3_LCD_MD0430R_RGB */

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static TDD_AUDIO_I2C_HANDLE    i2c_bus_handle = NULL;
static TDD_AUDIO_I2S_TX_HANDLE i2s_tx_handle  = NULL;
static TDD_AUDIO_I2S_RX_HANDLE i2s_rx_handle  = NULL;
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
    rt                   = xl9555_set_dir(pin_in_mask, 1); // Set input direction
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
        .i2c_id        = I2C_NUM,
        .i2c_sda_io    = I2C_SDA_IO,
        .i2c_scl_io    = I2C_SCL_IO,
        .i2s_id        = I2S_NUM,
        .i2s_mck_io    = I2S_MCK_IO,
        .i2s_bck_io    = I2S_BCK_IO,
        .i2s_ws_io     = I2S_WS_IO,
        .i2s_do_io     = I2S_DO_IO,
        .i2s_di_io     = I2S_DI_IO,
        .dma_desc_num  = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .sample_rate   = I2S_OUTPUT_SAMPLE_RATE,
    };

    tdd_audio_codec_bus_i2c_new(bus_cfg, &i2c_bus_handle);
    tdd_audio_codec_bus_i2s_new(bus_cfg, &i2s_tx_handle, &i2s_rx_handle);

    TDD_AUDIO_ES8388_CODEC_T codec = {
        .i2c_id          = I2C_NUM,
        .i2c_handle      = i2c_bus_handle,
        .i2s_id          = I2S_NUM,
        .i2s_tx_handle   = i2s_tx_handle,
        .i2s_rx_handle   = i2s_rx_handle,
        .mic_sample_rate = I2S_INPUT_SAMPLE_RATE,
        .spk_sample_rate = I2S_OUTPUT_SAMPLE_RATE,
        .es8388_addr     = AUDIO_CODEC_ES8388_ADDR,
        .pa_pin          = -1, /* The speaker power is controlled by XL9555. */
        .default_volume  = 80,
    };
    TUYA_CALL_ERR_RETURN(tdd_audio_es8388_codec_register(AUDIO_CODEC_NAME, codec));

    xl9555_set_dir(EX_IO_SPK_EN, 0);
    xl9555_set_level(EX_IO_SPK_EN, 0); // Enable speaker
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DNESP32S3_LCD_MD0430R_RGB) && (DNESP32S3_LCD_MD0430R_RGB == 1)
    /* ATK-MD0430R-800480: 4.3" RGB parallel LCD, DE mode, no init sequence needed */
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = false, /* RGB panel hw does byte-order natively */
        .bl.type   = TUYA_DISP_BL_TP_NONE, /* BL via XL9555 */
    };

    TDD_DISP_ESP_RGB_HW_CFG_T hw = {
        .pclk_hz            = DISPLAY_PCLK_HZ,
        .h_res              = DISPLAY_WIDTH,
        .v_res              = DISPLAY_HEIGHT,
        .hsync_back_porch   = 88,
        .hsync_front_porch  = 40,
        .hsync_pulse_width  = 48,
        .vsync_back_porch   = 32,
        .vsync_front_porch  = 13,
        .vsync_pulse_width  = 3,
        .pclk_gpio          = LCD_RGB_PCLK,
        .de_gpio            = LCD_RGB_DE,
        .hsync_gpio         = LCD_RGB_HSYNC,
        .vsync_gpio         = LCD_RGB_VSYNC,
        .data_gpio          = {
            LCD_RGB_D0,  LCD_RGB_D1,  LCD_RGB_D2,  LCD_RGB_D3,
            LCD_RGB_D4,  LCD_RGB_D5,  LCD_RGB_D6,  LCD_RGB_D7,
            LCD_RGB_D8,  LCD_RGB_D9,  LCD_RGB_D10, LCD_RGB_D11,
            LCD_RGB_D12, LCD_RGB_D13, LCD_RGB_D14, LCD_RGB_D15,
        },
        .bounce_buffer_size = 480 * 10 * 2, /* 4800px bounce buf, match Alientek (saves internal SRAM) */
    };

    /* Enable backlight via XL9555 */
    xl9555_set_level(EX_IO_LCD_BL, 1);

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_rgb_register(DISPLAY_NAME, &hw, &cfg));

    /* GT9147/GT1151 touch on ATK-MD0430R, I2C addr 0x14 */
    /* Address select sequence: INT=HIGH during RST rising edge → addr 0x14 */
    TUYA_GPIO_BASE_CFG_T int_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    tkl_gpio_init(40, &int_cfg);    /* INT = HIGH → select address 0x14 */

    xl9555_set_level(EX_IO_CTP_RST, 0);  /* RST low */
    tal_system_sleep(10);
    xl9555_set_level(EX_IO_CTP_RST, 1);  /* RST high (address latched) */
    tal_system_sleep(5);

    /* Release INT pin (driver will reconfigure as input for interrupt) */
    int_cfg.direct = TUYA_GPIO_INPUT;
    int_cfg.mode   = TUYA_GPIO_PULLUP;
    tkl_gpio_init(40, &int_cfg);
    tal_system_sleep(50);

    TDD_TP_ESP_GT911_CFG_T tp_cfg = {
        .i2c_port   = 1,    /* Use I2C1 (I2C0 is used by XL9555/ES8388 on IO42/IO41) */
        .i2c_scl_io = 38,   /* CT_SCL = IO38 */
        .i2c_sda_io = 39,   /* CT_SDA = IO39 */
        .rst_io     = -1,   /* RST via XL9555, already toggled above */
        .int_io     = 40,   /* CT_INT = IO40 */
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

#elif defined(DNESP32S3_LCD_MD0240_SPI) && (DNESP32S3_LCD_MD0240_SPI == 1)
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

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_st7789_spi_register(DISPLAY_NAME, &hw, &cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_sd(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_DNESP32S3_SDCARD) && (ENABLE_DNESP32S3_SDCARD == 1)
    /* SD card SPI pinmux */
    tkl_io_pinmux_config(SD_SPI_MOSI_IO, TUYA_SPI1_MOSI);
    tkl_io_pinmux_config(SD_SPI_SCLK_IO, TUYA_SPI1_CLK);
    tkl_io_pinmux_config(SD_SPI_MISO_IO, TUYA_SPI1_MISO);
    tkl_io_pinmux_config(SD_SPI_CS_IO, TUYA_SPI1_CS);
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* KEY0~KEY3 on XL9555 IO1_4~IO1_7, active-low (pull-up) */
    const struct {
        const char *name;
        uint32_t    pin_mask;
    } key_tbl[] = {
        {BUTTON_NAME, EX_IO_KEY_0},
        {BUTTON_NAME_2, EX_IO_KEY_1},
        {BUTTON_NAME_3, EX_IO_KEY_2},
        {BUTTON_NAME_4, EX_IO_KEY_3},
    };

    for (uint32_t i = 0; i < sizeof(key_tbl) / sizeof(key_tbl[0]); i++) {
        TDD_BUTTON_IO_EXP_CFG_T hw_cfg = {
            .pin_mask     = key_tbl[i].pin_mask,
            .active_level = TUYA_GPIO_LEVEL_LOW,
            .init         = NULL,
            .set_dir      = xl9555_set_dir,
            .get_level    = xl9555_get_level,
        };
        TUYA_CALL_ERR_RETURN(tdd_button_esp_io_expander_register((char *)key_tbl[i].name, &hw_cfg));
    }

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    /* Onboard red LED on GPIO1, active-low (VCC3.3 → R4 → LED → IO1) */
    TDD_LED_GPIO_CFG_T led_cfg = {
        .pin   = 1,  /* GPIO1 */
        .mode  = TUYA_GPIO_PULLUP,
        .level = TUYA_GPIO_LEVEL_LOW,  /* active-low */
    };
    return tdd_led_gpio_register(LED_NAME, &led_cfg);
}

static OPERATE_RET __board_register_camera(void)
{
#if defined(ENABLE_DNESP32S3_CAMERA) && (ENABLE_DNESP32S3_CAMERA == 1)
    /* Power sequence: assert PWDN low (sensor on), pulse RESET via XL9555. */
    xl9555_set_level(EX_IO_OV_PWDN, 0);  /* PWDN active-low: drive low = powered on */
    xl9555_set_level(EX_IO_OV_RESET, 0); /* hold in reset */
    tal_system_sleep(50);
    xl9555_set_level(EX_IO_OV_RESET, 1); /* release reset */
    tal_system_sleep(20);  /* OV2640 reset recovery */

    /* DVP pin mapping for DNESP32S3:
     *   D0~D7  : IO4, IO5, IO6, IO7, IO15, IO16, IO17, IO18
     *   PCLK   : IO45   VSYNC : IO47   HREF : IO48
     *   SCCB   : SCL=IO38  SDA=IO39  (I2C port 1)
     *   PWDN / RESET: driven via XL9555 in the power sequence above.
     *   XCLK   : -1 (OV2640 module carries its own 24 MHz oscillator)
     */
    TDD_CAMERA_ESP_DVP_CFG_T cam_cfg = {
        .pin_pwdn      = -1,
        .pin_reset     = -1,
        .pin_xclk      = -1,
        .xclk_freq_hz  = OV_CAMERA_CLK,
        .pin_sccb_scl  = OV_I2C_SCL_IO,
        .pin_sccb_sda  = OV_I2C_SDA_IO,
        .sccb_i2c_port = -1,   /* Let SCCB_Init create its own I2C bus on port 1 */
        .pin_d0        = 4,
        .pin_d1        = 5,
        .pin_d2        = 6,
        .pin_d3        = 7,
        .pin_d4        = 15,
        .pin_d5        = 16,
        .pin_d6        = 17,
        .pin_d7        = 18,
        .pin_vsync     = 47,
        .pin_href      = 48,
        .pin_pclk      = 45,
    };
    return tdd_camera_esp_dvp_register(CAMERA_NAME, &cam_cfg);
#else
    return OPRT_OK;
#endif /* ENABLE_DNESP32S3_CAMERA */
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
    TUYA_CALL_ERR_LOG(__board_register_sd());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_camera());
    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
