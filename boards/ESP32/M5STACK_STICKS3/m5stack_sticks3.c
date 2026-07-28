/**
 * @file m5stack_sticks3.c
 * @brief Board-level hardware registration for M5Stack StickS3.
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"

#include "board_com_api.h"
#include "board_config.h"
#include "tdd_disp_esp_st7789_spi.h"
#include "m5pm1_driver.h"
#include "tal_api.h"
#include "tdd_audio_8311_codec.h"
#include "tdl_display_manage.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#include "board_bmi270_api.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdd_button_gpio.h"
#endif

/* SPI host for LCD (SPI2_HOST = 1 in ESP-IDF) */
#ifndef LCD_SPI_HOST
#define LCD_SPI_HOST (1)
#endif

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Configure the TuyaOpen TKL I2C pinmux for the StickS3 internal bus.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_i2c_pinmux_config(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PIN_FUNC_E scl_func = TUYA_IIC1_SCL;
    TUYA_PIN_FUNC_E sda_func = TUYA_IIC1_SDA;

    switch (I2C_NUM) {
    case TUYA_I2C_NUM_0:
        scl_func = TUYA_IIC0_SCL;
        sda_func = TUYA_IIC0_SDA;
        break;
    case TUYA_I2C_NUM_1:
        scl_func = TUYA_IIC1_SCL;
        sda_func = TUYA_IIC1_SDA;
        break;
    default:
        PR_ERR("StickS3 unsupported TKL I2C port:%d", I2C_NUM);
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config((TUYA_PIN_NAME_E)I2C_SCL_IO, scl_func));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config((TUYA_PIN_NAME_E)I2C_SDA_IO, sda_func));

    PR_NOTICE("StickS3 I2C pinmux: port:%d SCL:%d SDA:%d", I2C_NUM, I2C_SCL_IO, I2C_SDA_IO);

    return rt;
}

/**
 * @brief Configure one M5PM1 GPIO as push-pull output.
 * @param[in] pin M5PM1 GPIO number.
 * @param[in] high true for high level, false for low level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_gpio_output(M5PM1_GPIO_NUM_E pin, bool high)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_wake_enable(pin, false));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_func(pin, M5PM1_GPIO_FUNC_GPIO));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_mode(pin, M5PM1_GPIO_MODE_OUTPUT));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_drive(pin, M5PM1_GPIO_DRIVE_PUSHPULL));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_pull(pin, M5PM1_GPIO_PULL_NONE));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_output(pin, high));

    return OPRT_OK;
}

/**
 * @brief Configure one M5PM1 GPIO as input.
 * @param[in] pin M5PM1 GPIO number.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __m5pm1_gpio_input(M5PM1_GPIO_NUM_E pin)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_func(pin, M5PM1_GPIO_FUNC_GPIO));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_mode(pin, M5PM1_GPIO_MODE_INPUT));

    return OPRT_OK;
}

/**
 * @brief Enable or disable StickS3 L1 IMU power.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l1(bool enable)
{
    return m5pm1_set_ldo_enable(enable);
}

/**
 * @brief Enable or disable StickS3 L2/L3A ESP32-S3-side power switch.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l2_l3a(bool enable)
{
    return m5pm1_set_dcdc_enable(enable);
}

/**
 * @brief Enable or disable StickS3 EXT_5V output mode.
 * @param[in] enable true for 5V output mode, false for 5V input mode.
 * @return OPRT_OK on success, error code on failure.
 * @attention Only enable output mode when external 5V is not supplied through Grove/Hat EXT_5V/5VIN.
 */
OPERATE_RET board_sticks3_power_set_ext_5v_output(bool enable)
{
    return m5pm1_set_boost_enable(enable);
}

