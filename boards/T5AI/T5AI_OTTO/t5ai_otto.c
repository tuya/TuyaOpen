/**
 * @file tuya_t5ai_mini.c
 * @author Tuya Inc.
 * @brief Implementation of common board-level hardware registration APIs for audio, button, and LED peripherals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

 #include "tuya_cloud_types.h"

 #include "tal_api.h"
 
 #include "tdd_audio.h"
 #if defined(LED_NAME)
 #include "tdd_led_gpio.h"
 #endif
 #if defined(BUTTON_NAME)
 #include "tdd_button_gpio.h"
 #endif
 
#if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1)
#include "tdd_disp_st7789.h"
#elif defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1)
#include "tdd_disp_st7735s.h"
#elif defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
#include "tdd_disp_gc9d01.h"
#endif
 /***********************************************************
 ************************macro define************************
 ***********************************************************/
 #define BOARD_SPEAKER_EN_PIN TUYA_GPIO_NUM_27
 
 #define BOARD_BUTTON_PIN       TUYA_GPIO_NUM_MAX   
 #define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
 
 #define BOARD_LED_PIN       TUYA_GPIO_NUM_MAX
 #define BOARD_LED_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH
 
 #if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1) || defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1) || defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
 
 #define BOARD_LCD_BL_TYPE      TUYA_DISP_BL_TP_GPIO
 #define BOARD_LCD_BL_PIN       TUYA_GPIO_NUM_5
 #define BOARD_LCD_BL_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH
 
#if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1) 
#define BOARD_LCD_WIDTH      240
#define BOARD_LCD_HEIGHT     240

#define BOARD_LCD_X_OFFSET   0
#define BOARD_LCD_Y_OFFSET   0

#elif defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1)
#define BOARD_LCD_WIDTH      160
#define BOARD_LCD_HEIGHT     80

#define BOARD_LCD_X_OFFSET   1
#define BOARD_LCD_Y_OFFSET   0x1A

#elif defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
#define BOARD_LCD_WIDTH      160
#define BOARD_LCD_HEIGHT     160

#define BOARD_LCD_X_OFFSET   0
#define BOARD_LCD_Y_OFFSET   0

#endif
 
 #define BOARD_LCD_SPI_CS_PIN  TUYA_GPIO_NUM_13
 #define BOARD_LCD_PIXELS_FMT TUYA_PIXEL_FMT_RGB565
 #define BOARD_LCD_ROTATION   TUYA_DISPLAY_ROTATION_0
 
 #define BOARD_LCD_SPI_PORT    TUYA_SPI_NUM_0
 #define BOARD_LCD_SPI_CLK     48000000
 
 #define BOARD_LCD_SPI_DC_PIN  TUYA_GPIO_NUM_17
 #define BOARD_LCD_SPI_RST_PIN TUYA_GPIO_NUM_19
 
 #define BOARD_LCD_PIXELS_FMT TUYA_PIXEL_FMT_RGB565
 
 #define BOARD_LCD_POWER_PIN       TUYA_GPIO_NUM_MAX
 #define BOARD_LCD_POWER_ACTIVE_LV TUYA_GPIO_LEVEL_HIGH
 
 #endif
 /***********************************************************
 ***********************typedef define***********************
 ***********************************************************/
 
 /***********************************************************
 ********************function declaration********************
 ***********************************************************/
 
 /***********************************************************
 ***********************variable define**********************
 ***********************************************************/
 #if defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1)
const uint8_t cST7735S_XLT_INIT_SEQ[] = {
    1,    120,  0x11, 
    1,    0,  0x21, 
    1,    0,  0x21, 
    4,    100,  0xB1, 0x05,0x3A,0x3A,
    4,    0,    0xB2, 0x05,0x3A,0x3A,
    7,    0,    0xB3, 0x05,0x3A,0x3A, 0x05,0x3A,0x3A,
    2,    0,    0xB4, 0x03,
    4,    0,    0xC0, 0x62,0x02,0x04,
    2,    0,    0xC1, 0xC0,
    3,    0,    0xC2, 0x0D, 0x00,
    3,    0,    0xC3, 0x8A, 0x6A,
    3,    0,    0xC4, 0x8D, 0xEE,
    2,    0,    0xC5, 0x0E,
    17,   0,    0xE0, 0x10,0x0E,0x02,0x03,0x0E,0x07,0x02,0x07,0x0A,0x12,0x27,0x37,0x00,0x0D,0x0E,0x10,
    17,   0,    0xE1, 0x10,0x0E,0x03,0x03,0x0F,0x06,0x02,0x08,0x0A,0x13,0x26,0x36,0x00,0x0D,0x0E,0x10,
    2,    0,    0x3A, 0x05, 
    2,    0,    0x36, 0xA8,
    1,    0,    0x29,
    0 // Terminate list                    // Terminate list
};
#endif

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
 
 static OPERATE_RET __board_register_button(void)
 {
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
 
     return rt;
 }
 
 static OPERATE_RET __board_register_led(void)
 {
     OPERATE_RET rt = OPRT_OK;
 
 #if defined(LED_NAME)
     TDD_LED_GPIO_CFG_T led_gpio;
 
     led_gpio.pin = BOARD_LED_PIN;
     led_gpio.level = BOARD_LED_ACTIVE_LV;
     led_gpio.mode = TUYA_GPIO_PUSH_PULL;
 
     TUYA_CALL_ERR_RETURN(tdd_led_gpio_register(LED_NAME, &led_gpio));
 #endif
 
     return rt;
 }
 
 #if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1) || defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1) || defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
 static OPERATE_RET __board_register_display(void)
 {
     OPERATE_RET rt = OPRT_OK;
 
 #if defined(DISPLAY_NAME)
     DISP_SPI_DEVICE_CFG_T display_cfg;
 
     memset(&display_cfg, 0, sizeof(DISP_SPI_DEVICE_CFG_T));
 
     display_cfg.bl.type = BOARD_LCD_BL_TYPE;
     display_cfg.bl.gpio.pin = BOARD_LCD_BL_PIN;
     display_cfg.bl.gpio.active_level = BOARD_LCD_BL_ACTIVE_LV;
 
     display_cfg.width = BOARD_LCD_WIDTH;
     display_cfg.height = BOARD_LCD_HEIGHT;
     display_cfg.x_offset = BOARD_LCD_X_OFFSET;
     display_cfg.y_offset = BOARD_LCD_Y_OFFSET;
     display_cfg.pixel_fmt = BOARD_LCD_PIXELS_FMT;
     display_cfg.rotation = BOARD_LCD_ROTATION;
 
     display_cfg.port = BOARD_LCD_SPI_PORT;
     display_cfg.spi_clk = BOARD_LCD_SPI_CLK;
     display_cfg.cs_pin = BOARD_LCD_SPI_CS_PIN;
     display_cfg.dc_pin = BOARD_LCD_SPI_DC_PIN;
     display_cfg.rst_pin = BOARD_LCD_SPI_RST_PIN;
 
     display_cfg.power.pin = BOARD_LCD_POWER_PIN;
     display_cfg.power.active_level = BOARD_LCD_POWER_ACTIVE_LV;
 
#if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1)
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7789_register(DISPLAY_NAME, &display_cfg));
#elif defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1)
    tdd_disp_spi_st7735s_set_init_seq(cST7735S_XLT_INIT_SEQ);
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_st7735s_register(DISPLAY_NAME, &display_cfg));
#elif defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
    TUYA_CALL_ERR_RETURN(tdd_disp_spi_gc9d01_register(DISPLAY_NAME, &display_cfg));
#endif
 #endif
 
     return rt;
 }
 #endif
 
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
 
 #if defined(T5AI_OTTO_EX_MODULE_ST7789) && (T5AI_OTTO_EX_MODULE_ST7789 == 1) || defined(T5AI_OTTO_EX_MODULE_ST7735S_XLT) && (T5AI_OTTO_EX_MODULE_ST7735S_XLT == 1) || defined(T5AI_OTTO_EX_MODULE_GC9D01) && (T5AI_OTTO_EX_MODULE_GC9D01 == 1)
     TUYA_CALL_ERR_LOG(__board_register_display());
 #endif
 
     return rt;
 }