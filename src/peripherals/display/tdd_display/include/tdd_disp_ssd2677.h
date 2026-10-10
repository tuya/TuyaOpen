/**
 * @file tdd_disp_ssd2677.h
 * @brief SSD2677ZB hardware-SPI driver for the SEEKINK E0397A09-A0 panel.
 *
 * Supports synchronous full-screen monochrome refresh at the configured dimensions. Input follows
 * TDL drawing layout: LSB first, 1 = black and 0 = white. BUSY is required: LOW = busy, HIGH = ready.
 * Input rows use a top-left origin. The driver selects UD=0 and sends white
 * padding before the image to initialize the entire controller scan domain.
 * Image rows are transmitted in forward order; the caller's buffer is unchanged.
 * SDA is bidirectional; no separate MISO wire is connected to the panel.
 * Fast/partial refresh, grayscale and rotated framebuffers are not supported.
 * Buffers from external drawing libraries must be converted to TDL layout;
 * passing MSB-first buffers directly reverses pixels within each group of eight.
 * Commands and image bytes use synchronous hardware SPI, mode 0, MSB first.
 * Temperature reads temporarily release SPI and use CLK/SDA as GPIO, then
 * restore the SPI controller and pin mux. CS is always controlled as GPIO.
 * CLK/SDA must be the selected port's default CLK/MOSI pins. Use a dedicated
 * SPI port; this driver is not a shared-bus arbiter. On T5AI the adapter also
 * maps the unconnected default MISO pin (GPIO5 for SPI1); reserve it accordingly.
 * T5AI's tkl_spi_deinit releases both SPI controllers, so SDA handover requires
 * that no other SPI0/SPI1 device be active concurrently on that platform.
 * SPI IRQ/asynchronous mode must remain disabled. DMA is not enabled here.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TDD_DISP_SSD2677_H__
#define __TDD_DISP_SSD2677_H__

#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Pin configuration for the E0397A09-A0 panel. All six pins are required. */
typedef struct {
    uint16_t        width;             /**< Visible width: nonzero, <= 960 and divisible by eight. */
    uint16_t        height;            /**< Visible height: nonzero and <= the effective controller height. */
    uint16_t        controller_height; /**< Command 0x61 height: 420..680; 0 uses height. E0397A09-A0 uses 680. */
    TUYA_SPI_NUM_E  port;              /**< Dedicated hardware SPI port. */
    uint32_t        spi_clk;           /**< SPI frequency in Hz; validated T5AI setting: 4000000. */
    TUYA_GPIO_NUM_E clk_pin;           /**< SPI clock pin, also used for temperature-read GPIO handover. */
    TUYA_GPIO_NUM_E sda_pin;           /**< Bidirectional SPI data. */
    TUYA_GPIO_NUM_E cs_pin;            /**< Active-low chip select. */
    TUYA_GPIO_NUM_E dc_pin;            /**< LOW for command, HIGH for data. */
    TUYA_GPIO_NUM_E rst_pin;           /**< Active-low reset. */
    TUYA_GPIO_NUM_E busy_pin;          /**< Active-low busy, required for bounded waits. */
    TUYA_DISPLAY_IO_CTRL_T power;      /**< Optional enable; set pin = TUYA_GPIO_NUM_MAX when absent. */
} DISP_EINK_SSD2677_CFG_T;

/**
 * @brief Register an SSD2677ZB monochrome display with caller-supplied dimensions.
 *
 * The driver copies the configuration and keeps independent state per device.
 * Flush accepts a complete width * height / 8 byte TUYA_PIXEL_FMT_MONOCHROME frame in TDL drawing layout
 * at (0, 0). The caller retains ownership on error; free_cb is called only after
 * a successful synchronous refresh, matching the other TDD display drivers.
 * Each flush resets the panel to restore internal temperature sensing, loads
 * the panel's OTP waveform, refreshes and powers the internal booster off.
 * Close enters deep sleep and releases SPI and the six bus/control pins.
 * Register once per device; TDL currently has no device unregister operation.
 * The OTP/booster profile is validated on E0397A09-A0; configurable dimensions
 * alone do not guarantee support for other panels. Other SSD2677 panels may need
 * different panel timings and OTP settings. The display CMake source glob
 * includes this driver when ENABLE_DISPLAY and ENABLE_SPI are selected.
 * On E0397A09-A0, set height = 480 and controller_height = 680: hardware A/B
 * tests with both UD=1 and UD=0 showed incomplete refresh with 0x61 height
 * 480; UD=0 with scan height 680 and leading white padding refreshed all rows. This command field is not a
 * claim about the panel's physical row count; do not omit its override here.
 * With UD=0, flush writes controller_height - height white rows before the
 * image, followed by height image rows in forward order. Fixed-orientation
 * tests confirmed that omitting the 200 white rows at 800x480/680 clips the
 * image and leaves corruption; including them restores both corner markers.
 * The framebuffer remains width * height / 8 bytes; no padded frame is allocated.
 *
 * Example for the validated T5AI wiring (no external power-enable signal):
 * @code
 * const DISP_EINK_SSD2677_CFG_T cfg = {
 *     .width = 800, .height = 480, .controller_height = 680,
 *     .port = TUYA_SPI_NUM_1, .spi_clk = 4000000,
 *     .clk_pin = TUYA_GPIO_NUM_2, .sda_pin = TUYA_GPIO_NUM_4,
 *     .cs_pin = TUYA_GPIO_NUM_3, .dc_pin = TUYA_GPIO_NUM_7,
 *     .rst_pin = TUYA_GPIO_NUM_8, .busy_pin = TUYA_GPIO_NUM_6,
 *     .power = {.pin = TUYA_GPIO_NUM_MAX},
 * };
 * tdd_disp_spi_mono_ssd2677_register("display", &cfg);
 * @endcode
 *
 * @param name Unique display name; must fit DISPLAY_DEV_NAME_MAX_LEN.
 * @param dev_cfg Six distinct valid GPIO pins plus an optional power-enable pin.
 * @return OPRT_OK on success, or a parameter, allocation or registration error.
 */
OPERATE_RET tdd_disp_spi_mono_ssd2677_register(char *name, const DISP_EINK_SSD2677_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SSD2677_H__ */