/**
 * @brief Enable or disable StickS3 L3B peripheral power.
 * @param[in] enable true to power LCD backlight, microphone, and speaker peripherals.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l3b(bool enable)
{
    bool level = enable ? M5PM1_L3B_POWER_ENABLE_LEVEL : M5PM1_L3B_POWER_DISABLE_LEVEL;

    return __m5pm1_gpio_output((M5PM1_GPIO_NUM_E)M5PM1_GPIO_L3B_POWER, level);
}

/**
 * @brief Enable or disable StickS3 speaker amplifier.
 * @param[in] enable true to enable amplifier, false to disable amplifier.
 * @return OPRT_OK on success, error code on failure.
 * @note StickS3 routes the amplifier SHDN signal through M5PM1 PYG3
 *       (G3_WAKEin/IRQout/PWM13 alternate function). This API forces it to
 *       GPIO push-pull output and drives high to enable the amplifier.
 */
OPERATE_RET board_sticks3_power_set_speaker_amp(bool enable)
{
    bool level = enable ? M5PM1_SPK_AMP_ENABLE_LEVEL : M5PM1_SPK_AMP_DISABLE_LEVEL;

    return __m5pm1_gpio_output((M5PM1_GPIO_NUM_E)M5PM1_GPIO_SPK_AMP_SHDN, level);
}

/**
 * @brief Enable or disable StickS3 L1 IMU power hold while M5PM1 sleeps.
 * @param[in] enable true to keep L1 held during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l1_hold(bool enable)
{
    return m5pm1_ldo_set_power_hold(enable);
}

/**
 * @brief Enable or disable StickS3 EXT_5V power hold while M5PM1 sleeps.
 * @param[in] enable true to keep EXT_5V held during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 * @attention Only hold EXT_5V output when external 5V is not supplied through Grove/Hat EXT_5V/5VIN.
 */
OPERATE_RET board_sticks3_power_set_ext_5v_hold(bool enable)
{
    return m5pm1_boost_set_power_hold(enable);
}

/**
 * @brief Enable or disable StickS3 L3B peripheral power hold while M5PM1 sleeps.
 * @param[in] enable true to hold L3B state during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l3b_hold(bool enable)
{
    return m5pm1_gpio_set_power_hold((M5PM1_GPIO_NUM_E)M5PM1_GPIO_L3B_POWER, enable);
}

/**
 * @brief Enable or disable StickS3 speaker amplifier hold while M5PM1 sleeps.
 * @param[in] enable true to hold speaker amplifier state during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_speaker_amp_hold(bool enable)
{
    return m5pm1_gpio_set_power_hold((M5PM1_GPIO_NUM_E)M5PM1_GPIO_SPK_AMP_SHDN, enable);
}

/**
 * @brief Read and optionally clear StickS3 M5PM1 wake source flags.
 * @param[out] wake_source wake source bitmask from M5PM1.
 * @param[in] clear_after_read true to clear the returned flags.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_get_wake_source(uint8_t *wake_source, bool clear_after_read)
{
    M5PM1_CLEAN_E clean = clear_after_read ? M5PM1_CLEAN_ONCE : M5PM1_CLEAN_NONE;

    return m5pm1_get_wake_source(wake_source, clean);
}

/**
 * @brief Configure a timer wake before entering M5PM1 shutdown.
 * @param[in] seconds wake timer in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_timer_wake(uint32_t seconds)
{
    return m5pm1_timer_set(seconds, M5PM1_TIM_ACTION_POWERON);
}

/**
 * @brief Clear the M5PM1 timer wake configuration.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_clear_timer_wake(void)
{
    return m5pm1_timer_clear();
}

/**
 * @brief Configure BMI270 INT1 via M5PM1 PYG4 as a PMIC wake source.
 * @param[in] enable true to enable IMU wake source, false to disable it.
 * @param[in] rising_edge true for rising-edge wake, false for falling-edge wake.
 * @return OPRT_OK on success, error code on failure.
 * @note The BMI270 itself must be configured separately before this wake source can fire.
 */
