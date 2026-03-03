/**
 * @file tdd_led_esp_ws1280.c
 * @brief ESP32 WS1280 LED driver implementation.
 *
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tkl_memory.h"
#include "tdd_led_esp_ws1280.h"
#include "tdl_led_driver.h"

#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "WS1280_DRIVER"

#define WS1280_COLOR_BYTES  3 // Number of bytes used by one LED color data (RGB)
#define WS1280_BITS_PER_LED (WS1280_COLOR_BYTES * 8)
#define WS1280_RESOLUTION   10000000 // 10MHz
#define WS1280_TIMEOUT_MS   100      // Transmission timeout in milliseconds
#define WS1280_T0H          4
#define WS1280_T0L          9
#define WS1280_T1H          8
#define WS1280_T1L          5
#define WS1280_RESET_US     50
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_LED_WS1280_CFG_T cfg;
    uint16_t             symbol_count;
    rmt_symbol_word_t   *symbols_buf;
} TDD_LED_WS1280_INFO_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static rmt_channel_handle_t sg_rmt_chan     = NULL;
static rmt_encoder_handle_t sg_copy_encoder = NULL; // Generic copy encoder

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Encode WS1280 timing for a single bit.
 * @param sym Output RMT symbol pointer.
 * @param bit Input bit value (0 or 1).
 * @return
 */
static void __ws1280_encode_bit(rmt_symbol_word_t *sym, uint8_t bit)
{
    if (bit) {
        // Logic 1 timing
        sym->level0    = 1;
        sym->duration0 = WS1280_T1H;
        sym->level1    = 0;
        sym->duration1 = WS1280_T1L;
    } else {
        // Logic 0 timing
        sym->level0    = 1;
        sym->duration0 = WS1280_T0H;
        sym->level1    = 0;
        sym->duration1 = WS1280_T0L;
    }
}

/**
 * @brief Encode RGB data into RMT timing symbols.
 * @param symbols_buff Output symbol buffer.
 * @param color LED color value.
 * @param led_count Number of LEDs.
 * @return
 */
static void __ws1280_encode_data(rmt_symbol_word_t *symbols_buff, uint32_t color, uint16_t led_count)
{
    uint16_t sym_idx = 0;

    if (NULL == symbols_buff || led_count == 0) {
        return;
    }

    for (int i = 0; i < led_count; i++) {
        for (int bit = 23; bit >= 0; bit--) {
            __ws1280_encode_bit(&symbols_buff[sym_idx], (color >> bit) & 0x01);
            sym_idx++;
        }
    }

    // Add reset timing (low level)
    symbols_buff[sym_idx].level0    = 0;
    symbols_buff[sym_idx].duration0 = WS1280_RESET_US * 10;
    symbols_buff[sym_idx].level1    = 0;
    symbols_buff[sym_idx].duration1 = 0;

    return;
}

/**
 * @brief Initialize the WS1280 driver.
 * @param gpio_num GPIO used for WS1280 data output.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t __ws1280_init(gpio_num_t gpio_num)
{
    esp_err_t err = ESP_OK;

    // 1. Create RMT TX channel
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num          = gpio_num,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = WS1280_RESOLUTION,
        .mem_block_symbols = 64, // Enough for 8 LEDs (24 bits each) plus reset timing
        .trans_queue_depth = 4,
        .flags.invert_out  = false, // Keep output level non-inverted
        .flags.with_dma    = false, // DMA is unnecessary for small data payloads
    };
    err = rmt_new_tx_channel(&tx_cfg, &sg_rmt_chan);
    if (err != ESP_OK) {
        PR_ERR("rmt_new_tx_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Create generic copy encoder (required because rmt_transmit encoder arg cannot be NULL)
    rmt_copy_encoder_config_t copy_encoder_cfg = {}; // No special configuration is required
    err                                        = rmt_new_copy_encoder(&copy_encoder_cfg, &sg_copy_encoder);
    if (err != ESP_OK) {
        PR_ERR("rmt_new_copy_encoder failed: %s", esp_err_to_name(err));
        rmt_del_channel(sg_rmt_chan);
        sg_rmt_chan = NULL;
        return err;
    }

    // 3. Enable RMT channel
    err = rmt_enable(sg_rmt_chan);
    if (err != ESP_OK) {
        PR_ERR("rmt_enable failed: %s", esp_err_to_name(err));
        rmt_del_encoder(sg_copy_encoder);
        sg_copy_encoder = NULL;
        rmt_del_channel(sg_rmt_chan);
        sg_rmt_chan = NULL;
        return err;
    }

    PR_NOTICE("WS1280 driver initialized (GPIO:%d)", gpio_num);

    return ESP_OK;
}

/**
 * @brief Refresh LED output by transmitting buffered symbols.
 * @param symbols Encoded symbol buffer.
 * @param sym_count Number of symbols to transmit.
 * @return ESP_OK on success, otherwise an error code.
 */
