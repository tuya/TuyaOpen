/**
 * @file tdd_led_esp_ws1280_spi.c
 * @brief ESP32 WS1280 LED driver implementation (SPI backend).
 *
 * Same WS1280 timing as the RMT version, but encodes each 1.25us WS1280 bit
 * as 10 SPI bits @ 8MHz, for chips without the RMT peripheral (esp32c2).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tkl_memory.h"
#include "tdd_led_esp_ws1280_spi.h"
#include "tdl_led_driver.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "WS1280_SPI_DRIVER"

#define WS1280_SPI_HOST        SPI2_HOST
#define WS1280_SPI_CLK_HZ      8000000  // 10 SPI bits per 1.25us WS1280 bit
#define WS1280_SPI_BITS_PER_WS 10       // 0: 0b1110000000, 1: 0b1111110000
#define WS1280_COLOR_BYTES     3
#define WS1280_RESET_BYTES     64       // >50us low level latch (64B = 512bit = 64us @8MHz)
#define WS1280_TIMEOUT_MS      100

#define WS1280_CODE_0 0x380 // 0b1110000000
#define WS1280_CODE_1 0x7C0 // 0b1111110000
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_LED_WS1280_SPI_CFG_T cfg;
    uint8_t                 *frame_buf; // SPI-encoded bytes, reset appended
    uint16_t                 frame_len;
    bool                     bus_inited;
} TDD_LED_WS1280_SPI_INFO_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static spi_device_handle_t sg_spi_dev = NULL;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Encode one WS1280 bit into 10 SPI bits and shift them into the frame buffer.
 * @param buf Frame byte buffer.
 * @param bit_pos WS1280 bit index (0 = MSB of the first LED).
 * @param bit Input bit value (0 or 1).
 * @return
 */
static void __ws1280_encode_bit(uint8_t *buf, uint16_t bit_pos, uint8_t bit)
{
    uint16_t spi_bit0 = bit_pos * WS1280_SPI_BITS_PER_WS;
    uint16_t code     = bit ? WS1280_CODE_1 : WS1280_CODE_0;

    for (int i = 0; i < WS1280_SPI_BITS_PER_WS; i++) {
        if (code & (1 << (WS1280_SPI_BITS_PER_WS - 1 - i))) {
            buf[(spi_bit0 + i) / 8] |= 1 << (7 - ((spi_bit0 + i) % 8));
        }
    }
}

/**
 * @brief Encode RGB data into the SPI frame buffer (MSB first, 24 bits per LED).
 * @param info Driver info holding the frame buffer.
 * @param color LED color value.
 * @return
 */
static void __ws1280_encode_data(TDD_LED_WS1280_SPI_INFO_T *info, uint32_t color)
{
    memset(info->frame_buf, 0, info->frame_len);

    for (int i = 0; i < info->cfg.led_count; i++) {
        for (int bit = 23; bit >= 0; bit--) {
            __ws1280_encode_bit(info->frame_buf, i * 24 + (23 - bit), (color >> bit) & 0x01);
        }
    }
    // frame_buf tail (WS1280_RESET_BYTES) stays zero = latch reset
}

/**
 * @brief Initialize the SPI bus and device.
 * @param gpio_num GPIO used for WS1280 data output (SPI MOSI via GPIO matrix).
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t __ws1280_init(gpio_num_t gpio_num)
{
    esp_err_t err = spi_bus_initialize(WS1280_SPI_HOST,
                                       &(spi_bus_config_t){
                                           .mosi_io_num = gpio_num,
                                           .miso_io_num = -1,
                                           .sclk_io_num = -1,
                                           .quadwp_io_num = -1,
                                           .quadhd_io_num = -1,
                                       },
                                       SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        PR_ERR("spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = WS1280_SPI_CLK_HZ,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 1,
    };
    err = spi_bus_add_device(WS1280_SPI_HOST, &dev_cfg, &sg_spi_dev);
    if (err != ESP_OK) {
        PR_ERR("spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    PR_NOTICE("WS1280 SPI driver initialized (GPIO:%d)", gpio_num);

    return ESP_OK;
}

/**
 * @brief Transmit the encoded frame.
 * @param info Driver info holding the frame buffer.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t __ws1280_show(TDD_LED_WS1280_SPI_INFO_T *info)
{
    esp_err_t err = spi_device_polling_transmit(sg_spi_dev, &(spi_transaction_t){
        .tx_buffer = info->frame_buf,
        .length    = info->frame_len * 8,
    });
    if (err != ESP_OK) {
        PR_ERR("spi_device_polling_transmit failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief Deinitialize the SPI device and bus.
 * @param
 * @return
 */
