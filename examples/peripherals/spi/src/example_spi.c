/**
 * @file example_spi.c
 * @brief SPI driver example for SDK with ST7789 LCD display support.
 *
 * This file provides an example implementation of an SPI (Serial Peripheral Interface) driver using the Tuya SDK.
 * It demonstrates the configuration and usage of SPI communication to interact with peripheral devices such as sensors,
 * memory chips, and other microcontrollers. The example covers initializing the SPI bus, configuring SPI parameters
 * (mode, frequency, data bits, bit order), sending data to a peripheral device, and deinitializing the SPI bus after
 * communication is complete.
 *
 * Additionally, this example includes ST7789 LCD display initialization and basic drawing capabilities
 * to verify SPI communication through the tkl_spi abstraction layer.
 *
 * The SPI driver example aims to help developers understand how to use SPI communication in their Tuya IoT projects for
 * applications requiring high-speed serial data transfer. It includes detailed examples of setting up SPI
 * configurations, handling data transmission, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental SPI
 * operations critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"

#include "xl9555.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#ifndef EXAMPLE_ENABLE_LCD
#define EXAMPLE_ENABLE_LCD 1
#endif

#ifndef EXAMPLE_LCD_WIDTH
#define EXAMPLE_LCD_WIDTH 240
#endif

#ifndef EXAMPLE_LCD_HEIGHT
#define EXAMPLE_LCD_HEIGHT 320
#endif

#ifndef EXAMPLE_LCD_MOSI_PIN
#define EXAMPLE_LCD_MOSI_PIN 11
#endif

#ifndef EXAMPLE_LCD_SCLK_PIN
#define EXAMPLE_LCD_SCLK_PIN 12
#endif

#ifndef EXAMPLE_LCD_MISO_PIN
#define EXAMPLE_LCD_MISO_PIN -1
#endif

#ifndef EXAMPLE_LCD_CS_PIN
#define EXAMPLE_LCD_CS_PIN 21
#endif

#ifndef EXAMPLE_LCD_DC_PIN
#define EXAMPLE_LCD_DC_PIN 40
#endif

#ifndef EXAMPLE_LCD_RST_PIN
#define EXAMPLE_LCD_RST_PIN -1
#endif

#ifndef EXAMPLE_LCD_BL_PIN
#define EXAMPLE_LCD_BL_PIN -1
#endif

#ifndef EXAMPLE_LCD_IO_EXPANDER
#define EXAMPLE_LCD_IO_EXPANDER 1
#endif

#if EXAMPLE_LCD_IO_EXPANDER
#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN 42
#endif
#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN 41
#endif
#ifndef EXAMPLE_LCD_IO_BL_PIN
#define EXAMPLE_LCD_IO_BL_PIN 8
#endif
#ifndef EXAMPLE_LCD_IO_RST_PIN
#define EXAMPLE_LCD_IO_RST_PIN 10
#endif
#ifndef EXAMPLE_LCD_IO_PWR_PIN
#define EXAMPLE_LCD_IO_PWR_PIN 11
#endif
#define EX_IO_LCD_BL   (0x0001 << EXAMPLE_LCD_IO_BL_PIN)
#define EX_IO_SLCD_RST (0x0001 << EXAMPLE_LCD_IO_RST_PIN)
#define EX_IO_SLCD_PWR (0x0001 << EXAMPLE_LCD_IO_PWR_PIN)
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define***********************
***********************************************************/

/* Forward declarations */
static OPERATE_RET __lcd_write_cmd(uint8_t cmd);
static OPERATE_RET __lcd_write_data(uint8_t *data, uint32_t len);
static OPERATE_RET __lcd_send_cmd_data(uint8_t cmd, uint8_t *data, uint32_t len);

