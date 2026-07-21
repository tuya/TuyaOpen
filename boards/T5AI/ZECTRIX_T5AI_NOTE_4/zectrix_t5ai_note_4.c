/**
 * @file zectrix_t5ai_note_4.c
 * @author Tuya Inc.
 * @brief Board-level hardware registration for ZECTRIX_NOTE4_TY (T5-E1 e-paper note device).
 *
 * Registers audio (internal codec + NS4150B speaker amp), buttons, LED, SD card and
 * power domains. The e-paper display panel controller is not yet confirmed, so the
 * display is wired (pins/power) but not registered — see __board_register_display().
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tal_api.h"
#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#include "tdd_led_pwm.h"
#include "tdd_disp_ssd2683.h"
#include "board_power_domain_api.h"
#include "board_charge_detect_api.h"
#include "tkl_fs.h"
#include "tal_log.h"

/***********************************************************
************************macro define************************
***********************************************************/
// Speaker amplifier (NS4150B) control — PA_CTRL -> P45, CTRL active HIGH.
// The platform's spk_pin_polarity is the IDLE/MUTE level (it drives !polarity to
// enable), so the cfg passes TUYA_GPIO_LEVEL_LOW directly — see __board_register_audio().
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_45

// Button pin definitions (all active LOW with external pull-up)
#define BOARD_BUTTON_PGUP_PIN        TUYA_GPIO_NUM_12
#define BOARD_BUTTON_PGUP_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_PGDN_PIN        TUYA_GPIO_NUM_24
#define BOARD_BUTTON_PGDN_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_ENTER_PIN       TUYA_GPIO_NUM_43
#define BOARD_BUTTON_ENTER_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

// LED (green) — LED_G -> P34 (V1.1; was P20 on V1.0, now used by CHRG_EN), anode to
// 3V3 so active LOW (lit when the pin is LOW). Driven through the tdd_led_pwm driver on
// PWM8 (TUYA_PWM_NUM_3, fixed to P34). active_level = LOW tells the driver the LED lights
// on the low phase; it drives the PWM with inverted duty so the percentages below are the
// LED's lit fraction. Steady "on" is a dim 20 % charge indicator; breathing is available too.
#define BOARD_LED_PWM_CH        TUYA_PWM_NUM_3
#define BOARD_LED_PWM_FREQ      1000
#define BOARD_LED_ACTIVE_LV     TUYA_GPIO_LEVEL_LOW
#define BOARD_LED_ON_DUTY_PCT   20
// Breathing/dimming spans the full duty range on this LED.
#define BOARD_LED_DUTY_MIN_PCT  0
#define BOARD_LED_DUTY_MAX_PCT  100

// SDIO (4-bit) pin assignments
#define BOARD_SDIO_CLK_PIN   TUYA_GPIO_NUM_14
#define BOARD_SDIO_CMD_PIN   TUYA_GPIO_NUM_15
#define BOARD_SDIO_DATA0_PIN TUYA_GPIO_NUM_16
#define BOARD_SDIO_DATA1_PIN TUYA_GPIO_NUM_17
#define BOARD_SDIO_DATA2_PIN TUYA_GPIO_NUM_18
#define BOARD_SDIO_DATA3_PIN TUYA_GPIO_NUM_19

// E-paper display: SSD2683 400x300 B/W, software-SPI on GPIO
#define BOARD_EPD_WIDTH    400
#define BOARD_EPD_HEIGHT   300
#define BOARD_EPD_SDA_PIN  TUYA_GPIO_NUM_4 // MOSI (V1.1; was P6 on V1.0)
#define BOARD_EPD_SCK_PIN  TUYA_GPIO_NUM_2 // SCLK (V1.1; was P7 on V1.0)
#define BOARD_EPD_CS_PIN   TUYA_GPIO_NUM_3 // NCS  (V1.1; was P8 on V1.0)
#define BOARD_EPD_DC_PIN   TUYA_GPIO_NUM_9
#define BOARD_EPD_RST_PIN  TUYA_GPIO_NUM_28
#define BOARD_EPD_BUSY_PIN TUYA_GPIO_NUM_26

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_power_domains(void)
{
    // Initialize EPD / SD / audio 3V3 load switches and enable by default
    return board_power_domain_init();
}

