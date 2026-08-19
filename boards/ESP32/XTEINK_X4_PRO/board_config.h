/**
 * @file board_config.h
 * @brief Xteink X4 Pro (ESP32-S3) board pinout and constants.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note Hardware map recovered and confirmed on hardware by the FreeInk /
 *       CrossPoint Reader project (freeink-sdk/docs/xteink-x4pro-support.md,
 *       BoardConfig::XTEINK_X4_PRO). The X4 Pro is an ESP32-S3 device
 *       (16 MB flash, 8 MB octal PSRAM) and is electrically distinct from
 *       the ESP32-C3 Xteink X4.
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * EPD — SSD1677 800x480 B/W panel (some production batches ship a UC8179;
 * both accept the same OTP-waveform init below). Write-only SPI: no MISO.
 * OEM clocks the bus at 5 MHz; 20 MHz is well within spec and matches the
 * FreeInk profile. BUSY is active-HIGH.
 * --------------------------------------------------------------------------- */
#define X4PRO_EPD_SPI_PORT    TUYA_SPI_NUM_0
#define X4PRO_EPD_SPI_FREQ_HZ (20000000U)

#define X4PRO_EPD_PIN_SCLK TUYA_GPIO_NUM_12
#define X4PRO_EPD_PIN_MOSI TUYA_GPIO_NUM_11
#define X4PRO_EPD_PIN_CS   TUYA_GPIO_NUM_13
#define X4PRO_EPD_PIN_DC   TUYA_GPIO_NUM_18
#define X4PRO_EPD_PIN_RST  TUYA_GPIO_NUM_14
#define X4PRO_EPD_PIN_BUSY TUYA_GPIO_NUM_6

#define X4PRO_EPD_WIDTH  (800U)
#define X4PRO_EPD_HEIGHT (480U)

/**
 * Panel bezel overlap (FreeInk ViewableInsets top/right/bottom/left = 9/7/3/7):
 * the panel is recessed and 7 px side insets keep edge-hugging chrome visible.
 */
#define X4PRO_PANEL_VIEWABLE_TOP_PX    9U
#define X4PRO_PANEL_VIEWABLE_RIGHT_PX  7U
#define X4PRO_PANEL_VIEWABLE_BOTTOM_PX 3U
#define X4PRO_PANEL_VIEWABLE_LEFT_PX   7U

#define X4PRO_PANEL_VIEWABLE_WIDTH  (X4PRO_EPD_WIDTH - X4PRO_PANEL_VIEWABLE_LEFT_PX - X4PRO_PANEL_VIEWABLE_RIGHT_PX)
#define X4PRO_PANEL_VIEWABLE_HEIGHT (X4PRO_EPD_HEIGHT - X4PRO_PANEL_VIEWABLE_TOP_PX - X4PRO_PANEL_VIEWABLE_BOTTOM_PX)

/* ---------------------------------------------------------------------------
 * Input — three digital active-LOW buttons (INPUT_PULLUP, NOT an ADC ladder)
 * plus the capacitive Home key delivered by the GT911 touch controller.
 * NOTE: GPIO0 is a boot-strap pin; it works as a key unless held during reset.
 * --------------------------------------------------------------------------- */
#define X4PRO_BTN_LEFT_PIN  TUYA_GPIO_NUM_0
#define X4PRO_BTN_RIGHT_PIN TUYA_GPIO_NUM_7
#define X4PRO_BTN_POWER_PIN TUYA_GPIO_NUM_3

/* ---------------------------------------------------------------------------
 * Frontlight — dual warm/cold PWM, active-HIGH (10 kHz on the OEM).
 * cool/white = GPIO8, warm = GPIO9.
 * --------------------------------------------------------------------------- */
#define X4PRO_FRONTLIGHT_PIN_COOL TUYA_GPIO_NUM_8
#define X4PRO_FRONTLIGHT_PIN_WARM TUYA_GPIO_NUM_9
#define X4PRO_FRONTLIGHT_FREQ_HZ  (10000U)

/* ---------------------------------------------------------------------------
 * Shared I2C bus (SDA=39 / SCL=38, 400 kHz):
 *   0x5D GT911 capacitive touch (INT=GPIO10, RST=GPIO4, rail enable=GPIO2
 *        ACTIVE-LOW — the controller is unpowered/silent until GPIO2 is LOW
 *        with GPIO1 HIGH; portrait-mounted, reports X:0..480 Y:0..800)
 *   0x63 CW2017 battery fuel gauge (needs the 80-byte BATINFO profile)
 *   0x51 BM8563 RTC (PCF8563-compatible; not driven by this BSP)
 * --------------------------------------------------------------------------- */
#define X4PRO_I2C_PORT     TUYA_I2C_NUM_0
#define X4PRO_I2C_PIN_SDA  TUYA_GPIO_NUM_39
#define X4PRO_I2C_PIN_SCL  TUYA_GPIO_NUM_38
#define X4PRO_I2C_SPEED_HZ (400000U)

#define X4PRO_TOUCH_PIN_INT     TUYA_GPIO_NUM_10
#define X4PRO_TOUCH_PIN_RST     TUYA_GPIO_NUM_4
#define X4PRO_TOUCH_PIN_PWR     TUYA_GPIO_NUM_2 /* active-LOW rail enable */
#define X4PRO_TOUCH_I2C_ADDR    (0x5DU)
#define X4PRO_TOUCH_I2C_ADDRAlt (0x14U)

#define X4PRO_BATT_GAUGE_I2C_ADDR (0x63U)

/* ---------------------------------------------------------------------------
 * Power rails (board init order matters; see xteink_x4_pro.c):
 *   GPIO1 — master peripheral rail, driven HIGH first (required for touch).
 *   GPIO2 — GT911 power enable, ACTIVE-LOW.
 *   GPIO5 — SD data-path enable, ACTIVE-LOW; pulsed HIGH->LOW before mount
 *           and must stay LOW afterwards (HIGH breaks block reads, 0x107).
 * --------------------------------------------------------------------------- */
#define X4PRO_RAIL_PERIPH_PIN  TUYA_GPIO_NUM_1
#define X4PRO_SD_PIN_PWR       TUYA_GPIO_NUM_5

/* ---------------------------------------------------------------------------
 * SD card — native SDMMC (the card is silent to SPI-mode CMD0), 1-bit,
 * slot 1, internal pull-ups, 40 MHz.
 * --------------------------------------------------------------------------- */
#define X4PRO_SD_PIN_CLK  TUYA_GPIO_NUM_41
#define X4PRO_SD_PIN_CMD  TUYA_GPIO_NUM_42
#define X4PRO_SD_PIN_DAT0 TUYA_GPIO_NUM_40

/* USB native port (D-/D+): never repurpose, no I2C/GPIO probe across them. */
#define X4PRO_USB_PIN_DM TUYA_GPIO_NUM_19
#define X4PRO_USB_PIN_DP TUYA_GPIO_NUM_20

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */
