/**
 * @file tdd_disp_ssd2677.h
 * @brief SSD2677 hardware-SPI monochrome display driver.
 *
 * Supports synchronous full-screen and partial-window refresh using the panel's
 * OTP waveform. Fast refresh and grayscale are not supported.
 *
 * SPI uses mode 0, MSB first and GPIO-controlled CS. Temperature reads temporarily
 * switch CLK/SDA to GPIO; SDA is bidirectional and needs no separate MISO wire.
 * Use the selected port's default CLK/MOSI pins and a dedicated SPI controller.
 * On T5AI, reserve the unused MISO pin (GPIO5 for SPI1); SPI deinitialization
 * also affects the other controller, so no other SPI0/SPI1 device may be active.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TDD_DISP_SSD2677_H__
#define __TDD_DISP_SSD2677_H__

#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Panel dimensions, SPI settings and six required signal pins. */
typedef struct {
    uint16_t        width;             /**< Visible width: nonzero, <= 960 and divisible by eight. */
    uint16_t        height;            /**< Visible height: nonzero and <= the effective controller_height. */
    uint16_t        controller_height; /**< Scan height (0x61): 420..680; 0 uses height. */
    TUYA_SPI_NUM_E  port;              /**< Dedicated hardware SPI port. */
    uint32_t        spi_clk;           /**< SPI frequency in Hz; validated T5AI setting: 4000000. */
    TUYA_GPIO_NUM_E clk_pin;           /**< SPI CLK; also used as GPIO for temperature reads. */
    TUYA_GPIO_NUM_E sda_pin;           /**< Bidirectional SPI data. */
    TUYA_GPIO_NUM_E cs_pin;            /**< Active-low chip select. */
    TUYA_GPIO_NUM_E dc_pin;            /**< LOW for command, HIGH for data. */
    TUYA_GPIO_NUM_E rst_pin;           /**< Active-low reset. */
    TUYA_GPIO_NUM_E busy_pin;          /**< LOW while busy, HIGH when ready. */
    TUYA_DISPLAY_IO_CTRL_T power;      /**< Optional enable; set pin = TUYA_GPIO_NUM_MAX when absent. */
} DISP_EINK_SSD2677_CFG_T;

/**
 * @brief Register an SSD2677 display with a copied configuration.
 *
 * Frames use TUYA_PIXEL_FMT_MONOCHROME: LSB-first pixels, 1 = black, 0 = white,
 * with a top-left origin. x_start and width must be multiples of eight; windows
 * must fit within the visible screen and len must equal width * height / 8.
 *
 * A successful full-screen refresh is required before partial updates and after
 * reset, close/reopen or a failed refresh. Partial updates reuse the last full
 * refresh's temperature and waveform; use a full refresh to reload them.
 *
 * Full refresh resets the panel and powers the booster off after completion.
 * Close enters deep sleep and releases SPI/GPIO resources. On refresh failure,
 * power-off is attempted only if BUSY confirms idle. Failed close can be retried.
 *
 * Open, flush and close are serialized per device. On successful refresh, free_cb
 * runs after unlocking; on error the caller retains ownership of the frame.
 * Register once per device; TDL does not provide an unregister operation.
 *
 * @note controller_height describes the scan domain, independently of visible
 * height. Full refresh sends controller_height - height white rows before the
 * image; the caller supplies only the visible framebuffer. Set dimensions for
 * the attached panel. OTP and timing settings may also need panel-specific changes.
 *
 * @param name Unique, nonempty name of at most DISPLAY_DEV_NAME_MAX_LEN characters.
 * @param dev_cfg Panel configuration with six distinct pins and optional power control.
 * @return OPRT_OK on success, otherwise an SDK error code.
 */
OPERATE_RET tdd_disp_spi_mono_ssd2677_register(char *name, const DISP_EINK_SSD2677_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SSD2677_H__ */
