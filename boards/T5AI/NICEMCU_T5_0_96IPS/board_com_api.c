/**
 * @file board_com_api.c
 * @brief NiceMCU-T5-0.96IPS board hardware registration
 * @version 0.2
 * @date 2026-08-10
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "board_com_api.h"

#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_st7735s.h"
#endif
#include "sh3001.h"

#include <math.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_8
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

/* Periodic SH3001 sample log for pose/axis bring-up (hold flat / upright / ~45°) */
#define BOARD_SH3001_DEBUG_POLL     1
#define BOARD_SH3001_POLL_MS        1000
#define BOARD_SH3001_POLL_STACK     2048

/* Chip->board: +45° about Y. Flat chip (-0.7,0,-0.7) → board (0,0,-1). */
#define BOARD_SH3001_INV_SQRT2      0.70710678f

/***********************************************************
***********************variable define**********************
***********************************************************/
static sh3001_dev_t s_sh3001_dev;
#if defined(BOARD_SH3001_DEBUG_POLL) && (BOARD_SH3001_DEBUG_POLL == 1)
static THREAD_HANDLE s_sh3001_poll_thrd = NULL;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Register onboard audio codec / speaker amp
 * @return OPRT_OK on success
 */
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_T5AI_T cfg;

    memset(&cfg, 0, sizeof(cfg));

#if defined(ENABLE_AUDIO_AEC) && (ENABLE_AUDIO_AEC == 1)
    cfg.aec_enable = 1;
#else
    cfg.aec_enable = 0;
#endif

    cfg.ai_chn = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits = TKL_AUDIO_DATABITS_16;
    cfg.channel = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
    PR_NOTICE("board: audio SPK_EN=P%d", BOARD_SPEAKER_EN_PIN);
#endif

    return rt;
}

/**
 * @brief Register optional GPIO button (P8, active-low) when BUTTON_NAME is enabled
 * @return OPRT_OK on success
 */
static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_IRQ_MODE,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

    return rt;
}

/*
 * Board-specific ST7735S init: default seq + INVON (0x21).
 * Many 0.96" 80x160 IPS panels show inverted colors without INVON
 * (light theme white bg appears black, black text appears white).
 */
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
static const uint8_t s_st7735s_init_seq[] = {
    1,    0,    0x01,
    1,    100,  0x11,
    4,    100,  0xB1, 0x02, 0x35, 0x36,
    4,    0,    0xB2, 0x02, 0x35, 0x36,
    7,    0,    0xB3, 0x02, 0x35, 0x36, 0x02, 0x35, 0x36,
    2,    0,    0xB4, 0x00,
    4,    0,    0xC0, 0xa2, 0x02, 0x84,
    2,    0,    0xC1, 0xC5,
    3,    0,    0xC2, 0x0D, 0x00,
    3,    0,    0xC3, 0x8A, 0x2A,
    3,    0,    0xC4, 0x8D, 0xEE,
    2,    0,    0xC5, 0x02,
    17,   0,    0xE0, 0x12, 0x1C, 0x10, 0x18, 0x33, 0x2C, 0x25, 0x28, 0x28, 0x27, 0x2F, 0x3C, 0x00, 0x03, 0x03, 0x10,
    17,   0,    0xE1, 0x12, 0x1C, 0x10, 0x18, 0x2D, 0x28, 0x23, 0x28, 0x28, 0x26, 0x2F, 0x3B, 0x00, 0x03, 0x03, 0x10,
    2,    0,    0x3A, 0x05,
    2,    0,    0x36, 0x08, /* MADCTL: BGR */
    1,    0,    0x21,       /* INVON: fix color inversion on this IPS module */
    1,    0,    0x29,
    1,    0,    0x2C,
    0
};
#endif

