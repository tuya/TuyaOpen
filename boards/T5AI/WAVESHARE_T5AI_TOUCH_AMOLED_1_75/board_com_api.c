/**
 * @file board_com_api.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "board_com_api.h"
#include "tkl_gpio.h"
#include "tal_api.h"

#include "tdd_audio.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#include "tdd_disp_co5300.h"
#include "tdd_tp_cst92xx.h"
#include "tdd_power_soc.h"

#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "qmi8658.h"

/***********************************************************
***********************macro define***********************
***********************************************************/
#define BOARD_PWR_EN_PIN           TUYA_GPIO_NUM_19
#define BOARD_PWR_EN_ACTIVE_LV     TUYA_GPIO_LEVEL_HIGH

#define BOARD_BUTTON_PWR_PIN       TUYA_GPIO_NUM_18
#define BOARD_BUTTON_PWR_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_SPEAKER_EN_PIN       TUYA_GPIO_NUM_28
   
#define BOARD_BUTTON_PIN           TUYA_GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LV     TUYA_GPIO_LEVEL_LOW
   
#define BOARD_LCD_RST_PIN          TUYA_GPIO_NUM_29
#define BOARD_LCD_QSPI_PORT        TUYA_QSPI_NUM_0
#define BOARD_LCD_QSPI_CLK         (80 * 1000000)

#define BOARD_LCD_POWER_PIN        TUYA_GPIO_NUM_MAX
   
#define BOARD_LCD_WIDTH            466
#define BOARD_LCD_HEIGHT           466
#define BOARD_LCD_X_OFFSET         6
#define BOARD_LCD_Y_OFFSET         0
#define BOARD_LCD_PIXELS_FMT       TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION         TUYA_DISPLAY_ROTATION_0
   
#define BOARD_TP_I2C_PORT          TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN       TUYA_GPIO_NUM_20
#define BOARD_TP_I2C_SDA_PIN       TUYA_GPIO_NUM_21
#define BOARD_TP_RST_PIN           TUYA_GPIO_NUM_42

// QMI8658 IMU sits on the same physical I2C bus as the touch panel.
#define BOARD_QMI8658_I2C_PORT     TUYA_I2C_NUM_0
#define BOARD_QMI8658_I2C_SCL_PIN  TUYA_GPIO_NUM_20
#define BOARD_QMI8658_I2C_SDA_PIN  TUYA_GPIO_NUM_21

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

    cfg.ai_chn = TKL_AI_0;
    cfg.sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.data_bits = TKL_AUDIO_DATABITS_16;
    cfg.channel = TKL_AUDIO_CHANNEL_MONO;

    cfg.spk_sample_rate = TKL_AUDIO_SAMPLE_16K;
    cfg.spk_pin = BOARD_SPEAKER_EN_PIN;
    cfg.spk_pin_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tdd_audio_register(AUDIO_CODEC_NAME, cfg));
#endif
    return rt;
}

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
    } break;

    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
        tkl_gpio_write(BOARD_PWR_EN_PIN, TUYA_GPIO_LEVEL_LOW);
    } break;

    default:
        break;
    }
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Power enable pin: active HIGH to turn on the board power supply.
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = BOARD_PWR_EN_ACTIVE_LV;
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(BOARD_PWR_EN_PIN, &pin_cfg));

    // Power button: active LOW to request power off (long press).
    BUTTON_GPIO_CFG_T button_2_hw_cfg = {
        .pin = BOARD_BUTTON_PWR_PIN,
        .level = BOARD_BUTTON_PWR_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_BUTTON_PWR_NAME, &button_2_hw_cfg));
    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    TUYA_CALL_ERR_RETURN(tdl_button_create(BOARD_BUTTON_PWR_NAME, &button_cfg, &button_hdl));
    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);
    
    // Register the button with the button manager so it can be polled and events generated.
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = BOARD_BUTTON_PIN,
        .level = BOARD_BUTTON_ACTIVE_LV,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
    DISP_QSPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_QSPI_DEVICE_CFG_T));

    display_cfg.bl.type = TUYA_DISP_BL_TP_CUSTOM;

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.x_offset  = BOARD_LCD_X_OFFSET;
    display_cfg.y_offset  = BOARD_LCD_Y_OFFSET;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port = BOARD_LCD_QSPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_QSPI_CLK;
    display_cfg.rst_pin = BOARD_LCD_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_co5300_register(DISPLAY_NAME, &display_cfg));

    TUYA_CALL_ERR_RETURN(tdl_disp_custom_backlight_register(DISPLAY_NAME,\
                                                            tdd_qspi_co5300_send_cmd_set_bl,\
                                                            NULL));

    TDD_TP_CST92XX_INFO_T cst92xx_info = {
        .rst_pin = BOARD_TP_RST_PIN,
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
                        .mirror_x = 1,
                        .mirror_y = 1,
                        .swap_xy = 0,
                    },
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_cst92xx_register(DISPLAY_NAME, &cst92xx_info));
#endif

    return rt;
}

