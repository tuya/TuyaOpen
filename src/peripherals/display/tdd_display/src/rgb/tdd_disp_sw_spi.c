/**
 * @file tdd_disp_sw_spi.c
 * @brief Software SPI implementation for display controllers
 *
 * This file contains the implementation of software SPI communication for LCD displays.
 * It provides GPIO-based bit-banged SPI functionality for display controllers that require
 * software SPI interface. The implementation includes initialization sequences, command
 * and data transmission functions with proper timing and GPIO control.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_system.h"

#include "tdd_disp_sw_spi.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define TUYA_LCD_SPI_DELAY 2

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_DISP_SW_SPI_CFG_T sg_sw_spi_cfg;
static bool sg_is_sw_spi_init = false;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __spi_send_data(uint8_t data)
{
    uint8_t n;

    // in while loop, to avoid disable IRQ too much time, release it if finish one byte.
    TKL_ENTER_CRITICAL();

    for (n = 0; n < 8; n++) {
        if (data & 0x80) {
            tkl_gpio_write(sg_sw_spi_cfg.spi_sda, TUYA_GPIO_LEVEL_HIGH);
        } else {
            tkl_gpio_write(sg_sw_spi_cfg.spi_sda, TUYA_GPIO_LEVEL_LOW);
        }

        data <<= 1;

        tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_LOW);
        tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_HIGH);
    }

    TKL_EXIT_CRITICAL();

    return;
}

void disp_sw_spi_write_cmd(uint8_t cmd)
{
    tkl_gpio_write(sg_sw_spi_cfg.spi_csx, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(sg_sw_spi_cfg.spi_sda, TUYA_GPIO_LEVEL_LOW);
    if (sg_sw_spi_cfg.spi_dc < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(sg_sw_spi_cfg.spi_dc, TUYA_GPIO_LEVEL_LOW);
    }

    tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_HIGH);

    __spi_send_data(cmd);

    tkl_gpio_write(sg_sw_spi_cfg.spi_csx, TUYA_GPIO_LEVEL_HIGH);

    return;
}

void disp_sw_spi_write_data(uint8_t data)
{
    tkl_gpio_write(sg_sw_spi_cfg.spi_csx, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(sg_sw_spi_cfg.spi_sda, TUYA_GPIO_LEVEL_HIGH);
    if (sg_sw_spi_cfg.spi_dc < TUYA_GPIO_NUM_MAX) {
        tkl_gpio_write(sg_sw_spi_cfg.spi_dc, TUYA_GPIO_LEVEL_HIGH);
    }

    tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(sg_sw_spi_cfg.spi_clk, TUYA_GPIO_LEVEL_HIGH);

    __spi_send_data(data);

    tkl_gpio_write(sg_sw_spi_cfg.spi_csx, TUYA_GPIO_LEVEL_HIGH);

    return;
}

static void disp_sw_spi_lcd_write_cmd(uint8_t *cmd, uint8_t data_count)
{
    uint32_t i = 0;

    if (NULL == cmd) {
        return;
    }

    // send cmd
    disp_sw_spi_write_cmd(*cmd);

    // send lcd_data
    for (i = 0; i < data_count; i++) {
        disp_sw_spi_write_data(cmd[1 + i]);
    }
}

static void __tdd_disp_sw_spi_reset(TUYA_GPIO_NUM_E rst_pin)
{
    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tkl_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tkl_system_sleep(100);
}

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
OPERATE_RET tdd_disp_sw_spi_init(TDD_DISP_SW_SPI_CFG_T *cfg)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    if (NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_sw_spi_cfg, cfg, sizeof(TDD_DISP_SW_SPI_CFG_T));

    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(sg_sw_spi_cfg.spi_clk, &gpio_cfg);

    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(sg_sw_spi_cfg.spi_csx, &gpio_cfg);

    gpio_cfg.level = TUYA_GPIO_LEVEL_LOW;
    tkl_gpio_init(sg_sw_spi_cfg.spi_sda, &gpio_cfg);

    if (sg_sw_spi_cfg.spi_dc < TUYA_GPIO_NUM_MAX) {
        gpio_cfg.level = TUYA_GPIO_LEVEL_LOW;
        tkl_gpio_init(sg_sw_spi_cfg.spi_dc, &gpio_cfg);
    }

    if (sg_sw_spi_cfg.spi_rst < TUYA_GPIO_NUM_MAX) {
        gpio_cfg.level = TUYA_GPIO_LEVEL_LOW;
        tkl_gpio_init(sg_sw_spi_cfg.spi_rst, &gpio_cfg);
    }

    tkl_system_sleep(1);

    sg_is_sw_spi_init = true;

    return OPRT_OK;
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
void tdd_disp_sw_spi_lcd_init_seq(const uint8_t *init_seq)
{
    uint8_t *cmd = (uint8_t *)init_seq;

    if (false == sg_is_sw_spi_init) {
        PR_ERR("Please call tdd_disp_sw_spi_init first.");
        return;
    }

    if (sg_sw_spi_cfg.spi_rst < TUYA_GPIO_NUM_MAX) {
        __tdd_disp_sw_spi_reset(sg_sw_spi_cfg.spi_rst);
    }

    while (*cmd) {
        disp_sw_spi_lcd_write_cmd((uint8_t *)(cmd + 2), *cmd - 1);
        tal_system_sleep(*(cmd + 1));
        cmd += *cmd + 2;
    }
}