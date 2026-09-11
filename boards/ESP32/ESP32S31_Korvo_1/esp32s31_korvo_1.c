/**
 * @file esp32s31_korvo_1.c
 * @brief Board hardware registration for the ESP32-S31-Korvo-1 board.
 *
 * Pin map follows the official Espressif BSP (espressif/esp32_s31_korvo_1):
 *   - ES8389 stereo codec, MCLK-less (the codec is clocked from BCLK):
 *       I2C0 SCL=1 SDA=0, I2S0 BCLK=3 LRCK=4 DOUT=5 DSIN=6, PA_EN=7
 *   - SET/MODE/VOL-/VOL+ keys on an ADC resistor ladder (GPIO42 = ADC1_CH0)
 *   - WS2812 RGB status LED on GPIO37 (5 V level-shifted)
 *   - 4.3" 800x480 RGB LCD subboard (ESP32-S3-LCD-EV-Board-SUB3):
 *       PCLK=40 DE=43 HSYNC=44 VSYNC=45 DISP=38,
 *       DATA0-15 = 8..19 / 33..36, PCLK 18 MHz sampled on the falling edge
 *   - OV3660 DVP camera on GPIO46-57, using Espressif esp_video.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_adc.h"

#include "board_com_api.h"

#if defined(AUDIO_CODEC_NAME)
#include "tdd_audio_codec_bus.h"
#include "tdd_audio_es8389_codec.h"
#endif

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_esp_rgb.h"
#endif

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#include "tdd_tp_esp_gt1151.h"
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdd_led_esp_ws1280.h"
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_driver.h"
#endif

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
#include "tdd_camera_esp_video_dvp.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/* ES8389 codec: control over I2C0, data over I2S0 */
#define AUDIO_I2C_NUM    (0)
#define AUDIO_I2C_SCL_IO (1)
#define AUDIO_I2C_SDA_IO (0)

#define AUDIO_I2S_NUM    (0)
#define AUDIO_I2S_MCK_IO (-1) /* MCLK not wired on Korvo-1 */
#define AUDIO_I2S_BCK_IO (3)
#define AUDIO_I2S_WS_IO  (4)
#define AUDIO_I2S_DO_IO  (5)
#define AUDIO_I2S_DI_IO  (6)

#define ES8389_I2C_ADDR (0x10 << 1) /* 7-bit 0x10 == ES8389_CODEC_DEFAULT_ADDR */
#define ES8389_PA_IO    (7)         /* NS4150B PA enable, active high */

#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define MIC_SAMPLE_RATE  (16000) /* MCLK-less codec: stay at 16 kHz */
#define SPK_SAMPLE_RATE  (16000)
#define DEFAULT_VOLUME   (80)

/* 4.3" 800x480 RGB subboard (ESP32-S3-LCD-EV-Board-SUB3), DE-mode panel */
#define LCD_RGB_PCLK_HZ  (18000000)
#define LCD_RGB_PCLK_IO  (40)
#define LCD_RGB_DE_IO    (43)
#define LCD_RGB_HSYNC_IO (44)
#define LCD_RGB_VSYNC_IO (45)
#define LCD_RGB_DISP_IO  (38)

#define DISPLAY_WIDTH  (800)
#define DISPLAY_HEIGHT (480)

/* WS2812 status LED */
#define LED_WS1280_IO (37)

/* GT1151 cap touch: shares the codec I2C0 (SCL=1/SDA=0), addr 0x14, no INT/RST. */
#define TOUCH_I2C_NUM    (AUDIO_I2C_NUM)
#define TOUCH_I2C_SCL_IO (AUDIO_I2C_SCL_IO)
#define TOUCH_I2C_SDA_IO (AUDIO_I2C_SDA_IO)
#define TOUCH_INT_IO     (-1)
#define TOUCH_RST_IO     (-1)

/* Function keys: resistor ladder on GPIO42 = ADC1_CH0 */
#define BUTTON_ADC_PORT (TUYA_ADC_NUM_0)
#define BUTTON_ADC_CH   (0)

/* OV3660 DVP camera, matching the official Espressif S31 Korvo BSP */
#define CAMERA_I2C_NUM    (AUDIO_I2C_NUM)
#define CAMERA_XCLK_IO    (55)
#define CAMERA_PCLK_IO    (54)
#define CAMERA_VSYNC_IO   (56)
#define CAMERA_HSYNC_IO   (57)
#define CAMERA_XCLK_HZ    (20000000)

/***********************************************************
***********************typedef define***********************
***********************************************************/

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
typedef struct {
    const char *name;
    int32_t raw_min; /* raw 12-bit window (official BSP values) */
    int32_t raw_max;
} KORVO_ADC_BUTTON_T;
#endif

/***********************************************************
********************variable define*************************
***********************************************************/

