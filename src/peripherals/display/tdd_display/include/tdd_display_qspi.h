/**
 * @file tdd_display_qspi.h
 * @brief tdd_display_qspi module is used to implement the QSPI interface for display devices.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISPLAY_QSPI_H__
#define __TDD_DISPLAY_QSPI_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"
#include "tdd_disp_type.h"

#if defined(ENABLE_QSPI) && (ENABLE_QSPI == 1)

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
	QSPI_REFRESH_BY_LINE,
    QSPI_REFRESH_BY_FRAME
} QSPI_REFRESH_METHOD_E;

typedef struct {
	uint8_t  hsync_cmd;
	uint8_t  vsync_cmd;
	uint8_t  vsw;
	uint8_t  hfp;
	uint8_t  hbp;
    uint16_t line_len;
} QSPI_REFRESH_CFG_BY_LINE_T;

typedef struct {
    uint8_t cmd;
    uint8_t cmd_lines;
    uint8_t addr[4];
    uint8_t addr_size;
    TUYA_QSPI_WIRE_MODE_E addr_lines;
    uint8_t command_index;
    uint32_t dummy_cycle;
} QSPI_CMD_FRAME_T;

typedef struct {
    uint16_t                    width;
    uint16_t                    height;
    TUYA_DISPLAY_PIXEL_FMT_E    pixel_fmt;
    TUYA_GPIO_NUM_E             rst_pin;
    TUYA_GPIO_NUM_E             cs_pin;
    TUYA_QSPI_NUM_E             port;
    uint32_t                    freq_hz;
    TUYA_QSPI_MODE_E            mode;
    TUYA_QSPI_WIRE_MODE_E       data_lines;
    TUYA_GPIO_NUM_E             te_pin;
    TUYA_GPIO_LEVEL_E           te_active_level;
    QSPI_REFRESH_METHOD_E       refresh_method;
    QSPI_CMD_FRAME_T            pixel_cmd_1wire;
    QSPI_CMD_FRAME_T            pixel_cmd_4wire;
    uint8_t                     has_vram;
    uint8_t                     cmd_caset;
    uint8_t                     cmd_raset;
    uint8_t                     cmd_opcode;
    uint8_t                     x_offset;
    uint8_t                     y_offset;
    bool                        use_dma;
    bool                        te_sync_enable;
    uint8_t                     te_wait_div;
}DISP_QSPI_BASE_CFG_T;

typedef struct { 
    DISP_QSPI_BASE_CFG_T        cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;    
    TUYA_DISPLAY_ROTATION_E     rotation;
    bool                        is_swap;
    const uint8_t              *init_seq; // Initialization commands for the display
}TDD_DISP_QSPI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a QSPI display device.
 *
 * @param name Device name for identification.
 * @param spi Pointer to QSPI configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdd_disp_qspi_device_register(char *name, TDD_DISP_QSPI_CFG_T *spi);

/**
 * @brief Register a QSPI panel by combining its static protocol template with
 *        board-provided port, pin and timing settings.
 */
OPERATE_RET tdd_disp_qspi_panel_register(char *name,
                                         const TDD_DISP_QSPI_CFG_T *panel_cfg,
                                         const DISP_QSPI_DEVICE_CFG_T *dev_cfg);

/**
 * @brief Sends a command with optional data to the display via QSPI interface.
 * 
 * @param p_cfg Pointer to the QSPI base configuration structure.
 * @param cmd The command byte to be sent.
 * @param data Pointer to the data buffer to be sent (can be NULL if no data).
 * @param data_len Length of the data buffer in bytes.
 * 
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdd_disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *p_cfg, uint8_t cmd, \
                                        uint8_t *data, uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __TDD_DISPLAY_QSPI_H__ */