static OPERATE_RET __board_register_audio(void)
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

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin         = BOARD_SPEAKER_EN_PIN;
    // NOTE: the platform treats spk_pin_polarity as the IDLE/MUTE level, not the
    // active level. It drives the pin to !polarity to ENABLE the amp. The NS4150B
    // CTRL is active-HIGH, so the idle/mute level is LOW. (All other T5AI boards
    // pass LOW here too.) Passing HIGH keeps the amp muted -> no sound.
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
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    // Enter: logical button1 (CONFIG_BUTTON_NAME, defaults to "button1"). Apps open
    // this name via the BUTTON_NAME macro; here it is bound to the ENTER key pin.
    button_hw_cfg.pin   = BOARD_BUTTON_ENTER_PIN;
    button_hw_cfg.level = BOARD_BUTTON_ENTER_ACTIVE_LV;
    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    // Page Down
    button_hw_cfg.pin   = BOARD_BUTTON_PGDN_PIN;
    button_hw_cfg.level = BOARD_BUTTON_PGDN_ACTIVE_LV;
    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_hw_cfg));

    // Page Up
    button_hw_cfg.pin   = BOARD_BUTTON_PGUP_PIN;
    button_hw_cfg.level = BOARD_BUTTON_PGUP_ACTIVE_LV;
    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_3, &button_hw_cfg));

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME)
    TDD_LED_PWM_CFG_T led_cfg = {0};
    led_cfg.pwm_ch       = BOARD_LED_PWM_CH;
    led_cfg.frequency    = BOARD_LED_PWM_FREQ;
    led_cfg.active_level = BOARD_LED_ACTIVE_LV;
    led_cfg.on_duty_pct  = BOARD_LED_ON_DUTY_PCT;
    led_cfg.duty_min_pct = BOARD_LED_DUTY_MIN_PCT; // 0
    led_cfg.duty_max_pct = BOARD_LED_DUTY_MAX_PCT; // 100 => breathing uses the full range

    TUYA_CALL_ERR_RETURN(tdd_led_pwm_register(LED_NAME, &led_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    // Ensure the e-paper power domain is on and stable before talking to the panel.
    board_power_domain_epd_3v3_enable();
    tal_system_sleep(50);

    // SSD2683 400x300 B/W, driven over software SPI (data lines are plain GPIO).
    DISP_EINK_SSD2683_CFG_T eink_cfg = {0};
    eink_cfg.width                   = BOARD_EPD_WIDTH;
    eink_cfg.height                  = BOARD_EPD_HEIGHT;
    eink_cfg.rotation                = TUYA_DISPLAY_ROTATION_0;
    eink_cfg.clk_pin                 = BOARD_EPD_SCK_PIN;
    eink_cfg.sda_pin                 = BOARD_EPD_SDA_PIN;
    eink_cfg.cs_pin                  = BOARD_EPD_CS_PIN;
    eink_cfg.dc_pin                  = BOARD_EPD_DC_PIN;
    eink_cfg.rst_pin                 = BOARD_EPD_RST_PIN;
    eink_cfg.busy_pin                = BOARD_EPD_BUSY_PIN;
    // Panel power is handled by the EPD power domain above, not by the driver.
    eink_cfg.power.pin          = TUYA_GPIO_NUM_MAX;
    eink_cfg.power.active_level = TUYA_GPIO_LEVEL_HIGH;
    // Use fast (non-flashing) refreshes, forcing a full de-ghost refresh every 10 frames.
    eink_cfg.full_refresh_period = 10;

    TUYA_CALL_ERR_RETURN(tdd_disp_sw_spi_mono_ssd2683_register(DISPLAY_NAME, &eink_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_sdio_pin_register(void)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_io_pinmux_config(BOARD_SDIO_CLK_PIN, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(BOARD_SDIO_CMD_PIN, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(BOARD_SDIO_DATA0_PIN, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(BOARD_SDIO_DATA1_PIN, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(BOARD_SDIO_DATA2_PIN, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(BOARD_SDIO_DATA3_PIN, TUYA_SDIO_DATA3);

    rt = board_power_domain_sd_3v3_enable();
    if (OPRT_OK != rt) {
        return rt;
    }

    tal_system_sleep(10); // power stabilization

    return OPRT_OK;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_power_domains());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_sdio_pin_register());

    return rt;
}
