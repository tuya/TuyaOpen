/**
 * @file board_config.h
 * @brief Xteink X4 (ESP32-C3) board pinout and constants.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note Panel and GPIO map align with the OpenX4 community-sdk targets and
 *       published X4 pinouts (SSD1677 800x480, shared SPI SD, resistor-ladder keys).
 *       Community SDK: https://github.com/open-x4-epaper/community-sdk
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "sdkconfig.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SPI / EPD (SSD1677, OpenX4 community-sdk EInkDisplay uses 40 MHz on X4) */
#define X4_EPD_SPI_HOST_ID TUYA_SPI_NUM_0
#define X4_EPD_SPI_FREQ_HZ (40000000U)

#define X4_EPD_PIN_SCLK TUYA_GPIO_NUM_8
#define X4_EPD_PIN_MOSI TUYA_GPIO_NUM_10
#define X4_EPD_PIN_CS   TUYA_GPIO_NUM_21
#define X4_EPD_PIN_DC   TUYA_GPIO_NUM_4
#define X4_EPD_PIN_RST  TUYA_GPIO_NUM_5
#define X4_EPD_PIN_BUSY TUYA_GPIO_NUM_6

/* Reserved spare GPIO kept for boards that add external EPD power switching. */
#define X4_EPD_PIN_PWR_DUMMY TUYA_GPIO_NUM_11

/* microSD shares SPI data lines; the EPD driver manually controls chip select. */
#define X4_SD_PIN_CS   TUYA_GPIO_NUM_12
#define X4_SD_PIN_MISO TUYA_GPIO_NUM_7

/* Buttons: two ADC resistor ladders + dedicated power key */
#define X4_BTN_ADC1_PIN  TUYA_GPIO_NUM_1
#define X4_BTN_ADC2_PIN  TUYA_GPIO_NUM_2
#define X4_BTN_POWER_PIN TUYA_GPIO_NUM_3

/* Battery sense (voltage divider to ADC) */
#define X4_BATTERY_ADC_PIN      TUYA_GPIO_NUM_0
#define X4_BATTERY_DIVIDER_MULT (2.0f)
/** LiPo series cell count at the pack (1 = single cell; use 2 if pack is 2S). */
#define X4_BATTERY_CELL_COUNT (1U)

/* USB charge indication (active level may vary; treat HIGH as charging) */
#define X4_CHARGE_SENSE_PIN TUYA_GPIO_NUM_20

#define X4_EPD_WIDTH  (800U)
#define X4_EPD_HEIGHT (480U)

/**
 * Physical panel viewable inset (native 800x480, landscape counter-clockwise mapping).
 * Source: CrossPoint Reader GfxRenderer::VIEWABLE_MARGIN_* and getOrientedViewableTRBL()
 * for Orientation::LandscapeCounterClockwise (example/crosspoint-reader/lib/GfxRenderer/GfxRenderer.h).
 * Maps portrait logical margins (top=9, right=3, bottom=3, left=3) to physical edges:
 * left=9, top=3, right=3, bottom=3.
 */
#define X4_PANEL_VIEWABLE_TOP_PX     3U
#define X4_PANEL_VIEWABLE_RIGHT_PX   3U
#define X4_PANEL_VIEWABLE_BOTTOM_PX  3U
#define X4_PANEL_VIEWABLE_LEFT_PX    9U

#define X4_PANEL_VIEWABLE_WIDTH  (X4_EPD_WIDTH - X4_PANEL_VIEWABLE_LEFT_PX - X4_PANEL_VIEWABLE_RIGHT_PX)
#define X4_PANEL_VIEWABLE_HEIGHT (X4_EPD_HEIGHT - X4_PANEL_VIEWABLE_TOP_PX - X4_PANEL_VIEWABLE_BOTTOM_PX)

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */
