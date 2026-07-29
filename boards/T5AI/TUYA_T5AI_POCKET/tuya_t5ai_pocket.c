/**
 * @file tuya_t5ai_pocket.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, LED, and LCD display
 * peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tkl_pinmux.h"
#include "tal_api.h"
#include "tdd_audio.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"
#include "tdd_disp_st7305.h" // Add LCD driver header
#include "tdd_joystick.h"
#include "board_audio_mux_api.h" // Add audio mux API header
#include "board_bmi270_api.h"    // Add BMI270 sensor API header
#include "tkl_gpio.h"
#include "tdd_power_axp2101.h"
#include "tdd_power_soc.h"
#include "tdl_power_manage.h"

// AXP2101 channel map (was board_axp2101_api.h)
#define RTC_VDD        XPOWERS_LDO1
#define VDD_CAM_2V8    XPOWERS_ALDO3
#define VDD_SD_3V3     XPOWERS_ALDO4
#define AVDD_CAM_2V8   XPOWERS_BLDO1
#define DVDD_CAM_1V8   XPOWERS_BLDO2
#define VDD_JOYCON_1V1 XPOWERS_DLDO2
// 4G module control GPIOs (SoC): RST high=working; SIM_VDD is the CELLULAR power domain
#define RST_4G_MODULE_CTRL     TUYA_GPIO_NUM_25
#define SIM_VDD_4G_MODULE_CTRL TUYA_GPIO_NUM_22
#include "tdd_camera_gc2145.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_42

#define BOARD_BUTTON_PIN        TUYA_GPIO_NUM_17
#define BOARD_BUTTON_ACTIVE_LV  TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON2_PIN       TUYA_GPIO_NUM_18
#define BOARD_BUTTON2_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON3_PIN       TUYA_GPIO_NUM_19
#define BOARD_BUTTON3_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON4_PIN       TUYA_GPIO_NUM_26
#define BOARD_BUTTON4_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#define BOARD_LED_PIN       TUYA_GPIO_NUM_28
#define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH

// Audio Mux Router: Controls signal routing to the MIC2 input of the codec.
//   - SEL = Low  : Route 1 (Microphone input to MIC2)
//   - SEL = High : Route 2 (Speaker loopback to MIC2)
// The SEL pin determines whether MIC2 receives audio from the microphone or from the speaker loopback.
#define BOARD_AUDIO_MUX_SEL_PIN         TUYA_GPIO_NUM_23
#define BOARD_AUDIO_MUX_SEL_MIC_LV      TUYA_GPIO_LEVEL_LOW
#define BOARD_AUDIO_MUX_SEL_LOOPBACK_LV TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_BL_TYPE TUYA_DISP_BL_TP_NONE

#define BOARD_LCD_WIDTH    168
#define BOARD_LCD_HEIGHT   384
#define BOARD_LCD_X_OFFSET 0x17
#define BOARD_LCD_Y_OFFSET 0

#if defined(TUYA_T5AI_POCKET_LCD_ROTATION_0) && (TUYA_T5AI_POCKET_LCD_ROTATION_0)
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_0
#elif defined(TUYA_T5AI_POCKET_LCD_ROTATION_90) && (TUYA_T5AI_POCKET_LCD_ROTATION_90)
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_90
#elif defined(TUYA_T5AI_POCKET_LCD_ROTATION_180) && (TUYA_T5AI_POCKET_LCD_ROTATION_180)
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_180
#elif defined(TUYA_T5AI_POCKET_LCD_ROTATION_270) && (TUYA_T5AI_POCKET_LCD_ROTATION_270)
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_270
#else
#define BOARD_LCD_ROTATION TUYA_DISPLAY_ROTATION_270
#endif

#define BOARD_LCD_SPI_PORT     TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK      48000000
#define BOARD_LCD_SPI_CS_PIN   TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_DC_PIN   TUYA_GPIO_NUM_47
#define BOARD_LCD_SPI_RST_PIN  TUYA_GPIO_NUM_43
#define BOARD_LCD_SPI_MISO_PIN TUYA_GPIO_NUM_46
#define BOARD_LCD_SPI_CLK_PIN  TUYA_GPIO_NUM_44

#define BOARD_LCD_POWER_PIN TUYA_GPIO_NUM_MAX

#define BOARD_JOYSTICK_PIN        TUYA_GPIO_NUM_9
#define BOARD_JOYSTICK_ADC_NUM    TUYA_ADC_NUM_0
#define BOARD_JOYSTICK_ADC_WIDTH  12
#define BOARD_JOYSTICK_ADC_CH_NUM 2
#define BOARD_JOYSTICK_ADC_CH_X   15
#define BOARD_JOYSTICK_ADC_CH_Y   14
#define BOARD_JOYSTICK_MODE       JOYSTICK_TIMER_SCAN_MODE

#define BOARD_CAMERA_I2C_PORT TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL  TUYA_GPIO_NUM_20
#define BOARD_CAMERA_I2C_SDA  TUYA_GPIO_NUM_21

#define BOARD_CAMERA_RST_PIN TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_POWER_PIN TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_CLK 24000000

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

#if defined(BUTTON_NAME)
    button_hw_cfg.pin                = BOARD_BUTTON_PIN;
    button_hw_cfg.level              = BOARD_BUTTON_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));
#endif

#if defined(BUTTON_NAME_2)
    button_hw_cfg.pin                = BOARD_BUTTON2_PIN;
    button_hw_cfg.level              = BOARD_BUTTON2_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_2, &button_hw_cfg));
#endif

#if defined(BUTTON_NAME_3)
    button_hw_cfg.pin                = BOARD_BUTTON3_PIN;
    button_hw_cfg.level              = BOARD_BUTTON3_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_3, &button_hw_cfg));
#endif

#if defined(BUTTON_NAME_4)
    button_hw_cfg.pin                = BOARD_BUTTON4_PIN;
    button_hw_cfg.level              = BOARD_BUTTON4_ACTIVE_LV;
    button_hw_cfg.mode               = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME_4, &button_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(LED_NAME)
    TDD_LED_GPIO_CFG_T led_gpio;

    led_gpio.pin   = BOARD_LED_PIN;
    led_gpio.level = BOARD_LED_ACTIVE_LV;
    led_gpio.mode  = TUYA_GPIO_PUSH_PULL;

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));
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

    display_cfg.bl.type = BOARD_LCD_BL_TYPE;

    display_cfg.width    = BOARD_LCD_WIDTH;
    display_cfg.height   = BOARD_LCD_HEIGHT;
    display_cfg.x_offset = BOARD_LCD_X_OFFSET;
    display_cfg.y_offset = BOARD_LCD_Y_OFFSET;
    display_cfg.rotation = BOARD_LCD_ROTATION;

    display_cfg.port    = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin  = BOARD_LCD_SPI_CS_PIN;
    display_cfg.dc_pin  = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_mono_st7305_register(DISPLAY_NAME, &display_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_joystick(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(JOYSTICK_NAME)
    JOYSTICK_GPIO_CFG_T joystick_hw_cfg = {
        .btn_pin            = BOARD_JOYSTICK_PIN,
        .mode               = BOARD_JOYSTICK_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
        .level              = TUYA_GPIO_LEVEL_LOW,
        .adc_num            = BOARD_JOYSTICK_ADC_NUM,
        .adc_ch_x           = BOARD_JOYSTICK_ADC_CH_X,
        .adc_ch_y           = BOARD_JOYSTICK_ADC_CH_Y,
        .adc_cfg =
            {
                .ch_list.data = (1 << BOARD_JOYSTICK_ADC_CH_X) | (1 << BOARD_JOYSTICK_ADC_CH_Y),
                .ch_nums      = BOARD_JOYSTICK_ADC_CH_NUM, /* adc Number of channel lists */
                .width        = BOARD_JOYSTICK_ADC_WIDTH,
                .mode         = TUYA_ADC_CONTINUOUS,
                .type         = TUYA_ADC_INNER_SAMPLE_VOL,
                .conv_cnt     = 1,
            },
    };

    TUYA_CALL_ERR_RETURN(tdd_joystick_register(JOYSTICK_NAME, &joystick_hw_cfg));

