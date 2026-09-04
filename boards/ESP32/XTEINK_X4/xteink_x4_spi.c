/**
 * @file xteink_x4_spi.c
 * @brief Shared SPI bus helper for Xteink X4 peripherals.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "xteink_x4_spi.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"

#include <string.h>

#define TAG "x4_spi"

#define X4_IDF_SPI_HOST SPI2_HOST
#define X4_IDF_SPI_DMA  SPI_DMA_CH_AUTO

static BOOL_T   s_bus_inited;
static uint32_t s_max_transfer_sz;

/**
 * @brief Initialize the shared X4 SPI bus for EPD and microSD.
 * @param[in] max_transfer_sz maximum transfer size required by the caller.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET xteink_x4_spi_bus_init(uint32_t max_transfer_sz)
{
    esp_err_t        esp_rt = ESP_OK;
    spi_bus_config_t bus_cfg;

    if (s_bus_inited) {
        if (max_transfer_sz > s_max_transfer_sz) {
            ESP_LOGW(TAG, "SPI bus already initialized, requested transfer size grows from %lu to %lu",
                     (unsigned long)s_max_transfer_sz, (unsigned long)max_transfer_sz);
        }
        return OPRT_OK;
    }

    (void)memset(&bus_cfg, 0, sizeof(bus_cfg));
    bus_cfg.mosi_io_num     = (int)X4_EPD_PIN_MOSI;
    bus_cfg.miso_io_num     = (int)X4_SD_PIN_MISO;
    bus_cfg.sclk_io_num     = (int)X4_EPD_PIN_SCLK;
    bus_cfg.quadwp_io_num   = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num   = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = (int)max_transfer_sz;

    esp_rt = spi_bus_initialize(X4_IDF_SPI_HOST, &bus_cfg, X4_IDF_SPI_DMA);
    if (esp_rt != ESP_OK && esp_rt != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(esp_rt));
        return OPRT_COM_ERROR;
    }

    s_bus_inited      = TRUE;
    s_max_transfer_sz = max_transfer_sz;
    return OPRT_OK;
}