/**
 * @brief Register 0.96" ST7735S SPI TFT
 * @return OPRT_OK on success
 * @note SPI0 G0: SCK=P14 MOSI=P16 (SDA must be wired to P16). LCD CS=P34 as GPIO.
 */
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_SPI_DEVICE_CFG_T display_cfg;

    /* SPI0 hardware group G0: P14/P15/P16/P17 must be pinmuxed together */
    tkl_io_pinmux_config(TUYA_GPIO_NUM_14, TUYA_SPI0_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_15, TUYA_SPI0_CS);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_16, TUYA_SPI0_MOSI);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_17, TUYA_SPI0_MISO);

    memset(&display_cfg, 0, sizeof(display_cfg));

    display_cfg.bl.type = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.width = BOARD_LCD_WIDTH;
    display_cfg.height = BOARD_LCD_HEIGHT;
    display_cfg.x_offset = BOARD_LCD_X_OFFSET;
    display_cfg.y_offset = BOARD_LCD_Y_OFFSET;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;
    display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_set_init_seq(s_st7735s_init_seq));
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_register(DISPLAY_NAME, &display_cfg));
    PR_NOTICE("board: ST7735S %dx%d offset(%d,%d) SPI0 SCK=P%d MOSI=P%d CS=P%d DC=P%d RST=P%d BL=P%d",
              BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
              BOARD_LCD_X_OFFSET, BOARD_LCD_Y_OFFSET,
              BOARD_LCD_SPI_CLK_PIN, BOARD_LCD_SPI_MOSI_PIN,
              BOARD_LCD_SPI_CS_PIN, BOARD_LCD_SPI_DC_PIN,
              BOARD_LCD_SPI_RST_PIN, BOARD_LCD_BL_PIN);
    PR_NOTICE("board: ST7735S SDA(MOSI) must be on P16 (not P15) for T5AI HW SPI");
#endif

    return rt;
}

#if defined(BOARD_SH3001_DEBUG_POLL) && (BOARD_SH3001_DEBUG_POLL == 1)
/**
 * @brief Poll SH3001 and print accel/gyro for three-pose axis check
 * @param[in] arg unused
 * @return none
 * @note Hold still ~2s in each pose and read the log:
 *       1) flat on table  2) upright facing you  3) ~45° between those
 */
static void __board_sh3001_poll_task(void *arg)
{
    (void)arg;

    PR_NOTICE("==============================================");
    PR_NOTICE("SH3001 pose dump every %dms — hold each pose ~2s:", BOARD_SH3001_POLL_MS);
    PR_NOTICE("  A) flat on table (screen up)");
    PR_NOTICE("  B) upright, looking at screen normally");
    PR_NOTICE("  C) about 45 deg between A and B");
    PR_NOTICE("acc unit=mg (1000~=1g), gyr unit=dps");
    PR_NOTICE("==============================================");

    for (;;) {
        sh3001_data_t data;
        OPERATE_RET rt = sh3001_read_sensor_data(&s_sh3001_dev, &data);

        if (OPRT_OK == rt) {
            float bx, by, bz;
            int ax_mg = (int)(data.acc_x * 1000.0f);
            int ay_mg = (int)(data.acc_y * 1000.0f);
            int az_mg = (int)(data.acc_z * 1000.0f);
            int mag_mg = (int)(sqrtf(data.acc_x * data.acc_x + data.acc_y * data.acc_y +
                                     data.acc_z * data.acc_z) *
                               1000.0f);

            board_sh3001_map_accel_to_board(data.acc_x, data.acc_y, data.acc_z, &bx, &by, &bz);
            /* Flat should show board X≈0; upright should show |board X|≈1000 */
            PR_INFO("SH3001 raw[mg] X=%d Y=%d Z=%d |g|=%d | board[mg] X=%d Y=%d Z=%d | gyr X=%d Y=%d Z=%d%s",
                    ax_mg, ay_mg, az_mg, mag_mg, (int)(bx * 1000.0f), (int)(by * 1000.0f), (int)(bz * 1000.0f),
                    (int)(data.gyr_x), (int)(data.gyr_y), (int)(data.gyr_z),
                    (mag_mg < 850 || mag_mg > 1150) ? "  (HOLD STILL |g|~1000)" : "");
        } else {
            PR_ERR("SH3001 read fail rt=%d", rt);
        }

        tal_system_sleep(BOARD_SH3001_POLL_MS);
    }
}