OPERATE_RET board_sticks3_power_set_imu_wake(bool enable, bool rising_edge)
{
    OPERATE_RET rt = OPRT_OK;
    M5PM1_GPIO_WAKE_EDGE_E edge = rising_edge ? M5PM1_GPIO_WAKE_RISING : M5PM1_GPIO_WAKE_FALLING;

    if (!enable) {
        return m5pm1_gpio_set_wake_enable((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1, false);
    }

    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_func((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1, M5PM1_GPIO_FUNC_WAKE));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_mode((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1, M5PM1_GPIO_MODE_INPUT));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_wake_edge((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1, edge));
    TUYA_CALL_ERR_RETURN(m5pm1_gpio_set_wake_enable((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1, true));

    return OPRT_OK;
}

/**
 * @brief Request StickS3 PMIC shutdown.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_shutdown(void)
{
    return m5pm1_shutdown();
}

/**
 * @brief Request StickS3 PMIC reboot.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_reboot(void)
{
    return m5pm1_reboot();
}

/**
 * @brief Enable all currently supported StickS3 board power rails.
 * @return OPRT_OK on success, error code on failure.
 * @attention This enables EXT_5V output mode for bring-up. Do not feed external 5V into output interfaces.
 */
OPERATE_RET board_sticks3_power_enable_all(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* L0 is the always-on battery/PMIC domain. The rest are independent switches sourced from L0. */
    TUYA_CALL_ERR_LOG(m5pm1_set_charge_enable(true));
    TUYA_CALL_ERR_RETURN(board_sticks3_power_set_l1(true));
    TUYA_CALL_ERR_RETURN(board_sticks3_power_set_l2_l3a(true));
    TUYA_CALL_ERR_RETURN(board_sticks3_power_set_ext_5v_output(true));
    TUYA_CALL_ERR_RETURN(board_sticks3_power_set_l3b(true));

    return OPRT_OK;
}

/**
 * @brief Initialize StickS3 PMIC and board power rails.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_register_power(void)
{
    OPERATE_RET rt = OPRT_OK;
    M5PM1_PWR_SRC_E power_src = M5PM1_PWR_SRC_UNKNOWN;
    uint8_t wake_src = 0;
    M5PM1_CFG_T cfg = {
        .i2c_port = M5PM1_I2C_PORT,
        .i2c_addr = M5PM1_I2C_ADDR,
    };

    TUYA_CALL_ERR_RETURN(__board_i2c_pinmux_config());
    TUYA_CALL_ERR_RETURN(m5pm1_init(&cfg));
    TUYA_CALL_ERR_LOG(m5pm1_get_power_source(&power_src));
    TUYA_CALL_ERR_LOG(m5pm1_get_wake_source(&wake_src, M5PM1_CLEAN_ONCE));
    TUYA_CALL_ERR_LOG(m5pm1_set_i2c_sleep_time(0));

    /* PYG0 is the PMIC charge-status input in the official StickS3 power setup. */
    TUYA_CALL_ERR_RETURN(__m5pm1_gpio_input((M5PM1_GPIO_NUM_E)M5PM1_GPIO_CHARGE_STATUS));

    /* PYG4 is connected to the BMI270 INT1 signal for later wake-up/power-save work. */
    TUYA_CALL_ERR_RETURN(__m5pm1_gpio_input((M5PM1_GPIO_NUM_E)M5PM1_GPIO_IMU_INT1));

    TUYA_CALL_ERR_RETURN(board_sticks3_power_enable_all());

    /* Keep the speaker amplifier SHDN low until the audio output path opens. */
    TUYA_CALL_ERR_RETURN(board_sticks3_power_set_speaker_amp(false));

    tal_system_sleep(100);

    PR_NOTICE("StickS3 M5PM1 power initialized: src:%d wake:0x%02x L1 on, L2/L3A on, EXT_5V output on, "
              "3V3_L3B_AU on, PYG3_SPK_SHDN off",
              power_src, wake_src);

    return OPRT_OK;
}

/**
 * @brief Enable or disable the StickS3 speaker amplifier.
 * @param[in] enable true to enable amplifier, false to disable amplifier.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_audio_pa_enable(bool enable)
{
    return board_sticks3_power_set_speaker_amp(enable);
}

/**
 * @brief Probe the optional StickS3 ES8311 codec on the shared internal I2C bus.
 * @return true if the codec ACKs its 7-bit I2C address, false otherwise.
 */
static bool __board_audio_codec_present(void)
{
    return (tkl_i2c_master_send(I2C_NUM, AUDIO_CODEC_ES8311_ADDR_7BIT, NULL, 0, FALSE) == OPRT_OK);
}

/**
 * @brief Register StickS3 ES8311 audio codec.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_8311_CODEC_T cfg = {0};

    if (!__board_audio_codec_present()) {
        PR_NOTICE("StickS3 ES8311 not detected on shared I2C addr7:0x%02x; audio demo disabled",
                  AUDIO_CODEC_ES8311_ADDR_7BIT);
        return OPRT_OK;
    }

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
    cfg.pa_enable_cb = __board_audio_pa_enable;
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.default_volume = 70;

    TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}

/**
 * @brief Register StickS3 hardware buttons.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_register_button(void)
{
#if !defined(ENABLE_BUTTON) || (ENABLE_BUTTON != 1)
    return OPRT_OK;
#else
    OPERATE_RET rt = OPRT_OK;

#if defined(BUTTON_NAME)
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

#if ((defined(ENABLE_BUTTON_2) && (ENABLE_BUTTON_2 == 1)) || defined(BOARD_CHOICE_M5STACK_STICKS3)) && defined(BUTTON_NAME_2)
    BUTTON_GPIO_CFG_T button_2_hw_cfg = {
        .pin = BOARD_BUTTON_2_PIN,
        .level = BOARD_BUTTON_2_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_2_hw_cfg));
#endif

    return rt;
#endif
}

/**
 * @brief Custom backlight callback for StickS3.
 *
 * The LCD backlight is powered by the M5PM1 L3B rail (PYG2).  Brightness
 * is therefore on/off only – any non-zero value turns L3B on, zero turns it off.
 */
static OPERATE_RET __board_disp_set_backlight(uint8_t brightness, void *arg)
{
    (void)arg;
    /* L3B provides the power rail for LCD backlight, mic, and speaker.
     * The actual backlight LED enable is GPIO 38 (handled by GPIO backlight
     * type).  This callback keeps L3B in sync with brightness requests. */
    return board_sticks3_power_set_l3b(brightness > 0);
}

/**
 * @brief Register StickS3 display hardware.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = DISPLAY_SWAP_BYTES,
        .bl.type          = TUYA_DISP_BL_TP_GPIO,
        .bl.gpio.pin      = DISPLAY_BACKLIGHT_PIN,
        .bl.gpio.active_level = TUYA_GPIO_LEVEL_HIGH,
    };

    LCD_ST7789_SPI_HW_CFG_T hw = {
        .spi_host     = LCD_SPI_HOST,
        .sclk_io      = LCD_SCLK_PIN,
        .mosi_io      = LCD_MOSI_PIN,
        .cs_io        = LCD_CS_PIN,
        .dc_io        = LCD_DC_PIN,
        .rst_io       = LCD_RST_PIN,
        .invert_color = DISPLAY_COLOR_INVERT,
        .swap_xy      = DISPLAY_SWAP_XY,
        .mirror_x     = DISPLAY_MIRROR_X,
        .mirror_y     = DISPLAY_MIRROR_Y,
        .offset_x     = DISPLAY_OFFSET_X,
        .offset_y     = DISPLAY_OFFSET_Y,
        .pclk_hz      = 40 * 1000 * 1000, /* 40 MHz – flexible cable signal integrity limit */
    };

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_st7789_spi_register(DISPLAY_NAME, &hw, &cfg));
    /* Keep L3B power rail on for the backlight LED power path. */
    TUYA_CALL_ERR_RETURN(tdl_disp_custom_backlight_register(DISPLAY_NAME, __board_disp_set_backlight, NULL));
