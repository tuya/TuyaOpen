/**
 * @file seeed_esp32s3_xiao_sense.c
 * @brief Board hardware registration for the Seeed XIAO ESP32S3 Sense.
 *
 * ESP32-S3R8 (8MB flash / 8MB PSRAM) carrier with the Sense expansion board:
 *   - DVP camera (OV2640 / OV3660, auto-detected by esp32-camera)
 *   - PDM digital microphone (onboard Sense mic)
 *   - optional MAX98357A I2S amplifier (external, GPIO2/4/7)
 *   - onboard user LED (GPIO21)
 *   - optional microSD/TF card over SPI
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"

#include "board_com_api.h"

#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
#include "tdd_camera_esp_dvp.h"
#endif

#if defined(AUDIO_CODEC_NAME)
#if defined(ENABLE_SEEED_XIAO_MAX98357A) && (ENABLE_SEEED_XIAO_MAX98357A == 1)
#include "tdd_audio_pdm_i2s_spk.h"
#else
#include "tdd_audio_pdm_mic.h"
#endif
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdd_led_gpio.h"
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdd_button_gpio.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* DVP camera (CAMERA_MODEL_XIAO_ESP32S3): the module has no onboard oscillator,
 * so XCLK is driven from the S3. PWDN / RESET are not wired out. */
#define CAM_XCLK_IO   (10)
#define CAM_XCLK_FREQ (20000000)
#define CAM_SCL_IO    (39)
#define CAM_SDA_IO    (40)
#define CAM_PCLK_IO   (13)
#define CAM_VSYNC_IO  (38)
#define CAM_HREF_IO   (47)
/* DVP data bus D0..D7 == sensor Y2..Y9 */
#define CAM_D0_IO (15)
#define CAM_D1_IO (17)
#define CAM_D2_IO (18)
#define CAM_D3_IO (16)
#define CAM_D4_IO (14)
#define CAM_D5_IO (12)
#define CAM_D6_IO (11)
#define CAM_D7_IO (48)

/* PDM microphone (onboard Sense expansion) */
#define MIC_I2S_NUM     (0)
#define MIC_CLK_IO      (42)
#define MIC_DATA_IO     (41)
#define MIC_SAMPLE_RATE (16000)

/* MAX98357A I2S amplifier (external module on XIAO header pins) */
#define SPK_I2S_NUM       (1)
#define SPK_BCLK_IO       (7)  /* D8 */
#define SPK_WS_IO         (4)  /* D3 / LRC */
#define SPK_DOUT_IO       (2)  /* D1 / DIN */
#define SPK_SAMPLE_RATE   (16000)
#define SPK_SD_PIN        (TUYA_GPIO_NUM_MAX) /* SD tied to 3V3 on module */
#define SPK_SD_POLARITY   (1)                 /* HIGH enables amp when GPIO wired */

/* Onboard user LED — active-low, shared with SD chip-select (see below). */
#define LED_IO (21)

/* Onboard BOOT button (GPIO0), active-low. */
#define BUTTON_IO        (0)
#define BUTTON_ACTIVE_LV (TUYA_GPIO_LEVEL_LOW)

/* microSD/TF over SPI on the Sense expansion board. CS shares GPIO21 with the
 * user LED, so the two cannot be used simultaneously. */