#if defined(AUDIO_CODEC_NAME)
static TDD_AUDIO_I2C_HANDLE sg_i2c_bus_handle = NULL;
static TDD_AUDIO_I2S_TX_HANDLE sg_i2s_tx_handle = NULL;
static TDD_AUDIO_I2S_RX_HANDLE sg_i2s_rx_handle = NULL;
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static const KORVO_ADC_BUTTON_T cKORVO_ADC_BUTTON_TBL[] = {
    /* windows calibrated on the real ladder: 319 / 848 / 1371 / 1813 measured */
    { BUTTON_NAME,   150,  560 }, /* SET  */
    { BUTTON_NAME_2, 560,  1100 }, /* MODE */
    { BUTTON_NAME_3, 1100, 1590 }, /* VOL- */
    { BUTTON_NAME_4, 1590, 2100 }, /* VOL+ */
};
#define KORVO_ADC_BUTTON_NUM (sizeof(cKORVO_ADC_BUTTON_TBL) / sizeof(cKORVO_ADC_BUTTON_TBL[0]))
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __board_register_audio(void)
{
#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_CODEC_BUS_CFG_T bus_cfg = {
        .i2c_id        = AUDIO_I2C_NUM,
        .i2c_sda_io    = AUDIO_I2C_SDA_IO,
        .i2c_scl_io    = AUDIO_I2C_SCL_IO,
        .i2s_id        = AUDIO_I2S_NUM,
        .i2s_mck_io    = AUDIO_I2S_MCK_IO,
        .i2s_bck_io    = AUDIO_I2S_BCK_IO,
        .i2s_ws_io     = AUDIO_I2S_WS_IO,
        .i2s_do_io     = AUDIO_I2S_DO_IO,
        .i2s_di_io     = AUDIO_I2S_DI_IO,
        .dma_desc_num  = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .sample_rate   = SPK_SAMPLE_RATE,
    };

    tdd_audio_codec_bus_i2c_new(bus_cfg, &sg_i2c_bus_handle);
    tdd_audio_codec_bus_i2s_new(bus_cfg, &sg_i2s_tx_handle, &sg_i2s_rx_handle);

    TDD_AUDIO_ES8389_CODEC_T codec = {
        .i2c_id          = AUDIO_I2C_NUM,
        .i2c_handle      = sg_i2c_bus_handle,
        .i2s_id          = AUDIO_I2S_NUM,
        .i2s_tx_handle   = sg_i2s_tx_handle,
        .i2s_rx_handle   = sg_i2s_rx_handle,
        .mic_sample_rate = MIC_SAMPLE_RATE,
        .spk_sample_rate = SPK_SAMPLE_RATE,
        .es8389_addr     = ES8389_I2C_ADDR,
        .pa_pin          = ES8389_PA_IO,
        .default_volume  = DEFAULT_VOLUME,
        .spk_channel_mask = 2, /* speaker_r is the ES8389 right DAC slot */
    };
    return tdd_audio_es8389_codec_register(AUDIO_CODEC_NAME, codec);
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_display(void)
{
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
    /* No backlight and no power-enable GPIO on this subboard (both always on).
     * power.pin MUST be TUYA_GPIO_NUM_MAX to mark it unused - the tdl layer
     * gates power init on `pin >= TUYA_GPIO_NUM_MAX`. Leaving it 0 makes the
     * layer gpio_init(GPIO0), which collides with the I2C0 SDA (also GPIO0)
     * and breaks GT1151 touch. */
    TDD_DISP_ESP_LCD_CFG_T cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = false,
        .bl        = { .type = TUYA_DISP_BL_TP_NONE },
        .power     = { .pin = TUYA_GPIO_NUM_MAX },
    };

    TDD_DISP_ESP_RGB_HW_CFG_T hw = {
        .pclk_hz           = LCD_RGB_PCLK_HZ,
        .h_res             = DISPLAY_WIDTH,
        .v_res             = DISPLAY_HEIGHT,
        .hsync_pulse_width = 40,
        .hsync_back_porch  = 40,
        .hsync_front_porch = 48,
        .vsync_pulse_width = 23,
        .vsync_back_porch  = 32,
        .vsync_front_porch = 13,
        .pclk_gpio         = LCD_RGB_PCLK_IO,
        .de_gpio           = LCD_RGB_DE_IO,
        .hsync_gpio        = LCD_RGB_HSYNC_IO,
        .vsync_gpio        = LCD_RGB_VSYNC_IO,
        .disp_gpio         = LCD_RGB_DISP_IO,
        .pclk_active_neg   = true,
        .data_gpio         = {
            8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 33, 34, 35, 36,
        },
        .bounce_buffer_size = 0, /* 0 = direct scan from PSRAM: zero-copy double-fb mode */
    };

    return tdd_disp_esp_rgb_register(DISPLAY_NAME, &hw, &cfg);
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_touch(void)
{
#if defined(ENABLE_TP) && (ENABLE_TP == 1)
    TDD_TP_ESP_GT1151_CFG_T tp_cfg = {
        .i2c_port   = TOUCH_I2C_NUM,
        .i2c_scl_io = TOUCH_I2C_SCL_IO,
        .i2c_sda_io = TOUCH_I2C_SDA_IO,
        .rst_io     = TOUCH_RST_IO,
        .int_io     = TOUCH_INT_IO,
        .tp = {
            .tp_cfg = {
                .x_max = DISPLAY_WIDTH,
                .y_max = DISPLAY_HEIGHT,
                .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
            },
        },
    };
    return tdd_tp_esp_i2c_gt1151_register(DISPLAY_NAME, &tp_cfg);
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_led(void)
{
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    TDD_LED_WS1280_CFG_T led_cfg = {
        .gpio      = LED_WS1280_IO,
        .led_count = 1,
        .color     = 0x001800, /* GRB, dim red: the ON color for tdl_led set_status */
    };
    return tdd_led_esp_ws1280_register(LED_NAME, &led_cfg);
#else
    return OPRT_OK;
#endif
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static OPERATE_RET __btn_adc_create(TDL_BUTTON_OPRT_INFO *dev)
{
    return (dev && dev->dev_handle) ? OPRT_OK : OPRT_INVALID_PARM;
}

static OPERATE_RET __btn_adc_delete(TDL_BUTTON_OPRT_INFO *dev)
{
    return (dev && dev->dev_handle) ? OPRT_OK : OPRT_INVALID_PARM;
}

/* A key counts as pressed when the ladder voltage lands in its window */
static OPERATE_RET __btn_adc_read(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    const KORVO_ADC_BUTTON_T *btn = NULL;
    int32_t raw = 0;
    OPERATE_RET rt = OPRT_OK;

    if (dev == NULL || dev->dev_handle == NULL || value == NULL) {
        return OPRT_INVALID_PARM;
    }
    btn = (const KORVO_ADC_BUTTON_T *)dev->dev_handle;

    rt = tkl_adc_read_single_channel(BUTTON_ADC_PORT, BUTTON_ADC_CH, &raw);
    if (rt != OPRT_OK) {
        return rt;
    }
    *value = (raw >= btn->raw_min && raw <= btn->raw_max) ? 1 : 0;

    return OPRT_OK;
}
#endif

static OPERATE_RET __board_register_button(void)
{
#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    OPERATE_RET rt = OPRT_OK;

    TUYA_ADC_BASE_CFG_T adc_cfg = {
        .ch_list.data = (1u << BUTTON_ADC_CH),
        .ch_nums      = 1,
        .width        = 12,
        .type         = TUYA_ADC_INNER_SAMPLE_VOL,
        .mode         = TUYA_ADC_SINGLE,
        .conv_cnt     = 1,
    };

    TUYA_CALL_ERR_RETURN(tkl_adc_init(BUTTON_ADC_PORT, &adc_cfg));

    for (uint32_t i = 0; i < KORVO_ADC_BUTTON_NUM; i++) {
        TDL_BUTTON_CTRL_INFO ctrl_info = {
            .button_create = __btn_adc_create,
            .button_delete = __btn_adc_delete,
            .read_value    = __btn_adc_read,
        };
        TDL_BUTTON_DEVICE_INFO_T device_info = {
            .dev_handle = (void *)&cKORVO_ADC_BUTTON_TBL[i],
            .mode       = BUTTON_TIMER_SCAN_MODE,
        };
        TUYA_CALL_ERR_LOG(tdl_button_register((char *)cKORVO_ADC_BUTTON_TBL[i].name,
                                              &ctrl_info, &device_info));
    }

    return rt;
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_camera(void)
{
#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
    TDD_CAMERA_ESP_VIDEO_DVP_CFG_T camera_cfg = {
        .i2c_port = CAMERA_I2C_NUM,
        .sccb_freq_hz = 100000,
        .reset_pin = -1,
        .pwdn_pin = -1,
        .xclk_pin = CAMERA_XCLK_IO,
        .xclk_freq_hz = CAMERA_XCLK_HZ,
        .data_io = {46, 47, 48, 49, 50, 51, 52, 53},
        .vsync_io = CAMERA_VSYNC_IO,
        .de_io = CAMERA_HSYNC_IO,
        .pclk_io = CAMERA_PCLK_IO,
    };
    return tdd_camera_esp_video_dvp_register(CAMERA_NAME, &camera_cfg);
#else
    return OPRT_OK;
#endif
}

/**
 * @brief Registers all board peripherals (audio, display, LED, buttons).
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_camera());
    TUYA_CALL_ERR_LOG(__board_register_display());
    PR_NOTICE("[INIT] DISPLAY registered");
    TUYA_CALL_ERR_LOG(__board_register_touch());
    PR_NOTICE("[INIT] TOUCH registered");
    TUYA_CALL_ERR_LOG(__board_register_led());

    return rt;
}
