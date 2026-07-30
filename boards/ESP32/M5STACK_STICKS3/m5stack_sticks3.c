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
#include "tdd_power_m5pm1.h"
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

/* --- board power rails: internal bring-up helpers over the M5PM1 --- */

// L3B peripheral power (LCD BL / mic / spk). Also used by the display backlight callback.
static OPERATE_RET __sticks3_set_l3b(bool enable)
{
    bool level = enable ? M5PM1_L3B_POWER_ENABLE_LEVEL : M5PM1_L3B_POWER_DISABLE_LEVEL;
    return __m5pm1_gpio_output((M5PM1_GPIO_NUM_E)M5PM1_GPIO_L3B_POWER, level);
}

// Speaker amp SHDN, routed through M5PM1 PYG3 (forced to GPIO push-pull output).
static OPERATE_RET __sticks3_set_spk_amp(bool enable)
{
    bool level = enable ? M5PM1_SPK_AMP_ENABLE_LEVEL : M5PM1_SPK_AMP_DISABLE_LEVEL;
    return __m5pm1_gpio_output((M5PM1_GPIO_NUM_E)M5PM1_GPIO_SPK_AMP_SHDN, level);
}

// Enable every board power rail at bring-up. L1/L2-L3A/EXT_5V are direct M5PM1 switches.
// Caution: EXT_5V is enabled as OUTPUT — don't also feed external 5V into Grove/Hat.
static OPERATE_RET __sticks3_enable_all(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(m5pm1_set_charge_enable(true));
    TUYA_CALL_ERR_RETURN(m5pm1_set_ldo_enable(true));   // L1 IMU power (LDO)
    TUYA_CALL_ERR_RETURN(m5pm1_set_dcdc_enable(true));  // L2/L3A ESP32-side power (DCDC)
    TUYA_CALL_ERR_RETURN(m5pm1_set_boost_enable(true)); // EXT_5V output (Grove/Hat)
    TUYA_CALL_ERR_RETURN(__sticks3_set_l3b(true));      // L3B peripheral power
    return OPRT_OK;
}

// Single-cell LiPo OCV->SOC curve (ascending mV), for the power component to derive
// percentage from the M5PM1-reported battery voltage.
static const TDL_POWER_OCV_PT_T s_batt_ocv[] = {
    {3000, 0},  {3300, 10}, {3500, 20}, {3600, 30}, {3680, 40}, {3740, 50},
    {3800, 60}, {3880, 70}, {3960, 80}, {4060, 90}, {4200, 100},
};

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

    TUYA_CALL_ERR_RETURN(__sticks3_enable_all());

    /* Keep the speaker amplifier SHDN low until the audio output path opens. */
    TUYA_CALL_ERR_RETURN(__sticks3_set_spk_amp(false));

    tal_system_sleep(100);

    PR_NOTICE("StickS3 M5PM1 power initialized: src:%d wake:0x%02x L1 on, L2/L3A on, EXT_5V output on, "
              "3V3_L3B_AU on, PYG3_SPK_SHDN off",
              power_src, wake_src);

    // Expose battery + charger through the power component (domains are board
    // infrastructure, not exposed). CHRG status is on PYG0, active low = charging.
    TDD_POWER_M5PM1_CFG_T pwr_cfg = {
        // M5PM1 IRQ output (PYG1) is wired to ESP32 GPIO13 (schematic net PYG1_IRQ).
        .irq_valid = TRUE,
        .irq_pin   = TUYA_GPIO_NUM_13,
        .info = {.battery = {.v_full_mv     = 4200,
                             .v_empty_mv    = 3000,
                             .v_low_mv      = 3300,
                             .v_critical_mv = 3100,
                             .curve         = s_batt_ocv,
                             .curve_cnt     = sizeof(s_batt_ocv) / sizeof(s_batt_ocv[0])}},
    };
    TUYA_CALL_ERR_RETURN(tdd_power_m5pm1_register(POWER_NAME, &pwr_cfg));

    return OPRT_OK;
}

/**
 * @brief Enable or disable the StickS3 speaker amplifier.
 * @param[in] enable true to enable amplifier, false to disable amplifier.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __board_audio_pa_enable(bool enable)
{
    return __sticks3_set_spk_amp(enable);
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
    return __sticks3_set_l3b(brightness > 0);
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

