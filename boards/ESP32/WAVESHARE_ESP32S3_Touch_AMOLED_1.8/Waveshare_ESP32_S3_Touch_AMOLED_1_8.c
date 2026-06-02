/**
 * @file waveshare_esp32_s3_touch_amoled_1_8.c
 * @brief Implementation of common board-level hardware registration APIs for peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"

#include "tdd_button_gpio.h"
#include "tdd_button_io_expander.h"
#include "tdl_button_manage.h"

#include "tdd_audio_8311_codec.h"

#include "tca9554.h"
#include "axp2101_driver.h"
#include "tkl_pinmux.h"
#include "tdd_disp_esp_sh8601.h"
#include "tdd_tp_esp_ft5x06.h"
#include "tdl_display_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* Audio sample rates */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (14)
#define I2C_SDA_IO (15)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (16)
#define I2S_BCK_IO (9)
#define I2S_WS_IO  (45)
#define I2S_DO_IO  (8)
#define I2S_DI_IO  (10)

#define GPIO_OUTPUT_PA (46)

/* Audio codec */
#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR   (0x30)

/* TCA9554 IO expander */
#define IO_EXPANDER_TCA9554_ADDR (0x20)

/* QSPI bus shared by SH8601 LCD */
#define SPI_NUM      (1)
#define SPI_SCLK_IO  (11)
#define SPI_MOSI_IO  (4)
#define SPI_MISO_IO  (5)
#define SPI_DATA2_IO (6)
#define SPI_DATA3_IO (7)

/* SH8601 AMOLED */
#define SPI_CS_LCD       (12)
#define DISPLAY_WIDTH    (368)
#define DISPLAY_HEIGHT   (448)
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_BYTES 1

/* FT5X06 touch */
#define TOUCH_INT_IO (21)

/* On-board mechanical button */
#define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_0
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

/* Power key: TCA9554 EXIO5 = AXP2101 IRQ (active low) */
#define BOARD_PWR_KEY_NAME      "power_key"
#define BOARD_PWR_KEY_PIN_MASK  (1U << 5)

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
static int __board_display_io_init(void)
{
    TCA9554_HW_CFG_T tca9554_hw = {
        .i2c_port = I2C_NUM,
        .scl_io   = I2C_SCL_IO,
        .sda_io   = I2C_SDA_IO,
        .dev_addr = IO_EXPANDER_TCA9554_ADDR,
    };

    int rt = tca9554_init(&tca9554_hw);
    if (rt != 0) {
        PR_ERR("tca9554_init failed");
        return rt;
    }

    uint32_t in_pin_mask = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);
    rt = tca9554_set_dir(in_pin_mask, 0);
    if (rt != 0) {
        PR_ERR("tca9554_set_dir failed");
        return rt;
    }

    uint32_t out_pin_mask = (1ULL << 4);
    rt = tca9554_set_dir(out_pin_mask, 0);
    if (rt != 0) {
        PR_ERR("tca9554_set_dir failed");
        return rt;
    }
    tca9554_set_level(out_pin_mask, 0);

    tca9554_set_level(in_pin_mask, 1);
    tal_system_sleep(100);
    tca9554_set_level(in_pin_mask, 0);
    tal_system_sleep(300);
    tca9554_set_level(in_pin_mask, 1);

    return 0;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin                = BOARD_BUTTON_PIN,
        .level              = BOARD_BUTTON_ACTIVE_LV,
        .mode               = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BUTTON_NAME, &button_hw_cfg));

    return OPRT_OK;
}

static void __power_key_irq_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    axp2101_getIrqStatus();

    if (axp2101_isPekeyShortPressIrq()) {
        PR_NOTICE("power key: short press");
    }
    if (axp2101_isPekeyLongPressIrq()) {
        PR_NOTICE("power key: long press — AXP2101 shutting down");
    }

    axp2101_clearIrqStatus();
}

