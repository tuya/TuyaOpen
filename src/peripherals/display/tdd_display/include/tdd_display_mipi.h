/**
 * @file tdd_display_mipi.h
 * @brief MIPI-DSI display driver interface definitions.
 *
 * The bus driver owns the DSI link only: it initialises the controller, pulses
 * the panel reset, sends the init sequence and hands frames to the controller.
 * Panel timing and the init sequence belong to the panel driver, and wiring
 * belongs to the board.
 *
 * Frames are not allocated here. Whatever buffer the display manager hands over
 * is submitted straight to the controller and released through its free_cb, so
 * a port that renders directly into these buffers pays no copy.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISPLAY_MIPI_H__
#define __TDD_DISPLAY_MIPI_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief One DCS write in the panel init sequence.
 *
 * A pseudo entry with @ref cmd == 0 performs no bus transaction and only waits
 * @ref delay_ms, so a sequence can express timings between commands.
 */
typedef struct {
    uint8_t        cmd;
    uint16_t       delay_ms; /**< wait after the command, 0 = none */
    const uint8_t *data;
    uint16_t       len; /**< 0 = command only */
} TDD_DISP_MIPI_INIT_CMD_T;

/**
 * @brief What the panel is. Owned by the panel driver, invisible to boards.
 */
typedef struct {
    uint16_t                        width;
    uint16_t                        height;
    uint8_t                         lanes;
    uint8_t                         fps;
    TUYA_DISPLAY_PIXEL_FMT_E        pixel_fmt;
    uint16_t                        hsync_pulse_width;
    uint16_t                        hsync_back_porch;
    uint16_t                        hsync_front_porch;
    uint16_t                        vsync_pulse_width;
    uint16_t                        vsync_back_porch;
    uint16_t                        vsync_front_porch;
    uint16_t                        reset_delay_ms[3]; /**< idle, active, release; panel defaults */
    const TDD_DISP_MIPI_INIT_CMD_T *init_cmds;
    uint16_t                        init_cmds_num;
    bool                            has_vram;
} TDD_DISP_MIPI_PANEL_T;

/**
 * @brief How this board wires the panel. The only structure a board fills in.
 */
typedef struct {
    TUYA_DISPLAY_IO_CTRL_T  reset;           /**< pin >= TUYA_GPIO_NUM_MAX = not wired */
    TUYA_DISPLAY_IO_CTRL_T  power;           /**< pin >= TUYA_GPIO_NUM_MAX = not wired */
    TUYA_DISPLAY_BL_CTRL_T  bl;              /**< applied by tdl_disp_dev_open() */
    uint16_t                reset_delay_ms[3]; /**< 0 = panel default */
    TUYA_DISPLAY_ROTATION_E rotation;
    bool                    is_swap;
    bool                    input_decompress; /**< false = controller reads pixels as-is */
    TUYA_GPIO_DRIVE_CAP_E   gpio_drive_cap;   /**< reset and power pins */
    uint8_t                 max_inflight;     /**< 0 = default; capped at 4 */
} TDD_DISP_MIPI_BOARD_IO_T;

/**
 * @brief Merged panel + board description handed to the bus driver.
 *
 * The init table must live in static storage: only this structure is copied.
 */
typedef struct {
    TUYA_MIPI_BASE_CFG_T            cfg;   /**< timing, lanes, format, pixel clock */
    TUYA_DISPLAY_IO_CTRL_T          reset;
    TUYA_DISPLAY_IO_CTRL_T          power;
    TUYA_DISPLAY_BL_CTRL_T          bl;
    const TDD_DISP_MIPI_INIT_CMD_T *init_cmds;
    uint16_t                        init_cmds_num;
    uint16_t                        reset_delay_ms[3]; /**< idle, active, release; required */
    TUYA_DISPLAY_ROTATION_E         rotation;
    bool                            is_swap;
    bool                            input_decompress;
    bool                            has_vram;
    TUYA_GPIO_DRIVE_CAP_E           gpio_drive_cap;
    uint8_t                         max_inflight;
} TDD_DISP_MIPI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a MIPI-DSI display device with the display management system.
 *
 * Frame ownership follows tkl_mipi_dsi_flush(): once a frame with free_cb is
 * accepted it is released exactly once through that callback. Frames rejected
 * before submission are released through free_cb before the error returns.
 * OPRT_RESOURCE_NOT_READY means the device is closed and the caller still owns
 * the frame.
 *
 * @param name Name of the display device (used for identification).
 * @param mipi Merged panel and board configuration.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_disp_mipi_device_register(char *name, const TDD_DISP_MIPI_CFG_T *mipi);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISPLAY_MIPI_H__ */
