/**
 * @file tuya_t5ai_eink_nfc.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and SD card peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tal_api.h"
#include "tdd_audio.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#include "tdd_disp_uc8276.h"
#include "tdd_tp_ft6336.h"
#include "board_power_domain_api.h"
#include "board_charge_detect_api.h"
#include "tkl_fs.h"
#include "tal_log.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_42

// Button pin definitions
#define BOARD_BUTTON_UP_PIN           TUYA_GPIO_NUM_22
#define BOARD_BUTTON_UP_ACTIVE_LV     TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_DOWN_PIN         TUYA_GPIO_NUM_23
#define BOARD_BUTTON_DOWN_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_ENTER_PIN        TUYA_GPIO_NUM_24
#define BOARD_BUTTON_ENTER_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_RETURN_PIN       TUYA_GPIO_NUM_25
#define BOARD_BUTTON_RETURN_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_LEFT_PIN         TUYA_GPIO_NUM_26
#define BOARD_BUTTON_LEFT_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_RIGHT_PIN        TUYA_GPIO_NUM_28
#define BOARD_BUTTON_RIGHT_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW

// LED pin definition
#define BOARD_LED_PIN       TUYA_GPIO_NUM_9
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

// E-Ink Display pin definitions (UC8276 400x300)
#define BOARD_EINK_WIDTH       400
#define BOARD_EINK_HEIGHT      300
#define BOARD_EINK_SPI_PORT    TUYA_SPI_NUM_0
#define BOARD_EINK_SPI_CLK     20000000 // 20MHz for E-Ink
#define BOARD_EINK_SPI_CS_PIN  TUYA_GPIO_NUM_45
#define BOARD_EINK_SPI_DC_PIN  TUYA_GPIO_NUM_47
#define BOARD_EINK_SPI_RST_PIN TUYA_GPIO_NUM_43
#define BOARD_EINK_SPI_CLK_PIN TUYA_GPIO_NUM_44

// #define BOARD_EINK_SPI_RST_PIN  TUYA_GPIO_NUM_7
#define BOARD_EINK_SPI_SCLK_PIN TUYA_GPIO_NUM_44
#define BOARD_EINK_SPI_SDI_PIN  TUYA_GPIO_NUM_46
#define BOARD_EINK_SPI_BUSY_PIN TUYA_GPIO_NUM_13
#define BOARD_EINK_BL_PIN       TUYA_GPIO_NUM_33 // Backlight/front light
#define BOARD_EINK_BL_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

// Touch Panel pin definitions (FT6336)
#define BOARD_TP_I2C_PORT    TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN     TUYA_GPIO_NUM_37
#define BOARD_TP_INTR_PIN    TUYA_GPIO_NUM_36

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
OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)

    TDD_AUDIO_T5AI_T cfg = {0};
    memset(&cfg, 0, sizeof(TDD_AUDIO_T5AI_T));

    cfg.aec_enable = 1;

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
    OPERATE_RET       rt = OPRT_OK;
    BUTTON_GPIO_CFG_T button_hw_cfg;

    memset(&button_hw_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));

    // register UP button
    button_hw_cfg.pin                = BOARD_BUTTON_UP_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_UP_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    // register DOWN button
    button_hw_cfg.pin                = BOARD_BUTTON_DOWN_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_DOWN_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_hw_cfg));

    // register ENTER button
    button_hw_cfg.pin                = BOARD_BUTTON_ENTER_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_ENTER_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_3, &button_hw_cfg));

    // register RETURN button
    button_hw_cfg.pin                = BOARD_BUTTON_RETURN_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_RETURN_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_4, &button_hw_cfg));

    // register LEFT button
    button_hw_cfg.pin                = BOARD_BUTTON_LEFT_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_LEFT_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_5, &button_hw_cfg));

    // register RIGHT button
    button_hw_cfg.pin                = BOARD_BUTTON_RIGHT_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_RIGHT_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_6, &button_hw_cfg));

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin   = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode  = TUYA_GPIO_PUSH_PULL;

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize E-Ink backlight GPIO and set to LOW (off)
    TUYA_GPIO_BASE_CFG_T bl_gpio_cfg = {0};
    bl_gpio_cfg.mode                 = TUYA_GPIO_PUSH_PULL;
    bl_gpio_cfg.direct               = TUYA_GPIO_OUTPUT;
    bl_gpio_cfg.level                = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(BOARD_EINK_BL_PIN, &bl_gpio_cfg);
    tkl_gpio_write(BOARD_EINK_BL_PIN, TUYA_GPIO_LEVEL_LOW);

#if defined(DISPLAY_NAME)
    // Ensure E-Ink power domain is enabled and stable
    board_power_domain_eink_3v3_enable();
    tal_system_sleep(50); // Wait for power stabilization

    // Configure SPI pinmux for E-Ink display (CS is GPIO controlled, not SPI peripheral)
    if(BOARD_EINK_SPI_CLK_PIN == TUYA_GPIO_NUM_44) {
        tkl_io_pinmux_config(TUYA_GPIO_NUM_45, TUYA_SPI0_CS);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_44, TUYA_SPI0_CLK);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_46, TUYA_SPI0_MOSI);
        tkl_io_pinmux_config(TUYA_GPIO_NUM_47, TUYA_SPI0_MISO);
    }

    // Configure E-Ink display (UC8276 400x300)
    DISP_EINK_UC8276_CFG_T eink_cfg;
    memset(&eink_cfg, 0, sizeof(DISP_EINK_UC8276_CFG_T));

    eink_cfg.width    = BOARD_EINK_WIDTH;
    eink_cfg.height   = BOARD_EINK_HEIGHT;
    eink_cfg.rotation = TUYA_DISPLAY_ROTATION_0;

    eink_cfg.port     = BOARD_EINK_SPI_PORT;
    eink_cfg.spi_clk  = BOARD_EINK_SPI_CLK;
    eink_cfg.cs_pin   = BOARD_EINK_SPI_CS_PIN;
    eink_cfg.dc_pin   = BOARD_EINK_SPI_DC_PIN;
    eink_cfg.rst_pin  = BOARD_EINK_SPI_RST_PIN;
    eink_cfg.busy_pin = BOARD_EINK_SPI_BUSY_PIN; // No busy pin connected

    // Set power pin to MAX to indicate no power control (prevents GPIO 0 init)
    eink_cfg.power.pin          = TUYA_GPIO_NUM_MAX;
    eink_cfg.power.active_level = TUYA_GPIO_LEVEL_HIGH;

    // Set backlight pin to MAX to indicate no backlight control
    // eink_cfg.bl.gpio.pin               = TUYA_GPIO_NUM_MAX;
    eink_cfg.bl.type               = TUYA_DISP_BL_TP_PWM;
    eink_cfg.bl.pwm.id             = TUYA_PWM_NUM_9;
    eink_cfg.bl.pwm.cfg.frequency  = 1000;              // PWM frequency in Hz
    eink_cfg.bl.pwm.cfg.duty       = 5000;              // Duty value (for 50%: duty=5000, cycle=10000)
    eink_cfg.bl.pwm.cfg.cycle      = 10000;             // Cycle value
    eink_cfg.bl.pwm.cfg.polarity   = TUYA_PWM_POSITIVE; // Normal polarity
    eink_cfg.bl.pwm.cfg.count_mode = TUYA_PWM_CNT_UP;   // Count up mode

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_mono_uc8276_register(DISPLAY_NAME, &eink_cfg));

    // Configure Touch Panel (FT6336)
    TDD_TP_FT6336_INFO_T ft6336_info = {
        .rst_pin  = BOARD_TP_RST_PIN,
        .intr_pin = BOARD_TP_INTR_PIN,
        .i2c_cfg =
            {
                .port    = BOARD_TP_I2C_PORT,
                .scl_pin = BOARD_TP_I2C_SCL_PIN,
                .sda_pin = BOARD_TP_I2C_SDA_PIN,
            },
        .tp_cfg =
            {
                .x_max = BOARD_EINK_WIDTH,
                .y_max = BOARD_EINK_HEIGHT,
                .flags =
                    {
                        .mirror_x = 0,
                        .mirror_y = 0,
                        .swap_xy  = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_ft6336_register(DISPLAY_NAME, &ft6336_info));
#endif

    PR_NOTICE("Initialized E-Ink display OK");
    return rt;
}

static OPERATE_RET __board_register_power_domains(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize power domains (EINK 3V3 and SD card 3V3) and enable by default
    rt = board_power_domain_init();
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

static OPERATE_RET __board_register_charge_detect(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize charge detect GPIO and interrupt
    // Disabled for now - interrupt mode not supported
    // rt = board_charge_detect_init();
    // if (OPRT_OK != rt) {
    //     return rt;
    // }

    // Initialize battery ADC reading (this works independently of charge detect interrupt)
    // rt = board_battery_adc_init();
    // if (OPRT_OK != rt) {
    //     PR_ERR("Failed to initialize battery ADC: %d", rt);
    //     return rt;
    // }

    return rt;
}

static OPERATE_RET __board_sdio_pin_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Configure SDIO pinmux
    tkl_io_pinmux_config(TUYA_GPIO_NUM_14, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_15, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_16, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_17, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_18, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_19, TUYA_SDIO_DATA3);

    // Ensure SD card power domain is enabled
    rt = board_power_domain_sd_3v3_enable();
    if (OPRT_OK != rt) {
        return rt;
    }

    // Small delay for power stabilization
    tal_system_sleep(10);

    return OPRT_OK;
}

/**
 * @brief Initialize SD card hardware (power and pinmux)
 *
 * This function ensures the SD card power domain is enabled and SDIO pins are configured.
 * After calling this, applications can mount the SD card using tkl_fs_mount().
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_sdcard_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Ensure SD card power domain is enabled
    rt = board_power_domain_sd_3v3_enable();
    if (OPRT_OK != rt) {
        return rt;
    }

    // Small delay for power stabilization
    tal_system_sleep(10);

    return OPRT_OK;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, SD card, power domains, display, TP) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_power_domains());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_display());
    // Charge detection may fail if interrupt mode not supported - make it non-fatal
    rt = __board_register_charge_detect();
    if (OPRT_OK != rt) {
        PR_WARN("Charge detection initialization failed: %d (continuing without it)", rt);
    }
    TUYA_CALL_ERR_LOG(__board_sdio_pin_register());

    return rt;
}