#endif

    return rt;
}

static OPERATE_RET __board_register_audio_mux_router(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize the audio multiplexer with default microphone route
    rt = board_audio_mux_init();
    if (OPRT_OK != rt) {
        return rt;
    }

    return rt;
}

static OPERATE_RET __board_register_bmi270_sensor(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Register BMI270 sensor
    rt = board_bmi270_register();
    if (OPRT_OK != rt) {
        PR_ERR("BMI270 sensor registration failed: %d", rt);
        return rt;
    }

    return rt;
}

static OPERATE_RET __board_register_axp2101(void)
{

    /* AXP2101 Power supply configuration for T5 Pocket
    ----- VBUS -----
    - 5V 500mA (Charging)
    ----- BAT Specs -----
    - 3V7 525mAh
    ----- VSYS Domain -----
    - 3V-4.2V (Charging/Discharging)
    ----- DCDC -----
    - DCDC1/LX1: VDD3V3 MCU
    - DCDC2/LX2: None
    - DCDC3/LX3: None
    - DCDC4/LX4: None
    ----- LDO -----
    - BLDO1: 2V8 Camera AVDD
    - BLDO2: 1V1 Camera
    ----- ALDO -----
    - ALDO1: None
    - ALDO2: None
    - ALDO3: 2V8 Camera VDDCAM
    - ALDO4: 3V3 SD Card
    - DLOO1: None
    -----END-----
    */

    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gc = {.mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};

    // --- AXP2101 contributor: chip bring-up + app domains + fuel gauge + charge ---
    // App-visible rails (role -> channel, boot voltage + state).
    static const TDD_POWER_AXP_DOMAIN_T axp_domains[] = {
        {TDL_PWR_DOMAIN_CAMERA,      VDD_CAM_2V8,    2800, TRUE },
        {TDL_PWR_DOMAIN_SD,          VDD_SD_3V3,     3300, TRUE },
        {TDL_PWR_DOMAIN_CAMERA_AVDD, AVDD_CAM_2V8,   2800, TRUE },
        {TDL_PWR_DOMAIN_CAMERA_DVDD, DVDD_CAM_1V8,   1800, TRUE },
        {TDL_PWR_DOMAIN_JOYSTICK,    VDD_JOYCON_1V1, 1100, FALSE},
        {TDL_PWR_DOMAIN_RTC,         RTC_VDD,        1800, TRUE },
    };
    // No-role rails: DCDC1/DCDC5 are the board 3V3 core (kept on); the rest are the
    // unused channels the original bring-up defensively disabled.
    static const TDD_POWER_AXP_RAIL_T axp_rails[] = {
        {XPOWERS_DCDC1, 3300, TRUE }, {XPOWERS_DCDC5, 3300, TRUE },
        {XPOWERS_DCDC2, 0, FALSE}, {XPOWERS_DCDC3, 0, FALSE}, {XPOWERS_DCDC4, 0, FALSE},
        {XPOWERS_ALDO1, 0, FALSE}, {XPOWERS_ALDO2, 0, FALSE},
        {XPOWERS_DLDO1, 0, FALSE}, {XPOWERS_CPULDO, 0, FALSE}, {XPOWERS_VBACKUP, 0, FALSE},
    };
    rt = tdd_power_axp2101_register(POWER_NAME, &(TDD_POWER_AXP2101_CFG_T){
        .domains = axp_domains, .domain_cnt = sizeof(axp_domains) / sizeof(axp_domains[0]),
        .rails   = axp_rails,   .rail_cnt   = sizeof(axp_rails) / sizeof(axp_rails[0]),
        .charge = {
            .vbus_vol_limit    = XPOWERS_AXP2101_VBUS_VOL_LIM_4V20,
            .vbus_cur_limit    = XPOWERS_AXP2101_VBUS_CUR_LIM_500MA,
            .sys_power_down_mv = 3300,
            .precharge_curr    = XPOWERS_AXP2101_PRECHARGE_200MA,
            .term_curr         = XPOWERS_AXP2101_CHG_ITERM_25MA,
            .const_curr        = XPOWERS_AXP2101_CHG_CUR_1000MA,
            .target_vol        = XPOWERS_AXP2101_CHG_VOL_4V2,
            .led_mode          = XPOWERS_CHG_LED_CTRL_CHG,
        },
        .pekey_on  = XPOWERS_POWERON_128MS,
        .pekey_off = XPOWERS_POWEROFF_4S,
        .irq_pin   = TDD_POWER_PIN_NONE, // AXP IRQ line not wired on POCKET
        .info = {.battery = {.v_full_mv = 4200, .v_empty_mv = 3000, .v_low_mv = 3400, .v_critical_mv = 3300}},
    });
    if (OPRT_OK != rt) {
        PR_ERR("AXP2101 power register failed: %d", rt);
        return rt;
    }

    // --- SoC contributor: 4G module supply is a plain SoC GPIO (SIM_VDD, active-low),
    // joined as a CELLULAR domain so the app can switch 4G power via tdl_power_domain_set.
    static const TDD_POWER_GPIO_DOMAIN_T soc_domains[] = {
        {TDL_PWR_DOMAIN_CELLULAR, SIM_VDD_4G_MODULE_CTRL, TUYA_GPIO_LEVEL_LOW, TRUE},
    };
    tdd_power_soc_register(POWER_NAME, &(TDD_POWER_SOC_CFG_T){
        .domains = soc_domains, .domain_cnt = sizeof(soc_domains) / sizeof(soc_domains[0]),
    });

    // 4G module reset line (not power): drive high = working.
    TUYA_CALL_ERR_LOG(tkl_gpio_init(RST_4G_MODULE_CTRL, &gc));
    tkl_gpio_write(RST_4G_MODULE_CTRL, TUYA_GPIO_LEVEL_HIGH);

    return rt;
}