/**
 * @brief Initialize ST7789 LCD display using tkl_spi interface
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
static OPERATE_RET __lcd_init_gpio(void)
{
    OPERATE_RET rt;

    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };

    rt = tkl_gpio_init(EXAMPLE_LCD_CS_PIN, &gpio_cfg);
    if (rt != OPRT_OK) return rt;

    rt = tkl_gpio_init(EXAMPLE_LCD_DC_PIN, &gpio_cfg);
    if (rt != OPRT_OK) return rt;

    if (EXAMPLE_LCD_RST_PIN >= 0) {
        rt = tkl_gpio_init(EXAMPLE_LCD_RST_PIN, &gpio_cfg);
        if (rt != OPRT_OK) return rt;
    }

    if (EXAMPLE_LCD_BL_PIN >= 0) {
        rt = tkl_gpio_init(EXAMPLE_LCD_BL_PIN, &gpio_cfg);
        if (rt != OPRT_OK) return rt;
    }

    return OPRT_OK;
}

#if EXAMPLE_LCD_IO_EXPANDER
static OPERATE_RET __xl9555_lcd_init(void)
{
    if (xl9555_init(&(XL9555_HW_CFG_T){
            .i2c_port = 0,
            .scl_io   = EXAMPLE_I2C_SCL_PIN,
            .sda_io   = EXAMPLE_I2C_SDA_PIN,
            .dev_addr = 0x20}) != 0) {
        PR_ERR("xl9555_init failed");
        return OPRT_COM_ERROR;
    }

    uint32_t out_mask = EX_IO_LCD_BL | EX_IO_SLCD_RST | EX_IO_SLCD_PWR;
    if (xl9555_set_dir(out_mask, 0) != 0) {
        PR_ERR("xl9555_set_dir failed");
        return OPRT_COM_ERROR;
    }

    xl9555_set_level(EX_IO_SLCD_PWR, 1);
    tal_system_sleep(10);

    xl9555_set_level(EX_IO_SLCD_RST, 0);
    tal_system_sleep(10);
    xl9555_set_level(EX_IO_SLCD_RST, 1);
    tal_system_sleep(120);

    xl9555_set_level(EX_IO_LCD_BL, 1);

    PR_NOTICE("XL9555 LCD init OK (PWR=%d RST=%d BL=%d)",
              EXAMPLE_LCD_IO_PWR_PIN, EXAMPLE_LCD_IO_RST_PIN, EXAMPLE_LCD_IO_BL_PIN);
    return OPRT_OK;
}
#endif

static OPERATE_RET __lcd_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if EXAMPLE_ENABLE_LCD
#if EXAMPLE_LCD_IO_EXPANDER
    TUYA_CALL_ERR_RETURN(__xl9555_lcd_init());
#endif

    /* Configure SPI pins via pinmux (required by tkl_spi driver) */
    tkl_io_pinmux_config(EXAMPLE_LCD_MOSI_PIN, TUYA_SPI0_MOSI);
    tkl_io_pinmux_config(EXAMPLE_LCD_SCLK_PIN, TUYA_SPI0_CLK);
    if (EXAMPLE_LCD_MISO_PIN >= 0) {
        tkl_io_pinmux_config(EXAMPLE_LCD_MISO_PIN, TUYA_SPI0_MISO);
    }

    /* Initialize GPIO pins for LCD control */
    TUYA_CALL_ERR_RETURN(__lcd_init_gpio());

    /* Hardware reset */
    if (EXAMPLE_LCD_RST_PIN >= 0) {
        tkl_gpio_write(EXAMPLE_LCD_RST_PIN, TUYA_GPIO_LEVEL_LOW);
        tal_system_sleep(10);
        tkl_gpio_write(EXAMPLE_LCD_RST_PIN, TUYA_GPIO_LEVEL_HIGH);
        tal_system_sleep(120);
    }

    /* Backlight on */
    if (EXAMPLE_LCD_BL_PIN >= 0) {
        tkl_gpio_write(EXAMPLE_LCD_BL_PIN, TUYA_GPIO_LEVEL_HIGH);
    }

    /* Configure SPI for LCD */
    TUYA_SPI_BASE_CFG_T lcd_spi_cfg = {
        .mode = TUYA_SPI_MODE0,
        .freq_hz = 40000000,
        .databits = TUYA_SPI_DATA_BIT8,
        .bitorder = TUYA_SPI_ORDER_MSB2LSB,
        .role = TUYA_SPI_ROLE_MASTER,
        .type = TUYA_SPI_AUTO_TYPE,
        .spi_dma_flags = 1,
    };

    TUYA_CALL_ERR_RETURN(tkl_spi_init(EXAMPLE_SPI_PORT, &lcd_spi_cfg));

    /* ST7789 init sequence (matching esp_lcd_panel_st7789 reference) */
    tal_system_sleep(10);

    /* Software reset */
    TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x01, NULL, 0));
    tal_system_sleep(150);

    /* Sleep out */
    TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x11, NULL, 0));
    tal_system_sleep(120);

    /* Memory data access control: RGB order, no mirror */
    {
        uint8_t val = 0x00;
        TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x36, &val, 1));
    }

    /* Color mode: 16-bit RGB565 (0x55 = MCU 16-bit + RGB 16-bit) */
    {
        uint8_t val = 0x55;
        TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x3A, &val, 1));
    }

    /* RAM control: big endian (0x00, 0xF0) */
    {
        uint8_t data[] = {0x00, 0xF0};
        TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0xB0, data, sizeof(data)));
    }

    /* Display on */
    TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x29, NULL, 0));
    tal_system_sleep(100);

    PR_NOTICE("LCD init OK (MOSI=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d)",
              EXAMPLE_LCD_MOSI_PIN, EXAMPLE_LCD_SCLK_PIN, EXAMPLE_LCD_CS_PIN,
              EXAMPLE_LCD_DC_PIN, EXAMPLE_LCD_RST_PIN, EXAMPLE_LCD_BL_PIN);