static OPERATE_RET __board_register_power_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* EXIO5 = AXP2101 IRQ: normally HIGH, goes LOW when a key event fires.
     * Register it as a timer-scan input so the TDL layer detects the falling
     * edge and calls the callback, at which point we read + clear AXP IRQ.
     *
     * tca9554 is already initialised in __board_display_io_init() at boot,
     * so we leave .init NULL — passing tca9554_init here would be a signature
     * mismatch (it now takes a TCA9554_HW_CFG_T *).
     */
    TDD_BUTTON_IO_EXP_CFG_T hw_cfg = {
        .pin_mask     = BOARD_PWR_KEY_PIN_MASK,
        .active_level = TUYA_GPIO_LEVEL_LOW,
        .init         = NULL,
        .set_dir      = tca9554_set_dir,
        .get_level    = tca9554_get_level,
    };
    TUYA_CALL_ERR_RETURN(tdd_io_expander_button_register(BOARD_PWR_KEY_NAME, &hw_cfg));

    TDL_BUTTON_HANDLE handle = NULL;
    TDL_BUTTON_CFG_T btn_cfg = {
        .long_start_valid_time     = 0,   /* long-press handled by AXP2101 hardware */
        .long_keep_timer           = 0,
        .button_debounce_time      = 20,
        .button_repeat_valid_count = 0,
        .button_repeat_valid_time  = 0,
    };
    TUYA_CALL_ERR_RETURN(tdl_button_create(BOARD_PWR_KEY_NAME, &btn_cfg, &handle));

    /* React as soon as EXIO5 goes low (IRQ fires) */
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOWN, __power_key_irq_cb);

    return OPRT_OK;
}

static OPERATE_RET __board_register_audio(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(AUDIO_CODEC_NAME)
    TDD_AUDIO_8311_CODEC_T cfg = {0};
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
    cfg.es8311_addr = AUDIO_CODEC_ES8311_ADDR;
    cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    cfg.default_volume = 80;

    TUYA_CALL_ERR_RETURN(tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}


static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    LCD_SH8601_HW_CFG_T sh8601_hw = {
        .spi_host = SPI_NUM,
        .sclk_io  = SPI_SCLK_IO,
        .data0_io = SPI_MOSI_IO,
        .data1_io = SPI_MISO_IO,
        .data2_io = SPI_DATA2_IO,
        .data3_io = SPI_DATA3_IO,
        .cs_io    = SPI_CS_LCD,
        .mirror_x = DISPLAY_MIRROR_X,
        .mirror_y = DISPLAY_MIRROR_Y,
    };
    TDD_DISP_ESP_LCD_CFG_T lcd_cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = DISPLAY_SWAP_BYTES,
    };
    TUYA_CALL_ERR_RETURN(tdd_disp_esp_sh8601_register(DISPLAY_NAME, &sh8601_hw, &lcd_cfg));


    TDD_TP_ESP_FT5X06_CFG_T tp_cfg = {
        .i2c_port   = I2C_NUM,
        .i2c_scl_io = I2C_SCL_IO,
        .i2c_sda_io = I2C_SDA_IO,
        .rst_io     = -1,
        .int_io     = TOUCH_INT_IO,
        .tp = {
            .tp_cfg = {
                .x_max = DISPLAY_WIDTH,
                .y_max = DISPLAY_HEIGHT,
                .flags = {
                    .swap_xy  = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
            },
        },
    };
    TUYA_CALL_ERR_RETURN(tdd_tp_esp_i2c_ft5x06_register(DISPLAY_NAME, &tp_cfg));

    return rt;
}

/**
 * @brief Registers all the hardware peripherals (audio, button, LED) on the board.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    TUYA_CALL_ERR_LOG(__board_display_io_init());

    /* Configure I2C0 pin mapping before any TKL I2C driver initialises the bus */
    tkl_io_pinmux_config(I2C_SCL_IO, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(I2C_SDA_IO, TUYA_IIC0_SDA);

    TUYA_CALL_ERR_LOG(axp2101_init());
    axp2101_setPowerKeyPressOffTime(XPOWERS_POWEROFF_6S);
    axp2101_enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
    TUYA_CALL_ERR_LOG(__board_register_power_button());

    TUYA_CALL_ERR_LOG(__board_register_audio());
    TUYA_CALL_ERR_LOG(__board_register_button());
    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
