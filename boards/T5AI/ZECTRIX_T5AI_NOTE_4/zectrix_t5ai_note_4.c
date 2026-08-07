/**
 * @file zectrix_t5ai_note_4.c
 * @author Tuya Inc.
 * @brief Board-level hardware registration for ZECTRIX_NOTE4_TY (T5-E1 e-paper note device).
 *
 * Registers audio (internal codec + NS4150B speaker amp), buttons, LED, SD card,
 * power domains and the SSD2683 e-paper display — see __board_register_display().
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
#include "tkl_wakeup.h" // register button pins as ULP wake sources
#include "tdd_disp_ssd2683.h"
#include "tdd_power_soc.h"
#include "tdl_power_manage.h"
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

// Power-domain load switches (all active-HIGH, on by default)
#define BOARD_PWR_EPD_PIN   TUYA_GPIO_NUM_23
#define BOARD_PWR_SD_PIN    TUYA_GPIO_NUM_8
#define BOARD_PWR_AUDIO_PIN TUYA_GPIO_NUM_42

// Battery: ADC_BAT -> P25 (ADC channel 1), divider VBAT->3.09M->tap->1M->GND (nominal x4.09).
// Gain-trimmed against a metered point (fw 3.86V vs meter 3.78V): 4.09 * 3780/3860 = 4.005.
#define BOARD_BAT_ADC_CH      1
#define BOARD_BAT_DIVIDER     4.005f
// Charger IP2332: CHRG_L -> P21 (low=charging), STDBY_H -> P22 (high=done)
#define BOARD_CHRG_PIN        TUYA_GPIO_NUM_21
#define BOARD_STDBY_PIN       TUYA_GPIO_NUM_22

// System power hold (the power-on-hold block on the schematic): PWR_ON(P44) gates
// Q7 to hold the battery->VIN P-MOS on. It powers up HIGH by default, so the board
// also boots on battery without this; we drive it HIGH explicitly to own the latch
// (guaranteed hold, robust against P44 being perturbed later, and the control point
// for a future power-off = drive LOW to cut power with the e-paper image retained).
#define BOARD_PWR_ON_PIN      TUYA_GPIO_NUM_44

/***********************************************************
***********************variable define**********************
***********************************************************/
static const TDD_POWER_GPIO_DOMAIN_T cPOWER_DOMAINS_TBL[] = {
    {TDL_PWR_DOMAIN_DISPLAY, BOARD_PWR_EPD_PIN,   TUYA_GPIO_LEVEL_HIGH, TRUE},
    {TDL_PWR_DOMAIN_SD,      BOARD_PWR_SD_PIN,    TUYA_GPIO_LEVEL_HIGH, TRUE},
    {TDL_PWR_DOMAIN_AUDIO,   BOARD_PWR_AUDIO_PIN, TUYA_GPIO_LEVEL_HIGH, TRUE},
};

static const TDD_POWER_ADC_BATTERY_T cBATTERY = {
    .adc_num       = TUYA_ADC_NUM_0, 
    .adc_ch        = BOARD_BAT_ADC_CH, 
    .divider_ratio = BOARD_BAT_DIVIDER,
    .cal_low       = 2469, 
    .cal_span      = 2429, 
    .samples       = 16, // NOTE4 metered two-point cal
};

static const TDD_POWER_GPIO_CHARGER_T cCHARGER = {
    .chrg_pin     = BOARD_CHRG_PIN, 
    .chrg_active  = TUYA_GPIO_LEVEL_LOW,
    .stdby_pin    = BOARD_STDBY_PIN, 
    .stdby_active = TUYA_GPIO_LEVEL_HIGH,
};

// Deep-sleep wakeup source(s): the ENTER key (active-low). The power manager arms
// these via tdl_power_enter_deepsleep() when the product deep-sleeps.
static const TDD_POWER_GPIO_WAKESRC_T cWAKESRC_TBL[] = {
    {BOARD_BUTTON_ENTER_PIN, 0},
};

// Ternary-lithium (NMC) OCV->SOC curve, ascending mV (single-cell pack).
static const TDL_POWER_OCV_PT_T cOCV_TBL[] = {
    {3000, 0},  {3350, 5},  {3480, 10}, {3550, 15}, {3600, 20}, {3630, 25}, {3660, 30},
    {3680, 35}, {3710, 40}, {3730, 45}, {3760, 50}, {3790, 55}, {3820, 60}, {3860, 65},
    {3900, 70}, {3950, 75}, {4000, 80}, {4050, 85}, {4100, 90}, {4150, 95}, {4200, 100},
};

