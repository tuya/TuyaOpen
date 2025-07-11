/**
 * @file tdd_disp_sw_spi.h
 * @brief Software SPI interface for display controllers
 *
 * This file provides software SPI implementation for display controllers that
 * don't have hardware SPI interfaces available. It includes GPIO configuration
 * structures and functions for initializing and controlling displays using
 * bit-banged SPI communication with proper timing and sequencing.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_SW_SPI_H__
#define __TDD_DISP_SW_SPI_H__

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
typedef struct {
    TUYA_GPIO_NUM_E spi_clk;
    TUYA_GPIO_NUM_E spi_sda;
    TUYA_GPIO_NUM_E spi_csx;
    TUYA_GPIO_NUM_E spi_dc;
    TUYA_GPIO_NUM_E spi_rst;
} TDD_DISP_SW_SPI_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the GPIO pins for the software SPI interface used by the display.
 *
 * This function configures the GPIO settings for the software SPI interface of the LCD.
 * It initializes four key pins: Reset (RST), Clock (CLK), Chip Select (CSX), and Data (SDA).
 * Each pin is configured as a push-pull output with specific initial levels:
 * - CLK are set to HIGH.
 * - CSX is set to HIGH.
 * - SDA is set to LOW.
 *
 * A delay of 200 microseconds is added at the end to ensure proper initialization.
 */
OPERATE_RET tdd_disp_sw_spi_init(TDD_DISP_SW_SPI_CFG_T *cfg);

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
void tdd_disp_sw_spi_lcd_init_seq(const uint8_t *init_seq);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SW_SPI_H__ */
