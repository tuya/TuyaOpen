/**
 * @file tdd_display_spi.h
 * @brief SPI display driver interface definitions.
 *
 * This header provides macro definitions and function declarations for
 * controlling SPI displays via serial peripheral interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISPLAY_SPI_H__
#define __TDD_DISPLAY_SPI_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI == 1)

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t                 width;
    uint16_t                 height;
    TUYA_DISPLAY_PIXEL_FMT_E pixel_fmt;
    TUYA_GPIO_NUM_E          cs_pin;
    TUYA_GPIO_NUM_E          dc_pin;
    TUYA_GPIO_NUM_E          rst_pin;
    TUYA_SPI_NUM_E           port;
    uint32_t                 spi_clk;
    uint8_t                  cmd_caset;
    uint8_t                  cmd_raset;
    uint8_t                  cmd_ramwr;
    uint8_t                  x_offset;
    uint8_t                  y_offset;
} DISP_SPI_BASE_CFG_T;

typedef struct { 
    DISP_SPI_BASE_CFG_T         cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_ROTATION_E     rotation;
    bool                        is_swap; 
    const uint8_t              *init_seq;      // Initialization commands for the display
}TDD_DISP_SPI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an RGB display device with the display management system.
 *
 * This function creates and initializes a new RGB display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param rgb Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */  
OPERATE_RET tdd_disp_spi_device_register(char *name, TDD_DISP_SPI_CFG_T *spi);

/**
 * @brief Initializes the SPI interface for display communication.
 *
 * This function sets up the SPI port and its associated semaphore for synchronization, 
 * and initializes the required GPIO pins for SPI-based display operations.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing port and clock settings.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if initialization fails.
 */
OPERATE_RET tdd_disp_spi_init(DISP_SPI_BASE_CFG_T *p_cfg);

/**
 * @brief Sends a command over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) and data/command (DC) pins low to indicate 
 * command transmission, then sends the specified command byte via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param cmd The command byte to be sent to the display.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the command fails.
 */
OPERATE_RET tdd_disp_spi_send_cmd(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t cmd);

/**
 * @brief Sends data over the SPI interface to the display device.
 *
 * This function pulls the chip select (CS) pin low and sets the data/command (DC) pin high 
 * to indicate data transmission, then sends the specified data buffer via SPI.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param data Pointer to the data buffer to be sent.
 * @param data_len Length of the data buffer in bytes.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if sending the data fails.
 */
OPERATE_RET tdd_disp_spi_send_data(DISP_SPI_BASE_CFG_T *p_cfg, uint8_t *data, uint32_t data_len);

/**
 * @brief Executes the display initialization sequence over SPI.
 *
 * This function processes a command-based initialization sequence, sending commands 
 * and associated data to the display device to configure it during initialization.
 *
 * @param p_cfg Pointer to the SPI configuration structure containing pin and port settings.
 * @param init_seq Pointer to the initialization sequence array (command/data format).
 *
 * @return None.
 */
void tdd_disp_spi_init_seq(DISP_SPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq);

/**
 * @brief Modifies a parameter in the display initialization sequence for a specific command.
 *
 * This function searches for the specified command in the initialization sequence and 
 * updates the parameter at the given index. If the index is out of bounds, an error is logged.
 *
 * @param init_seq Pointer to the initialization sequence array.
 * @param init_cmd The command whose parameter needs to be modified.
 * @param param The new parameter value to set.
 * @param idx The index of the parameter to modify within the command's data block.
 *
 * @return None.
 */
void tdd_disp_modify_init_seq_param(uint8_t *init_seq, uint8_t init_cmd, uint8_t param, uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __TDD_DISPLAY_SPI_H__ */