/**
 * @file disp_sw_spi_driver.h
 * @version 0.1
 * @date 2025-03-18
 */

#ifndef __DISP_SW_SPI_DRIVER_H__
#define __DISP_SW_SPI_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the display using a sequence of commands.
 * 
 * This function initializes the LCD display by sending a series of commands from the provided
 * initialization sequence. It performs the following steps:
 * 1. Calls `disp_sw_spi_init` to initialize the GPIO pins for the software SPI interface.
 * 2. Iterates through the initialization sequence, where each command is followed by a delay.
 * 3. For each command in the sequence:
 *    - Extracts the command data and the number of data bytes to follow.
 *    - Calls `disp_sw_spi_lcd_write_cmd` to send the command and its associated data.
 *    - Waits for a specified delay before proceeding to the next command.
 * 
 * @param init_seq A pointer to the initialization sequence array. Each command in the sequence
 *                 is followed by a byte indicating the number of data bytes and then the data bytes themselves.
 */
void disp_sw_spi_lcd_init_seq(const uint8_t *init_seq);

#ifdef __cplusplus
}
#endif

#endif /* __DISP_SW_SPI_DRIVER_H__ */