static TDL_POWER_HANDLE sg_pwr_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_power(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_POWER_SOC_CFG_T power_cfg = {
        .domains     = cPOWER_DOMAINS_TBL,
        .domain_cnt  = sizeof(cPOWER_DOMAINS_TBL) / sizeof(cPOWER_DOMAINS_TBL[0]),
        .battery     = &cBATTERY,
        .charger     = &cCHARGER,
        .wakesrc     = cWAKESRC_TBL,
        .wakesrc_cnt = sizeof(cWAKESRC_TBL) / sizeof(cWAKESRC_TBL[0]),
        .info = {
            .battery = {
                .v_full_mv     = 4200, 
                .v_empty_mv    = 3000, 
                .v_low_mv      = 3400, 
                .v_critical_mv = 3300,
                .curve         = cOCV_TBL, 
                .curve_cnt     = sizeof(cOCV_TBL) / sizeof(cOCV_TBL[0])
            }
        },
    };

    TUYA_CALL_ERR_RETURN(tdd_power_soc_register(POWER_NAME, &power_cfg));

    sg_pwr_hdl = tdl_power_find(POWER_NAME);

    return (NULL == sg_pwr_hdl) ? OPRT_COM_ERROR : OPRT_OK;
}

static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;
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

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET       rt = OPRT_OK;
    BUTTON_GPIO_CFG_T button_hw_cfg;

    memset(&button_hw_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));
    // IRQ mode (not timer-scan): under ULP the scan timer stops when the CPU sleeps, so a
    // scan-mode button can't be felt in deep sleep. A GPIO IRQ wakes the CPU. Active-low
    // buttons -> press is a falling edge.
    button_hw_cfg.mode              = BUTTON_IRQ_MODE;
    button_hw_cfg.pin_type.irq_edge = TUYA_GPIO_IRQ_FALL;

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

    // Register all button pins as ULP wake sources so a press wakes the SoC out of LV
    // deep sleep (a plain GPIO IRQ does not wake it from deep sleep -> unresponsive
    // buttons under ULP). Harmless when low power is unused: a wake source only acts
    // during deep sleep and never affects the normal button IRQ/scan. Active-low -> LOW.
    const struct {
        TUYA_GPIO_NUM_E   pin;
        TUYA_GPIO_LEVEL_E active_lv;
    } wk_btn[] = {
        {BOARD_BUTTON_ENTER_PIN, BOARD_BUTTON_ENTER_ACTIVE_LV},
        {BOARD_BUTTON_PGDN_PIN,  BOARD_BUTTON_PGDN_ACTIVE_LV },
        {BOARD_BUTTON_PGUP_PIN,  BOARD_BUTTON_PGUP_ACTIVE_LV },
    };
    for (uint32_t i = 0; i < CNTSOF(wk_btn); i++) {
        TUYA_WAKEUP_SOURCE_BASE_CFG_T src = {0};
        src.source                          = TUYA_WAKEUP_SOURCE_GPIO;
        src.wakeup_para.gpio_param.gpio_num = wk_btn[i].pin;
        src.wakeup_para.gpio_param.level =
            (TUYA_GPIO_LEVEL_LOW == wk_btn[i].active_lv) ? TUYA_GPIO_WAKEUP_LOW : TUYA_GPIO_WAKEUP_HIGH;
        tkl_wakeup_source_set(&src);
    }

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_LED_PWM_CFG_T led_cfg = {0};

    led_cfg.pwm_ch       = BOARD_LED_PWM_CH;
    led_cfg.frequency    = BOARD_LED_PWM_FREQ;
    led_cfg.active_level = BOARD_LED_ACTIVE_LV;
    led_cfg.on_duty_pct  = BOARD_LED_ON_DUTY_PCT;
    led_cfg.duty_min_pct = BOARD_LED_DUTY_MIN_PCT; // 0
    led_cfg.duty_max_pct = BOARD_LED_DUTY_MAX_PCT; // 100 => breathing uses the full range

    TUYA_CALL_ERR_RETURN(tdd_led_pwm_register(LED_NAME, &led_cfg));

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Ensure the e-paper power domain is on and stable before talking to the panel.
    tdl_power_domain_set(sg_pwr_hdl, TDL_PWR_DOMAIN_DISPLAY, TRUE);
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

    rt = tdl_power_domain_set(sg_pwr_hdl, TDL_PWR_DOMAIN_SD, TRUE);
    if (OPRT_OK != rt) {
        return rt;
    }

    tal_system_sleep(10); // power stabilization

    return OPRT_OK;
}

/* Drive PWR_ON HIGH to own the power-hold latch (see BOARD_PWR_ON_PIN). */
static OPERATE_RET __board_power_hold(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(BOARD_PWR_ON_PIN, &cfg));
    return tkl_gpio_write(BOARD_PWR_ON_PIN, TUYA_GPIO_LEVEL_HIGH);
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_power_hold()); // own the power-hold latch (PWR_ON high)
    TUYA_CALL_ERR_LOG(__board_register_power());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_sdio_pin_register());

    return rt;
}