#define SD_SPI_SCLK_IO (7)
#define SD_SPI_MISO_IO (8)
#define SD_SPI_MOSI_IO (9)
#define SD_SPI_CS_IO   (21)

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __board_register_camera(void)
{
#if defined(ENABLE_CAMERA) && (ENABLE_CAMERA == 1)
    TDD_CAMERA_ESP_DVP_CFG_T cam_cfg = {
        .pin_pwdn      = -1,
        .pin_reset     = -1,
        .pin_xclk      = CAM_XCLK_IO,
        .xclk_freq_hz  = CAM_XCLK_FREQ,
        .pin_sccb_scl  = CAM_SCL_IO,
        .pin_sccb_sda  = CAM_SDA_IO,
        .sccb_i2c_port = -1, /* let the driver allocate its own I2C bus */
        .pin_d0        = CAM_D0_IO,
        .pin_d1        = CAM_D1_IO,
        .pin_d2        = CAM_D2_IO,
        .pin_d3        = CAM_D3_IO,
        .pin_d4        = CAM_D4_IO,
        .pin_d5        = CAM_D5_IO,
        .pin_d6        = CAM_D6_IO,
        .pin_d7        = CAM_D7_IO,
        .pin_vsync     = CAM_VSYNC_IO,
        .pin_href      = CAM_HREF_IO,
        .pin_pclk      = CAM_PCLK_IO,
    };
    return tdd_camera_esp_dvp_register(CAMERA_NAME, &cam_cfg);
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_audio(void)
{
#if defined(AUDIO_CODEC_NAME)
#if defined(ENABLE_SEEED_XIAO_MAX98357A) && (ENABLE_SEEED_XIAO_MAX98357A == 1)
    TDD_AUDIO_PDM_I2S_SPK_T audio_cfg = {
        .mic_i2s_id       = MIC_I2S_NUM,
        .mic_clk_io       = MIC_CLK_IO,
        .mic_din_io       = MIC_DATA_IO,
        .mic_sample_rate  = MIC_SAMPLE_RATE,
        .spk_i2s_id       = SPK_I2S_NUM,
        .spk_bclk_io      = SPK_BCLK_IO,
        .spk_ws_io        = SPK_WS_IO,
        .spk_dout_io      = SPK_DOUT_IO,
        .spk_sample_rate  = SPK_SAMPLE_RATE,
        .spk_sd_pin       = SPK_SD_PIN,
        .spk_sd_polarity  = SPK_SD_POLARITY,
    };
    return tdd_audio_pdm_i2s_spk_register(AUDIO_CODEC_NAME, audio_cfg);
#else
    TDD_AUDIO_PDM_MIC_T mic_cfg = {
        .i2s_id          = MIC_I2S_NUM,
        .clk_io          = MIC_CLK_IO,
        .din_io          = MIC_DATA_IO,
        .mic_sample_rate = MIC_SAMPLE_RATE,
    };
    return tdd_audio_pdm_mic_register(AUDIO_CODEC_NAME, mic_cfg);
#endif
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_sd(void)
{
#if defined(ENABLE_SEEED_XIAO_SDCARD) && (ENABLE_SEEED_XIAO_SDCARD == 1)
    tkl_io_pinmux_config(SD_SPI_MOSI_IO, TUYA_SPI1_MOSI);
    tkl_io_pinmux_config(SD_SPI_SCLK_IO, TUYA_SPI1_CLK);
    tkl_io_pinmux_config(SD_SPI_MISO_IO, TUYA_SPI1_MISO);
    tkl_io_pinmux_config(SD_SPI_CS_IO, TUYA_SPI1_CS);
#endif
    return OPRT_OK;
}

static OPERATE_RET __board_register_led(void)
{
    /* ENABLE_LED is selected by the board Kconfig only when the SD card is
     * disabled (LED and SD CS share GPIO21), so no runtime check is needed. */
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    TDD_LED_GPIO_CFG_T led_cfg = {
        .pin   = LED_IO,
        .mode  = TUYA_GPIO_PULLUP,
        .level = TUYA_GPIO_LEVEL_LOW, /* active-low */
    };
    return tdd_led_gpio_register(LED_NAME, &led_cfg);
#else
    return OPRT_OK;
#endif
}

static OPERATE_RET __board_register_button(void)
{
#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    BUTTON_GPIO_CFG_T button_cfg = {
        .pin                = BUTTON_IO,
        .level              = BUTTON_ACTIVE_LV,
        .mode               = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };
    return tdd_gpio_button_register(BUTTON_NAME, &button_cfg);
#else
    return OPRT_OK;
#endif
}

/**
 * @brief Registers all board peripherals (camera, PDM mic, LED/button, optional SD).
 *
 * @return OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(__board_register_camera());
    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_sd());
    TUYA_CALL_ERR_LOG(__board_register_led());
    TUYA_CALL_ERR_LOG(__board_register_button());

    return rt;
}