/**
 * @brief Register QMI8658 IMU on the shared touch I2C bus (after display/touch init).
 *        The IMU is optional: if it is absent/unresponsive we log and carry on, so a
 *        missing sensor never fails board bring-up (power/display/etc. still come up).
 * @return Always OPRT_OK.
 */
static OPERATE_RET __board_register_qmi8658(void)
{
    tkl_io_pinmux_config(BOARD_QMI8658_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(BOARD_QMI8658_I2C_SDA_PIN, TUYA_IIC0_SDA);

    TUYA_IIC_BASE_CFG_T cfg = {
        .role       = TUYA_IIC_MODE_MASTER,
        .speed      = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT
    };
    tkl_i2c_init(BOARD_QMI8658_I2C_PORT, &cfg);

    if (OPRT_OK != qmi8658_init(&g_qmi8658_dev, QMI8658_ADDRESS_HIGH)) {
        PR_WARN("board: QMI8658 IMU not present, skipping (optional)");
    } else {
        PR_NOTICE("board: QMI8658 IMU ready (addr 0x%02X)", QMI8658_ADDRESS_HIGH);
    }
    
    return OPRT_OK;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
// Battery: ADC P13 (ch 15). Divider R12=2M 1% / R18=510K 1% -> (2M+510K)/510K = 4.922
// (confirmed against the board schematic, so this is the true ratio, not a trim).
#define BOARD_BAT_ADC_CH  15
#define BOARD_BAT_DIVIDER 4.922f
// Charger: single detect line CHRG -> P30 (LOW = charging), no STDBY line
#define BOARD_CHRG_PIN    TUYA_GPIO_NUM_30

static const TDD_POWER_ADC_BATTERY_T s_battery = {
    .adc_num = TUYA_ADC_NUM_0, .adc_ch = BOARD_BAT_ADC_CH, .divider_ratio = BOARD_BAT_DIVIDER,
    // Divider is exact per schematic, so the error is this chip's SAR gain+offset.
    // Two-point cal vs meter: (fw 4.08/meter 3.78) & (fw 4.55/meter 4.18) -> low/span.
    .cal_low = 2716, .cal_span = 2854, .samples = 16,
};
static const TDD_POWER_GPIO_CHARGER_T s_charger = {
    .chrg_pin = BOARD_CHRG_PIN, .chrg_active = TUYA_GPIO_LEVEL_LOW,
    .stdby_pin = TDD_POWER_PIN_NONE, .stdby_active = TUYA_GPIO_LEVEL_HIGH,
};
// LiPo OCV->SOC curve (ascending mV), from the WAVESHARE 11-point table.
static const TDL_POWER_OCV_PT_T s_ocv[] = {
    {2800, 0},  {3100, 10}, {3280, 20}, {3440, 30}, {3570, 40}, {3680, 50},
    {3780, 60}, {3880, 70}, {3980, 80}, {4090, 90}, {4200, 100},
};

static OPERATE_RET __board_register_power(void)
{
    TDD_POWER_SOC_CFG_T soc_cfg = {
        .battery = &s_battery,
        .charger = &s_charger,
        .info = {
            .battery = {
                .v_full_mv     = 4200, 
                .v_empty_mv    = 2800, 
                .v_low_mv      = 3300, 
                .v_critical_mv = 3100,
                .curve         = s_ocv, 
                .curve_cnt     = sizeof(s_ocv) / sizeof(s_ocv[0]),
            }
        },
    };


    // No switchable power domains on this board; battery(ADC) + charger(GPIO) only.
    return tdd_power_soc_register(POWER_NAME, &soc_cfg);
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_power());

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_display());

    TUYA_CALL_ERR_LOG(__board_register_qmi8658());

    return rt;
}
