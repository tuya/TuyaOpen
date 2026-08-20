/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_gpio.h"
#include "tkl_pinmux.h"

#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"
#include "tdd_disp_st7789.h"
#include "tdd_tp_cst816x.h"
#include "tdd_camera_ov2640.h"
#include "tdd_camera_gc2145.h"

/* Platform SDIO helpers (implemented in tuyaos_adapter) */
extern OPERATE_RET tkl_sdio_init(int port, const TUYA_SDIO_BASE_CFG_T *cfg);
extern void user_sdio_gpio_init(void);
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN         TUYA_GPIO_NUM_7

#define BOARD_BUTTON_PIN             TUYA_GPIO_NUM_8
#define BOARD_BUTTON_ACTIVE_LV       TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_9
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              240
#define BOARD_LCD_HEIGHT             320
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_90

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_47
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_6
#define BOARD_LCD_SPI_MISO_PIN       TUYA_GPIO_NUM_46
#define BOARD_LCD_SPI_CLK_PIN        TUYA_GPIO_NUM_44

#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX
#define BOARD_LCD_POWER_ACTIVE_LV    TUYA_GPIO_LEVEL_HIGH

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_1
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_23
#define BOARD_TP_INTR_PIN            TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_I2C_PORT        TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL         TUYA_GPIO_NUM_0
#define BOARD_CAMERA_I2C_SDA         TUYA_GPIO_NUM_1

#define BOARD_CAMERA_RST_PIN         TUYA_GPIO_NUM_MAX
#define BOARD_CAMERA_RST_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_POWER_PIN       TUYA_GPIO_NUM_MAX
#define BOARD_CAMERA_PWR_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_CLK             24000000

/* SDIO MODE1: GPIO14~GPIO19 */
#define BOARD_SDIO_CLK_PIN           TUYA_GPIO_NUM_14
#define BOARD_SDIO_CMD_PIN           TUYA_GPIO_NUM_15
#define BOARD_SDIO_D0_PIN            TUYA_GPIO_NUM_16
#define BOARD_SDIO_D1_PIN            TUYA_GPIO_NUM_17
#define BOARD_SDIO_D2_PIN            TUYA_GPIO_NUM_18
#define BOARD_SDIO_D3_PIN            TUYA_GPIO_NUM_19

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

#if defined(ENABLE_AUDIO_AEC) && (ENABLE_AUDIO_AEC == 1)
    cfg.aec_enable = 1;
#else
    cfg.aec_enable = 0;
#endif

    cfg.ai_chn      = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits   = TKL_AUDIO_DATABITS_16;
    cfg.channel     = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate  = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin          = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin   = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode  = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}


static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    // Composite Pinout from chip internal, muxing set the actual pinout for SPI0 interface
    if(BOARD_LCD_SPI_CLK_PIN == TUYA_GPIO_NUM_44) {
        tkl_io_pinmux_config(TUYA_GPIO_NUM_45, TUYA_SPI0_CS);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_44, TUYA_SPI0_CLK);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_46, TUYA_SPI0_MOSI);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_47, TUYA_SPI0_MISO);
    }

    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin    = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin    = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin   = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin          = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));


    TDD_TP_CST816X_INFO_T cst816x_info = {
        .rst_pin  = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INTR_PIN,
        .i2c_cfg =
            {
                .port = BOARD_TP_I2C_PORT,
                .scl_pin = BOARD_TP_I2C_SCL_PIN,
                .sda_pin = BOARD_TP_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = BOARD_LCD_WIDTH,
                .y_max = BOARD_LCD_HEIGHT,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_cst816x_register(DISPLAY_NAME, &cst816x_info));

#endif

    return rt;
}

static OPERATE_RET __board_register_camera(void)
{
#if defined(CAMERA_NAME)
    OPERATE_RET rt = OPRT_OK;
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr = {
            .pin = BOARD_CAMERA_POWER_PIN,
            .active_level = BOARD_CAMERA_PWR_ACTIVE_LV,
        },
        .rst = {
            .pin = BOARD_CAMERA_RST_PIN,
            .active_level = BOARD_CAMERA_RST_ACTIVE_LV,
        },
        .i2c ={
            .port = BOARD_CAMERA_I2C_PORT,
            .clk  = BOARD_CAMERA_I2C_SCL,
            .sda  = BOARD_CAMERA_I2C_SDA,
        },
        .clk = BOARD_CAMERA_CLK,
    };

    // TUYA_CALL_ERR_RETURN(tdd_camera_dvp_ov2640_register(CAMERA_NAME, &camera_cfg)); 
    TUYA_CALL_ERR_RETURN(tdd_camera_dvp_gc2145_register(CAMERA_NAME, &camera_cfg)); 
#endif

    return OPRT_OK;
}

/**
 * @brief Configure SDIO host pinmux for SD card (MODE1, GPIO14~GPIO19)
 * @return OPRT_OK on success
 */
OPERATE_RET board_sdcard_prepare(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_SDIO_BASE_CFG_T sdio_cfg = {0};

    /* 1-bit is enough for mount; D1~D3 still pinmuxed for 4-bit ready boards */
    sdio_cfg.bus_width = TUYA_SDIO_BUS_WIDTH_1BIT;
    sdio_cfg.speed_mode = TUYA_SDIO_SPEED_DEFAULT;
    sdio_cfg.voltage = TUYA_SDIO_VOLTAGE_3V3;
    sdio_cfg.clock_hz = 0;
    sdio_cfg.flags = 0;
    rt = tkl_sdio_init(TUYA_SDIO_NUM_0, &sdio_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_sdio_init failed: %d", rt);
        return rt;
    }

    tkl_io_pinmux_config(BOARD_SDIO_CLK_PIN, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(BOARD_SDIO_CMD_PIN, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(BOARD_SDIO_D0_PIN, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(BOARD_SDIO_D1_PIN, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(BOARD_SDIO_D2_PIN, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(BOARD_SDIO_D3_PIN, TUYA_SDIO_DATA3);

    /* Apply SDIO MODE1 mux + pull-up + drive strength now (not only at mount) */
    user_sdio_gpio_init();

    PR_NOTICE("SDIO MODE1 ready: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d",
              BOARD_SDIO_CLK_PIN, BOARD_SDIO_CMD_PIN, BOARD_SDIO_D0_PIN,
              BOARD_SDIO_D1_PIN, BOARD_SDIO_D2_PIN, BOARD_SDIO_D3_PIN);

    tal_system_sleep(200);

    return OPRT_OK;
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
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_camera());
    TUYA_CALL_ERR_LOG(board_sdcard_prepare());

    return rt;
}
