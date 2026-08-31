#include "tuya_cloud_types.h"
#include "tdd_audio_no_codec.h"
#include "tdd_led_gpio.h"
#include "tdd_button_gpio.h"

#if defined(DISPLAY_NAME)
#if ENABLE_TDD_DISP_ST7789
#include "tdl_display_driver.h"
#include "tdd_disp_st7789.h"
#else
#include "tdd_display_custom.h"
#endif
#endif /* defined(DISPLAY_NAME) */

#include "board_com_api.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define BOARD_LCD_WIDTH              320
#define BOARD_LCD_HEIGHT             240
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_3
#define BOARD_LCD_SPI_CLK            40000000
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_29
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_26
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_28

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_30
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

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
    TDD_AUDIO_NO_CODEC_T cfg = {0};
    cfg.i2s_id = 0;
    cfg.mic_sample_rate = 16000;
    cfg.spk_sample_rate = 16000;

    TUYA_CALL_ERR_RETURN(tdd_audio_no_codec_register(AUDIO_CODEC_NAME, cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_button(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BOARD_SW1_PIN)
    BUTTON_GPIO_CFG_T sw1_hw_cfg = {
        .pin = BOARD_SW1_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_SW1_NAME, &sw1_hw_cfg));
#endif

#if defined(BOARD_SW2_PIN)
    BUTTON_GPIO_CFG_T sw2_hw_cfg = {
        .pin = BOARD_SW2_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(BOARD_SW2_NAME, &sw2_hw_cfg));
#endif

    return rt;
}

static OPERATE_RET __board_register_led(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BOARD_LEDB_PIN)
    TDD_LED_GPIO_CFG_T ledb_gpio = {
        .pin = BOARD_LEDB_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = TUYA_GPIO_PUSH_PULL,
    };

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(BOARD_LEDB_NAME, &ledb_gpio));
#endif

#if defined(BOARD_LEDR_PIN)
    TDD_LED_GPIO_CFG_T ledr_gpio = {
        .pin = BOARD_LEDR_PIN,
        .level = TUYA_GPIO_LEVEL_LOW,
        .mode = TUYA_GPIO_PUSH_PULL,
    };

    TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(BOARD_LEDR_NAME, &ledr_gpio));
#endif

    return rt;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(DISPLAY_NAME)
#if ENABLE_TDD_DISP_ST7789
    DISP_SPI_DEVICE_CFG_T display_cfg;

    memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));

    display_cfg.width     = BOARD_LCD_WIDTH;
    display_cfg.height    = BOARD_LCD_HEIGHT;
    display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
    display_cfg.rotation  = BOARD_LCD_ROTATION;

    display_cfg.port      = BOARD_LCD_SPI_PORT;
    display_cfg.spi_clk   = BOARD_LCD_SPI_CLK;
    display_cfg.cs_pin    = BOARD_LEDG_PIN; //BOARD_LCD_SPI_CS_PIN; SPI CS auto already
    display_cfg.dc_pin    = BOARD_LCD_SPI_DC_PIN;
    display_cfg.rst_pin   = BOARD_LCD_SPI_RST_PIN;

    display_cfg.bl.type              = BOARD_LCD_BL_TYPE;
    display_cfg.bl.gpio.pin          = BOARD_LCD_BL_PIN;
    display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;

    display_cfg.power.pin = BOARD_LCD_POWER_PIN;

    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg)); 
#else
    TUYA_CALL_ERR_RETURN(tdd_disp_custom_register(DISPLAY_NAME));
#endif /* ENABLE_TDD_DISP_ST7789 */
#endif /* defined(DISPLAY_NAME) */

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

    TUYA_CALL_ERR_LOG(__board_register_audio());

    TUYA_CALL_ERR_LOG(__board_register_button());

    TUYA_CALL_ERR_LOG(__board_register_led());

    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}