#endif

    return rt;
}

/**
 * @brief Register StickS3 BMI270 IMU on the shared internal I2C bus.
 * @return OPRT_OK on success, error code on failure.
 * @note 3V3_L1 power must already be enabled (done in power init).
 */
static OPERATE_RET __board_register_bmi270(void)
{
    OPERATE_RET rt = OPRT_OK;

    rt = board_bmi270_register();
    if (rt != OPRT_OK) {
        PR_ERR("StickS3 BMI270 registration failed: %d", rt);
    }

    return rt;
}

/**
 * @brief Configure StickS3 IR TX / RX GPIOs.
 * @return OPRT_OK on success, error code on failure.
 * @note The TuyaOpen IR peripheral framework does not yet support ESP32.
 *       This sets the pins as ready for application-level use.
 */
static OPERATE_RET __board_register_ir(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* IR TX (G46) – push-pull output */
    TUYA_GPIO_BASE_CFG_T tx_cfg = {
        .direct = TUYA_GPIO_OUTPUT,
        .mode   = TUYA_GPIO_PUSH_PULL,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init((TUYA_GPIO_NUM_E)IR_TX_IO, &tx_cfg));

    /* IR RX (G42) – input */
    TUYA_GPIO_BASE_CFG_T rx_cfg = {
        .direct = TUYA_GPIO_INPUT,
        .mode   = TUYA_GPIO_PULLUP,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init((TUYA_GPIO_NUM_E)IR_RX_IO, &rx_cfg));

    PR_NOTICE("StickS3 IR pins configured: TX:%d RX:%d", IR_TX_IO, IR_RX_IO);

    return rt;
}