static void __ws1280_deinit(void)
{
    if (sg_spi_dev) {
        spi_bus_remove_device(sg_spi_dev);
        sg_spi_dev = NULL;
    }
    spi_bus_free(WS1280_SPI_HOST);
}

/**
 * @brief Open the WS1280 LED device.
 * @param dev LED device handle.
 * @param
 * @return Operation result code.
 */
static OPERATE_RET __tdd_led_ws1280_open(TDD_LED_HANDLE_T dev)
{
    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    TDD_LED_WS1280_SPI_INFO_T *info = (TDD_LED_WS1280_SPI_INFO_T *)dev;

    if (info->bus_inited) {
        return OPRT_OK;
    }

    esp_err_t err = __ws1280_init((gpio_num_t)info->cfg.gpio);
    if (err != ESP_OK) {
        return OPRT_COM_ERROR;
    }
    info->bus_inited = true;

    return OPRT_OK;
}

/**
 * @brief Set WS1280 LED on/off state.
 * @param dev LED device handle.
 * @param is_on True to turn on, false to turn off.
 * @return Operation result code.
 */
static OPERATE_RET __tdd_led_ws1280_set(TDD_LED_HANDLE_T dev, bool is_on)
{
    PR_NOTICE("Setting WS1280 LEDs %s", is_on ? "ON" : "OFF");

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    TDD_LED_WS1280_SPI_INFO_T *info = (TDD_LED_WS1280_SPI_INFO_T *)dev;

    if (!info->bus_inited) {
        return OPRT_COM_ERROR;
    }

    __ws1280_encode_data(info, is_on ? info->cfg.color : 0);

    if (__ws1280_show(info) != ESP_OK) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/**
 * @brief Close the WS1280 LED device.
 * @param dev LED device handle.
 * @param
 * @return Operation result code.
 */
static OPERATE_RET __tdd_led_ws1280_close(TDD_LED_HANDLE_T dev)
{
    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    TDD_LED_WS1280_SPI_INFO_T *info = (TDD_LED_WS1280_SPI_INFO_T *)dev;

    if (info->bus_inited) {
        __ws1280_deinit();
        info->bus_inited = false;
    }

    return OPRT_OK;
}

/**
 * @brief Register a WS1280 LED device (SPI backend).
 * @param dev_name Device name.
 * @param cfg WS1280 configuration.
 * @return Operation result code.
 */
OPERATE_RET tdd_led_esp_ws1280_spi_register(char *dev_name, TDD_LED_WS1280_SPI_CFG_T *cfg)
{
    TDD_LED_WS1280_SPI_INFO_T *info      = NULL;
    uint32_t                  frame_len = 0;

    if (dev_name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (cfg->led_count > TDD_LED_WS1280_SPI_COUNT_MAX) {
        PR_ERR("LED count exceeds maximum (%d)", TDD_LED_WS1280_SPI_COUNT_MAX);
        return OPRT_INVALID_PARM;
    }

    frame_len = cfg->led_count * WS1280_COLOR_BYTES * WS1280_SPI_BITS_PER_WS / 8 + WS1280_RESET_BYTES;

    info = (TDD_LED_WS1280_SPI_INFO_T *)tkl_system_malloc(sizeof(TDD_LED_WS1280_SPI_INFO_T) + frame_len);
    if (info == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(info, 0, sizeof(TDD_LED_WS1280_SPI_INFO_T) + frame_len);
    memcpy(&info->cfg, cfg, sizeof(TDD_LED_WS1280_SPI_CFG_T));

    info->frame_buf = (uint8_t *)(info + 1);
    info->frame_len = frame_len;

    TDD_LED_INTFS_T intfs = {
        .led_open  = __tdd_led_ws1280_open,
        .led_set   = __tdd_led_ws1280_set,
        .led_close = __tdd_led_ws1280_close,
    };

    OPERATE_RET rt = tdl_led_driver_register(dev_name, (TDD_LED_HANDLE_T)info, &intfs);
    if (rt != OPRT_OK) {
        tkl_system_free(info);
        return rt;
    }

    return OPRT_OK;
}
