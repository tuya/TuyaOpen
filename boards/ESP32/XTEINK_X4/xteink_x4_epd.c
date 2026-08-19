/**
 * @file xteink_x4_epd.c
 * @brief Xteink X4 SSD1677 E-Ink display driver ported from OpenX4 community-sdk EInkDisplay.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note This C driver ports the X4 800x480 black/white SSD1677 path from
 *       https://github.com/open-x4-epaper/community-sdk/tree/main/libs/display/EInkDisplay
 */
#include "xteink_x4_epd.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "tal_api.h"
#include "tal_system.h"
#include "tkl_gpio.h"
#include "xteink_x4_spi.h"
#include <string.h>

#define CMD_SOFT_RESET            0x12
#define CMD_BOOSTER_SOFT_START    0x0C
#define CMD_DRIVER_OUTPUT_CONTROL 0x01
#define CMD_BORDER_WAVEFORM       0x3C
#define CMD_TEMP_SENSOR_CONTROL   0x18
#define CMD_DATA_ENTRY_MODE       0x11
#define CMD_SET_RAM_X_RANGE       0x44
#define CMD_SET_RAM_Y_RANGE       0x45
#define CMD_SET_RAM_X_COUNTER     0x4E
#define CMD_SET_RAM_Y_COUNTER     0x4F
#define CMD_WRITE_RAM_BW          0x24
#define CMD_WRITE_RAM_RED         0x26
#define CMD_AUTO_WRITE_BW_RAM     0x46
#define CMD_AUTO_WRITE_RED_RAM    0x47
#define CMD_DISPLAY_UPDATE_CTRL1  0x21
#define CMD_DISPLAY_UPDATE_CTRL2  0x22
#define CMD_MASTER_ACTIVATION     0x20
#define CMD_DEEP_SLEEP            0x10

#define CTRL1_NORMAL     0x00
#define CTRL1_BYPASS_RED 0x40

#define TEMP_SENSOR_INTERNAL   0x80
#define DATA_ENTRY_X_INC_Y_DEC 0x01

#define X4_EPD_WIDTH_BYTES     (X4_EPD_WIDTH / 8U)
#define X4_EPD_BUF_SIZE        (X4_EPD_WIDTH_BYTES * X4_EPD_HEIGHT)
#define X4_EPD_SPI_CHUNK       4096U
#define X4_EPD_BUSY_TIMEOUT_MS 30000U

#define TAG "x4_epd"

#define X4_IDF_SPI_HOST SPI2_HOST

static BOOL_T              s_epd_inited   = FALSE;
static BOOL_T              s_is_screen_on = FALSE;
static BOOL_T              s_prev_valid   = FALSE;
static uint8_t             s_prev_frame[X4_EPD_BUF_SIZE];
static spi_device_handle_t s_epd_spi;

/**
 * @brief Initialize one output GPIO.
 * @param[in] pin GPIO number.
 * @param[in] level initial level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __gpio_output_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = level;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Initialize one input GPIO.
 * @param[in] pin GPIO number.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __gpio_input_init(TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PULLUP;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Initialize SPI and related GPIOs.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __bus_init(void)
{
    OPERATE_RET                   rt     = OPRT_OK;
    esp_err_t                     esp_rt = ESP_OK;
    spi_device_interface_config_t dev_cfg;

    TUYA_CALL_ERR_RETURN(xteink_x4_spi_bus_init(X4_EPD_SPI_CHUNK));

    if (NULL == s_epd_spi) {
        (void)memset(&dev_cfg, 0, sizeof(dev_cfg));
        dev_cfg.clock_speed_hz = (int)X4_EPD_SPI_FREQ_HZ;
        dev_cfg.mode           = 0;
        dev_cfg.spics_io_num   = GPIO_NUM_NC;
        dev_cfg.queue_size     = 1;
        esp_rt                 = spi_bus_add_device(X4_IDF_SPI_HOST, &dev_cfg, &s_epd_spi);
        if (ESP_OK != esp_rt) {
            ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(esp_rt));
            return OPRT_COM_ERROR;
        }
    }

    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_output_init(X4_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(__gpio_input_init(X4_EPD_PIN_BUSY));

    return OPRT_OK;
}

/**
 * @brief Send bytes over the configured SPI bus.
 * @param[in] data data pointer.
 * @param[in] len data length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __spi_send(const uint8_t *data, uint32_t len)
{
    uint32_t  offset = 0;
    esp_err_t esp_rt = ESP_OK;

    if (NULL == data || len == 0U || NULL == s_epd_spi) {
        return OPRT_INVALID_PARM;
    }

    while (offset < len) {
        spi_transaction_t trans;
        uint32_t          chunk = len - offset;
        if (chunk > X4_EPD_SPI_CHUNK) {
            chunk = X4_EPD_SPI_CHUNK;
        }

        (void)memset(&trans, 0, sizeof(trans));
        trans.length    = chunk * 8U;
        trans.tx_buffer = data + offset;
        esp_rt          = spi_device_transmit(s_epd_spi, &trans);
        if (ESP_OK != esp_rt) {
            ESP_LOGE(TAG, "spi_device_transmit failed: %s", esp_err_to_name(esp_rt));
            return OPRT_COM_ERROR;
        }
        offset += chunk;
    }

    return OPRT_OK;
}

/**
 * @brief Send one command byte.
 * @param[in] cmd command byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_cmd(uint8_t cmd)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_DC, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(&cmd, 1));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Send one data byte.
 * @param[in] data data byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_data(uint8_t data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(&data, 1));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Send a data buffer while keeping chip-select asserted.
 * @param[in] data data pointer.
 * @param[in] len data length.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __send_data_buf(const uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_DC, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__spi_send(data, len));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    return OPRT_OK;
}

/**
 * @brief Wait while SSD1677 BUSY is high.
 * @return OPRT_OK on idle, timeout error otherwise.
 */
static OPERATE_RET __wait_busy(void)
{
    OPERATE_RET       rt    = OPRT_OK;
    SYS_TIME_T        start = tal_system_get_millisecond();
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;

    do {
        TUYA_CALL_ERR_RETURN(tkl_gpio_read(X4_EPD_PIN_BUSY, &level));
        if (level == TUYA_GPIO_LEVEL_LOW) {
            return OPRT_OK;
        }
        tal_system_sleep(1);
    } while ((uint32_t)(tal_system_get_millisecond() - start) < X4_EPD_BUSY_TIMEOUT_MS);

    return OPRT_TIMEOUT;
}

/**
 * @brief Hardware reset sequence from OpenX4 EInkDisplay.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __reset(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(20);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_RST, TUYA_GPIO_LEVEL_LOW));
    tal_system_sleep(2);
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_EPD_PIN_RST, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(20);

    return OPRT_OK;
}

/**
 * @brief Configure a display RAM region.
 * @param[in] x region x in pixels.
 * @param[in] y region y in pixels.
 * @param[in] w region width in pixels.
 * @param[in] h region height in pixels.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __set_ram_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    OPERATE_RET rt = OPRT_OK;

    y = (uint16_t)(X4_EPD_HEIGHT - y - h);

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DATA_ENTRY_MODE));
    TUYA_CALL_ERR_RETURN(__send_data(DATA_ENTRY_X_INC_Y_DEC));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_X_RANGE));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((x + w - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((x + w - 1U) >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_Y_RANGE));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(y & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(y >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_X_COUNTER));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)(x >> 8)));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SET_RAM_Y_COUNTER));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((y + h - 1U) >> 8)));

    return OPRT_OK;
}

/**
 * @brief Write a full framebuffer to one SSD1677 RAM plane.
 * @param[in] ram_cmd RAM write command.
 * @param[in] data framebuffer pointer.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __write_ram_buffer(uint8_t ram_cmd, const uint8_t *data)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__send_cmd(ram_cmd));
    TUYA_CALL_ERR_RETURN(__send_data_buf(data, X4_EPD_BUF_SIZE));

    return OPRT_OK;
}

/**
 * @brief Run one SSD1677 refresh sequence.
 * @param[in] full true for full refresh, false for fast differential refresh.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __refresh(bool full)
{
    OPERATE_RET rt           = OPRT_OK;
    uint8_t     display_mode = 0;

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL1));
    TUYA_CALL_ERR_RETURN(__send_data(full ? CTRL1_BYPASS_RED : CTRL1_NORMAL));

    if (!s_is_screen_on) {
        s_is_screen_on = TRUE;
        display_mode |= 0xC0;
    }

    display_mode |= full ? 0x34 : 0x1C;

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL2));
    TUYA_CALL_ERR_RETURN(__send_data(display_mode));
    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_MASTER_ACTIVATION));
    TUYA_CALL_ERR_RETURN(__wait_busy());

    return OPRT_OK;
}

/**
 * @brief Initialize the SSD1677 controller with OpenX4 EInkDisplay values.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __controller_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_SOFT_RESET));
    TUYA_CALL_ERR_RETURN(__wait_busy());

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_TEMP_SENSOR_CONTROL));
    TUYA_CALL_ERR_RETURN(__send_data(TEMP_SENSOR_INTERNAL));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BOOSTER_SOFT_START));
    TUYA_CALL_ERR_RETURN(__send_data(0xAE));
    TUYA_CALL_ERR_RETURN(__send_data(0xC7));
    TUYA_CALL_ERR_RETURN(__send_data(0xC3));
    TUYA_CALL_ERR_RETURN(__send_data(0xC0));
    TUYA_CALL_ERR_RETURN(__send_data(0x40));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DRIVER_OUTPUT_CONTROL));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((X4_EPD_HEIGHT - 1U) & 0xFFU)));
    TUYA_CALL_ERR_RETURN(__send_data((uint8_t)((X4_EPD_HEIGHT - 1U) >> 8)));
    TUYA_CALL_ERR_RETURN(__send_data(0x02));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_BORDER_WAVEFORM));
    TUYA_CALL_ERR_RETURN(__send_data(0x01));

    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4_EPD_WIDTH, X4_EPD_HEIGHT));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_AUTO_WRITE_BW_RAM));
    TUYA_CALL_ERR_RETURN(__send_data(0xF7));
    TUYA_CALL_ERR_RETURN(__wait_busy());

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_AUTO_WRITE_RED_RAM));
    TUYA_CALL_ERR_RETURN(__send_data(0xF7));
    TUYA_CALL_ERR_RETURN(__wait_busy());

    return OPRT_OK;
}

OPERATE_RET xteink_x4_epd_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_epd_inited) {
        return OPRT_OK;
    }

    (void)memset(s_prev_frame, 0xFF, sizeof(s_prev_frame));
    s_prev_valid   = FALSE;
    s_is_screen_on = FALSE;

    TUYA_CALL_ERR_RETURN(__bus_init());
    TUYA_CALL_ERR_RETURN(__reset());
    TUYA_CALL_ERR_RETURN(__controller_init());

    s_epd_inited = TRUE;
    return OPRT_OK;
}

OPERATE_RET xteink_x4_epd_clear(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     white_line[X4_EPD_WIDTH_BYTES];
    uint16_t    y;

    if (!s_epd_inited) {
        return OPRT_COM_ERROR;
    }

    (void)memset(white_line, 0xFF, sizeof(white_line));
    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4_EPD_WIDTH, X4_EPD_HEIGHT));

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_WRITE_RAM_BW));
    for (y = 0; y < X4_EPD_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(white_line, sizeof(white_line)));
    }

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_WRITE_RAM_RED));
    for (y = 0; y < X4_EPD_HEIGHT; y++) {
        TUYA_CALL_ERR_RETURN(__send_data_buf(white_line, sizeof(white_line)));
    }

    TUYA_CALL_ERR_RETURN(__refresh(true));
    (void)memset(s_prev_frame, 0xFF, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_epd_display(uint8_t *image)
{
    OPERATE_RET rt           = OPRT_OK;
    bool        full_refresh = false;

    if (!s_epd_inited || NULL == image) {
        return OPRT_COM_ERROR;
    }

    full_refresh = (s_prev_valid == FALSE);
    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4_EPD_WIDTH, X4_EPD_HEIGHT));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_BW, image));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_RED, full_refresh ? image : s_prev_frame));
    TUYA_CALL_ERR_RETURN(__refresh(full_refresh));

    (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_epd_display_full_refresh(uint8_t *image)
{
    OPERATE_RET rt = OPRT_OK;

    if (!s_epd_inited || NULL == image) {
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(__set_ram_area(0, 0, X4_EPD_WIDTH, X4_EPD_HEIGHT));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_BW, image));
    TUYA_CALL_ERR_RETURN(__write_ram_buffer(CMD_WRITE_RAM_RED, image));
    TUYA_CALL_ERR_RETURN(__refresh(true));

    (void)memcpy(s_prev_frame, image, sizeof(s_prev_frame));
    s_prev_valid = TRUE;

    return OPRT_OK;
}

OPERATE_RET xteink_x4_epd_sleep(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!s_epd_inited) {
        return OPRT_COM_ERROR;
    }

    if (s_is_screen_on) {
        TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL1));
        TUYA_CALL_ERR_RETURN(__send_data(CTRL1_BYPASS_RED));
        TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DISPLAY_UPDATE_CTRL2));
        TUYA_CALL_ERR_RETURN(__send_data(0x03));
        TUYA_CALL_ERR_RETURN(__send_cmd(CMD_MASTER_ACTIVATION));
        TUYA_CALL_ERR_RETURN(__wait_busy());
        s_is_screen_on = FALSE;
    }

    TUYA_CALL_ERR_RETURN(__send_cmd(CMD_DEEP_SLEEP));
    TUYA_CALL_ERR_RETURN(__send_data(0x01));
    s_epd_inited = FALSE;

    return OPRT_OK;
}