static esp_err_t __ws1280_show(rmt_symbol_word_t *symbols, uint16_t sym_count)
{
    esp_err_t err = ESP_OK;

    if (NULL == symbols || 0 == sym_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sg_rmt_chan == NULL || sg_copy_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count      = 0, // Transmit once
        .flags.eot_level = 0, // Keep low level after transmission ends
    };

    // New transmission API: handles timing and encoding flow
    err = rmt_transmit(sg_rmt_chan, sg_copy_encoder, symbols, sym_count * sizeof(rmt_symbol_word_t), &tx_cfg);
    if (err != ESP_OK) {
        PR_ERR("rmt_transmit failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_tx_wait_all_done(sg_rmt_chan, WS1280_TIMEOUT_MS);
    if (err != ESP_OK) {
        PR_ERR("rmt_tx_wait_all_done timeout/error: %s", esp_err_to_name(err));
        return err;
    }

    PR_NOTICE("rmt send %d symbols", sym_count);

    return ESP_OK;
}

/**
 * @brief Deinitialize the RMT channel resources.
 * @param
 * @param
 * @return
 */
static void __ws1280_deinit(void)
{
    if (sg_rmt_chan) {
        rmt_disable(sg_rmt_chan);
    }

    if (sg_copy_encoder) {
        rmt_del_encoder(sg_copy_encoder);
        sg_copy_encoder = NULL;
    }

    if (sg_rmt_chan) {
        rmt_del_channel(sg_rmt_chan);
        sg_rmt_chan = NULL;
    }
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

    TDD_LED_WS1280_INFO_T *led_info = (TDD_LED_WS1280_INFO_T *)dev;

    esp_err_t err = __ws1280_init(led_info->cfg.gpio);
    if (err != ESP_OK) {
        return OPRT_COM_ERROR;
    }

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

    TDD_LED_WS1280_INFO_T *led_info = (TDD_LED_WS1280_INFO_T *)dev;

    if (is_on) {
        __ws1280_encode_data(led_info->symbols_buf, led_info->cfg.color, led_info->cfg.led_count);
    } else {
        __ws1280_encode_data(led_info->symbols_buf, 0, led_info->cfg.led_count);
    }

    esp_err_t err = __ws1280_show(led_info->symbols_buf, led_info->symbol_count);
    if (err != ESP_OK) {
        PR_ERR("Failed to transmit WS1280 data: %s", esp_err_to_name(err));
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

    __ws1280_deinit();

    return OPRT_OK;
}

/**
 * @brief Register a WS1280 LED device.
 * @param dev_name Device name.
 * @param cfg WS1280 configuration.
 * @return Operation result code.
 */
OPERATE_RET tdd_led_esp_ws1280_register(char *dev_name, TDD_LED_WS1280_CFG_T *cfg)
{
    TDD_LED_WS1280_INFO_T *led_info         = NULL;
    uint16_t               symbol_count     = 0;
    uint32_t               symbols_buf_size = 0;

    if (dev_name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (cfg->led_count > TDD_LED_WS1280_COUNT_MAX) {
        PR_ERR("LED count exceeds maximum (%d)", TDD_LED_WS1280_COUNT_MAX);
        return OPRT_INVALID_PARM;
    }

    symbol_count     = cfg->led_count * WS1280_BITS_PER_LED + 1; // 24-bit symbols per LED + reset timing
    symbols_buf_size = symbol_count * sizeof(rmt_symbol_word_t);

    led_info = (TDD_LED_WS1280_INFO_T *)tkl_system_malloc(sizeof(TDD_LED_WS1280_INFO_T) + symbols_buf_size);
    if (led_info == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(led_info, 0, sizeof(TDD_LED_WS1280_INFO_T) + symbols_buf_size);
    memcpy(&led_info->cfg, cfg, sizeof(TDD_LED_WS1280_CFG_T));

    led_info->symbols_buf  = (rmt_symbol_word_t *)(led_info + 1);
    led_info->symbol_count = symbol_count;

    TDD_LED_INTFS_T intfs = {
        .led_open  = __tdd_led_ws1280_open,
        .led_set   = __tdd_led_ws1280_set,
        .led_close = __tdd_led_ws1280_close,
    };

    OPERATE_RET rt = tdl_led_driver_register(dev_name, (TDD_LED_HANDLE_T)led_info, &intfs);
    if (rt != OPRT_OK) {
        tkl_system_free(led_info);
        return rt;
    }

    return OPRT_OK;
}