#endif

    return rt;
}

/**
 * @brief Write command byte (CS must already be LOW)
 *
 * @param[in] cmd Command byte
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __lcd_write_cmd(uint8_t cmd)
{
#if EXAMPLE_ENABLE_LCD
    OPERATE_RET rt;
    rt = tkl_gpio_write(EXAMPLE_LCD_DC_PIN, TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) return rt;
    return tkl_spi_send(EXAMPLE_SPI_PORT, &cmd, 1);
#else
    return OPRT_OK;
#endif
}

/**
 * @brief Write data bytes (CS must already be LOW)
 *
 * @param[in] data Data buffer
 * @param[in] len Data length
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __lcd_write_data(uint8_t *data, uint32_t len)
{
#if EXAMPLE_ENABLE_LCD
    OPERATE_RET rt;
    rt = tkl_gpio_write(EXAMPLE_LCD_DC_PIN, TUYA_GPIO_LEVEL_HIGH);
    if (rt != OPRT_OK) return rt;
    return tkl_spi_send(EXAMPLE_SPI_PORT, data, len);
#else
    return OPRT_OK;
#endif
}

/**
 * @brief Send command + data with CS held LOW for the full transaction
 *
 * @param[in] cmd  Command byte
 * @param[in] data Data buffer
 * @param[in] len  Data length
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __lcd_send_cmd_data(uint8_t cmd, uint8_t *data, uint32_t len)
{
#if EXAMPLE_ENABLE_LCD
    OPERATE_RET rt;
    rt = tkl_gpio_write(EXAMPLE_LCD_CS_PIN, TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) return rt;
    rt = __lcd_write_cmd(cmd);
    if (rt != OPRT_OK) return rt;
    if (data != NULL && len > 0) {
        rt = __lcd_write_data(data, len);
        if (rt != OPRT_OK) return rt;
    }
    return tkl_gpio_write(EXAMPLE_LCD_CS_PIN, TUYA_GPIO_LEVEL_HIGH);
#else
    return OPRT_OK;
#endif
}

/**
 * @brief Fill LCD with color
 *
 * @param[in] color 16-bit color value (RGB565)
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __lcd_fill_color(uint16_t color)
{
#if EXAMPLE_ENABLE_LCD
    OPERATE_RET rt;
    uint8_t cmd_data[4];
    uint32_t total_pixels = EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT;
    uint32_t pixel_bytes = total_pixels * 2;

    /* Set column address */
    cmd_data[0] = 0x00;
    cmd_data[1] = 0x00;
    cmd_data[2] = (EXAMPLE_LCD_WIDTH - 1) >> 8;
    cmd_data[3] = (EXAMPLE_LCD_WIDTH - 1) & 0xFF;
    TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x2A, cmd_data, 4));

    /* Set row address */
    cmd_data[0] = 0x00;
    cmd_data[1] = 0x00;
    cmd_data[2] = (EXAMPLE_LCD_HEIGHT - 1) >> 8;
    cmd_data[3] = (EXAMPLE_LCD_HEIGHT - 1) & 0xFF;
    TUYA_CALL_ERR_RETURN(__lcd_send_cmd_data(0x2B, cmd_data, 4));

    /* RAMWR — send pixel data in DMA-sized chunks */
    rt = tkl_gpio_write(EXAMPLE_LCD_CS_PIN, TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) return rt;
    rt = __lcd_write_cmd(0x2C);
    if (rt != OPRT_OK) return rt;

    uint32_t dma_max = tkl_spi_get_max_dma_data_length();
    /* Round chunk size down to even byte for RGB565 alignment */
    uint32_t chunk = (dma_max / 2) * 2;
    if (chunk == 0) chunk = 2;

    uint8_t *chunk_buf = tal_malloc(chunk);
    if (NULL == chunk_buf) { tkl_gpio_write(EXAMPLE_LCD_CS_PIN, TUYA_GPIO_LEVEL_HIGH); return OPRT_MALLOC_FAILED; }
    for (uint32_t i = 0; i < chunk / 2; i++) {
        chunk_buf[i * 2]     = (color >> 8) & 0xFF;
        chunk_buf[i * 2 + 1] = color & 0xFF;
    }

    uint32_t remaining = pixel_bytes;
    while (remaining > 0) {
        uint32_t send_len = (remaining > chunk) ? chunk : remaining;
        rt = __lcd_write_data(chunk_buf, send_len);
        if (rt != OPRT_OK) { tal_free(chunk_buf); return rt; }
        remaining -= send_len;
    }

    tal_free(chunk_buf);
    rt = tkl_gpio_write(EXAMPLE_LCD_CS_PIN, TUYA_GPIO_LEVEL_HIGH);
    if (rt != OPRT_OK) return rt;

    PR_NOTICE("LCD filled with color 0x%04X", color);
    return OPRT_OK;
#else
    return OPRT_OK;
#endif
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t send_buff[] = {"Hello Tuya"};

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /* Initialize LCD */
    TUYA_CALL_ERR_GOTO(__lcd_init(), __EXIT);

    /* Fill LCD with red color */
    __lcd_fill_color(0xF800);  /* Red color in RGB565 */
    tal_system_sleep(1000);

    /* Fill LCD with green color */
    __lcd_fill_color(0x07E0);  /* Green color in RGB565 */
    tal_system_sleep(1000);

    /* Fill LCD with blue color */
    __lcd_fill_color(0x001F);  /* Blue color in RGB565 */
    tal_system_sleep(1000);

    /* Basic SPI send test (bus already initialized by LCD init) */
    while(1) {
        TUYA_CALL_ERR_LOG(tkl_spi_send(EXAMPLE_SPI_PORT, send_buff, sizeof(send_buff)));
        PR_NOTICE("spi send \"%s\" finish", send_buff);

        tal_system_sleep(500);
    }

__EXIT:
    PR_ERR("example spi error code: %d", rt);
    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif