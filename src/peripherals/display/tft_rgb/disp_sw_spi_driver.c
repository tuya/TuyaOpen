/**
 * @file disp_sw_spi_driver.c
 * @brief Software SPI Driver for LCD Display
 * 
 * This file contains the implementation of a software SPI driver for controlling an LCD display.
 * It includes functions to initialize the GPIO pins, send commands and data, and perform the
 * initialization sequence for the LCD display.
 * 
 * @details The file defines the following main components:
 * - Macros for configuration and delays.
 * - Functions to initialize the GPIO pins for the software SPI interface.
 * - Functions to send commands and data to the LCD display.
 * - A function to initialize the LCD display using a predefined sequence of commands.
 * 
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"

#include "tkl_gpio.h"
#include "tkl_system.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TUYA_LCD_SPI_DELAY             2

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static void __spi_send_data(uint8_t data)
{
	uint8_t n;

	//in while loop, to avoid disable IRQ too much time, release it if finish one byte.
    TKL_ENTER_CRITICAL();

	for (n = 0; n < 8; n++) {
		if (data & 0x80) {
			tkl_gpio_write(LCD_RGB_SW_SPI_SDA_PIN, TUYA_GPIO_LEVEL_HIGH);
		}else {
			tkl_gpio_write(LCD_RGB_SW_SPI_SDA_PIN, TUYA_GPIO_LEVEL_LOW);
		}

		data <<= 1;

		tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_LOW);
		tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_HIGH);
	}

	TKL_EXIT_CRITICAL();

    return;
}
/**
 * @brief Initializes the GPIO pins for the software SPI interface used by the display.
 * 
 * This function configures the GPIO settings for the software SPI interface of the LCD.
 * It initializes four key pins: Reset (RST), Clock (CLK), Chip Select (CSX), and Data (SDA).
 * Each pin is configured as a push-pull output with specific initial levels:
 * - RST and CLK are set to HIGH.
 * - CSX is set to HIGH.
 * - SDA is set to LOW.
 * 
 * A delay of 200 microseconds is added at the end to ensure proper initialization.
 */
void disp_sw_spi_init(void)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg;


    gpio_cfg.mode   = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(LCD_RGB_SW_SPI_RST_PIN, &gpio_cfg);

    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(LCD_RGB_SW_SPI_CLK_PIN, &gpio_cfg);

    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(LCD_RGB_SW_SPI_CSX_PIN, &gpio_cfg);

    gpio_cfg.level  = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(LCD_RGB_SW_SPI_SDA_PIN, &gpio_cfg);

	tkl_system_sleep(1);

    return;
}

void disp_sw_spi_write_cmd(uint8_t cmd)
{
	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_LOW);
	tkl_gpio_write(LCD_RGB_SW_SPI_SDA_PIN, TUYA_GPIO_LEVEL_LOW);

	tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_LOW);
	tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_HIGH);

	__spi_send_data(cmd);

	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_HIGH);

    return;
}

void disp_sw_spi_write_data(uint8_t data)
{
	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_LOW);
	tkl_gpio_write(LCD_RGB_SW_SPI_SDA_PIN, TUYA_GPIO_LEVEL_HIGH);

	tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_LOW);
	tkl_gpio_write(LCD_RGB_SW_SPI_CLK_PIN, TUYA_GPIO_LEVEL_HIGH);

	__spi_send_data(data);

	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_HIGH);

    return;
}

void disp_sw_spi_write_hf_word_data(uint32_t data)
{
	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_LOW);

	__spi_send_data(0x40);
	__spi_send_data(data);

	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_HIGH);

    return;
}

void disp_sw_spi_write_hf_word_cmd(uint32_t cmd)
{
	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_LOW);

	__spi_send_data(0x20);
	__spi_send_data(cmd >> 8); //high 8bit

	__spi_send_data(0x00);	  //low 8bit
	__spi_send_data(cmd);

	tkl_gpio_write(LCD_RGB_SW_SPI_CSX_PIN, TUYA_GPIO_LEVEL_HIGH);

    return;
}

static void disp_sw_spi_lcd_write_cmd(uint8_t *cmd, uint8_t data_count)
{
	uint32_t i = 0;

    if(NULL == cmd) {;
        return;
    }

    // send cmd
    disp_sw_spi_write_cmd(*cmd);

    // send lcd_data
    for(i=0; i<data_count; i++) {
		disp_sw_spi_write_data(cmd[1+i]);
	}
}

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
void disp_sw_spi_lcd_init_seq(const uint8_t *init_seq)
{
	disp_sw_spi_init();

	const uint8_t *cmd = init_seq;
    while (*cmd) {
        disp_sw_spi_lcd_write_cmd((uint8_t *)(cmd + 2), *cmd - 1);
        tal_system_sleep(*(cmd + 1));
        cmd += *cmd + 2;
    }

}