static OPERATE_RET __board_register_camera(void)
{
#if defined(CAMERA_NAME)
    OPERATE_RET          rt         = OPRT_OK;
    TDD_DVP_SR_USR_CFG_T camera_cfg = {
        .pwr =
            {
                .pin = BOARD_CAMERA_POWER_PIN,
            },
        .rst =
            {
                .pin = BOARD_CAMERA_RST_PIN,
            },
        .i2c =
            {
                .port = BOARD_CAMERA_I2C_PORT,
                .clk  = BOARD_CAMERA_I2C_SCL,
                .sda  = BOARD_CAMERA_I2C_SDA,
            },
        .clk = BOARD_CAMERA_CLK,
    };

    TUYA_CALL_ERR_RETURN(tdd_camera_dvp_gc2145_register(CAMERA_NAME, &camera_cfg));
#endif

    return OPRT_OK;
}

static OPERATE_RET __board_sdio_pin_register(void)
{
    tkl_io_pinmux_config(TUYA_GPIO_NUM_14, TUYA_SDIO_CLK);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_15, TUYA_SDIO_CMD);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_16, TUYA_SDIO_DATA0);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_17, TUYA_SDIO_DATA1);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_18, TUYA_SDIO_DATA2);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_19, TUYA_SDIO_DATA3);

    return OPRT_OK;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, display) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_axp2101());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_joystick());
    TUYA_CALL_ERR_LOG(__board_register_audio_mux_router());
    TUYA_CALL_ERR_LOG(__board_register_bmi270_sensor());
    TUYA_CALL_ERR_LOG(__board_register_camera());
    TUYA_CALL_ERR_LOG(__board_sdio_pin_register());

    return rt;
}