/**
 * @brief Register all board hardware peripherals.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__board_register_power());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_bmi270());
    TUYA_CALL_ERR_LOG(__board_register_ir());

    return rt;
}

/**
 * @brief Show a simple centered red box on the board display.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_display_show_red_box(void)
{
#if defined(DISPLAY_NAME)
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_HANDLE_T disp_hdl = NULL;
    TDL_DISP_FRAME_BUFF_T *fb = NULL;

    disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if (disp_hdl == NULL) {
        PR_ERR("Display not found: %s", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_disp_dev_open(disp_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to open display: %s", DISPLAY_NAME);
        return rt;
    }

    /* Create a frame buffer for the full screen */
    fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    if (fb == NULL) {
        tdl_disp_dev_close(disp_hdl);
        return OPRT_MALLOC_FAILED;
    }

    /* Set frame buffer dimensions and position */
    fb->x_start = 0;
    fb->y_start = 0;
    fb->width   = DISPLAY_WIDTH;
    fb->height  = DISPLAY_HEIGHT;
    fb->fmt     = TUYA_PIXEL_FMT_RGB565;

    /* Clear to black */
    tdl_disp_draw_fill_full(fb, 0x0000, false);

    /* Draw a red box in the center */
    uint16_t box_x = (DISPLAY_WIDTH - DISPLAY_WIDTH / 2) / 2;
    uint16_t box_y = (DISPLAY_HEIGHT - DISPLAY_HEIGHT / 2) / 2;
    uint16_t box_w = DISPLAY_WIDTH / 2;
    uint16_t box_h = DISPLAY_HEIGHT / 2;

    TDL_DISP_RECT_T rect = {
        .x0 = box_x,
        .y0 = box_y,
        .x1 = box_x + box_w,
        .y1 = box_y + box_h,
    };
    tdl_disp_draw_fill(fb, &rect, 0xF800, false); /* Red in RGB565 */

    /* Flush to display */
    tdl_disp_dev_flush(disp_hdl, fb);

    PR_NOTICE("StickS3 red box displayed: %dx%d at (%d,%d)", box_w, box_h, box_x, box_y);

    tdl_disp_free_frame_buff(fb);

    /* Keep the display open so the content remains visible.
     * Closing would call esp_lcd_panel_disp_on_off(false), putting the
     * ST7789 to sleep and blanking the screen. */

    return rt;
#else
    return OPRT_COM_ERROR;
#endif
}