/**
 * @brief Start SH3001 debug poll thread after successful init
 * @return none
 */
static void __board_sh3001_start_poll(void)
{
    THREAD_CFG_T cfg = {
        .stackDepth = BOARD_SH3001_POLL_STACK,
        .priority = THREAD_PRIO_1,
        .thrdname = "sh3001_poll",
    };

    if (NULL != s_sh3001_poll_thrd) {
        return;
    }

    if (OPRT_OK != tal_thread_create_and_start(&s_sh3001_poll_thrd, NULL, NULL,
                                               __board_sh3001_poll_task, NULL, &cfg)) {
        PR_ERR("board: SH3001 poll thread create failed");
        s_sh3001_poll_thrd = NULL;
    }
}
#endif

/**
 * @brief Bring up SH3001 on soft I2C0 (SCL=P42, SDA=P43)
 * @return Always OPRT_OK; missing sensor only warns so board bring-up continues
 */
static OPERATE_RET __board_register_sh3001(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };

    tkl_io_pinmux_config(BOARD_SH3001_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(BOARD_SH3001_SDA_PIN, TUYA_IIC0_SDA);

    rt = tkl_i2c_init(BOARD_SH3001_I2C_PORT, &i2c_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("board: SH3001 I2C init failed: %d", rt);
        return OPRT_OK;
    }

    rt = sh3001_init(&s_sh3001_dev, BOARD_SH3001_I2C_PORT, SH3001_I2C_ADDR_LOW);
    if (OPRT_OK != rt) {
        rt = sh3001_init(&s_sh3001_dev, BOARD_SH3001_I2C_PORT, SH3001_I2C_ADDR_HIGH);
    }

    if (OPRT_OK != rt) {
        PR_WARN("board: SH3001 IMU not present, skipping (optional)");
    } else {
        PR_NOTICE("board: SH3001 ready on IIC0 SCL=P%d SDA=P%d", BOARD_SH3001_SCL_PIN, BOARD_SH3001_SDA_PIN);
#if defined(BOARD_SH3001_DEBUG_POLL) && (BOARD_SH3001_DEBUG_POLL == 1)
        __board_sh3001_start_poll();
#endif
    }

    return OPRT_OK;
}

/**
 * @brief Get SH3001 device handle after board_register_hardware()
 * @return pointer to device, or NULL if not initialized
 */
sh3001_dev_t *board_sh3001_get_dev(void)
{
    if (false == s_sh3001_dev.initialized) {
        return NULL;
    }
    return &s_sh3001_dev;
}

/**
 * @brief Map SH3001 chip-frame accel to board frame (g)
 * @param[in] ax ay az chip-frame accel
 * @param[out] ax_b ay_b az_b board-frame accel
 * @return none
 */
void board_sh3001_map_accel_to_board(float ax, float ay, float az,
                                     float *ax_b, float *ay_b, float *az_b)
{
    if (NULL == ax_b || NULL == ay_b || NULL == az_b) {
        return;
    }
    /* +45° about Y: Xb=(X-Z)/√2, Zb=(X+Z)/√2 */
    *ax_b = (ax - az) * BOARD_SH3001_INV_SQRT2;
    *ay_b = ay;
    *az_b = (ax + az) * BOARD_SH3001_INV_SQRT2;
}

/**
 * @brief Map SH3001 chip-frame gyro to board frame (dps)
 * @param[in] gx gy gz chip-frame gyro
 * @param[out] gx_b gy_b gz_b board-frame gyro
 * @return none
 */
void board_sh3001_map_gyro_to_board(float gx, float gy, float gz,
                                    float *gx_b, float *gy_b, float *gz_b)
{
    if (NULL == gx_b || NULL == gy_b || NULL == gz_b) {
        return;
    }
    *gx_b = (gx - gz) * BOARD_SH3001_INV_SQRT2;
    *gy_b = gy;
    *gz_b = (gx + gz) * BOARD_SH3001_INV_SQRT2;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_sh3001());

    return rt;
}
