/**
 * @file tdd_tp_esp.c
 * @brief Adapter that wraps an ESP-IDF esp_lcd_touch_handle_t into a TuyaOpen TDD tp driver.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_tp_esp.h"

#include "tal_memory.h"

#include "esp_lcd_touch.h"
#include "driver/gpio.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TDD_TP_ESP_MAX_POINTS CONFIG_ESP_LCD_TOUCH_MAX_POINTS

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    esp_lcd_touch_handle_t esp_tp_hdl;
} TDD_TP_ESP_DEV_T;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Open hook for the TDL tp framework.
 * @param[in] device Internal adapter context.
 * @return OPRT_OK on success.
 * @note   The underlying ESP-IDF touch handle is created externally, so this
 *         hook only validates the context.
 */
static OPERATE_RET __esp_tp_open(TDD_TP_DEV_HANDLE_T device)
{
    TDD_TP_ESP_DEV_T *dev = (TDD_TP_ESP_DEV_T *)device;

    if (NULL == dev || NULL == dev->esp_tp_hdl) {
        return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}

/**
 * @brief Read coordinates from the underlying esp_lcd_touch driver.
 * @param[in]  device     Internal adapter context.
 * @param[in]  max_num    Maximum number of touch points the caller can accept.
 * @param[out] point      Output array of touch points.
 * @param[out] point_num  Actual number of touch points returned.
 * @return OPRT_OK on success or when no touch is active, OPRT_INVALID_PARM on bad arguments.
 * @note   Many capacitive touch ICs (e.g. FT5x06) enter a low-power state when
 *         idle and NACK every I2C read until a touch event re-asserts the INT
 *         line. To avoid noisy I2C error logs from the upstream polling loop:
 *           1. If the INT pin is configured, only do the I2C read while INT is
 *              asserted (active level matches config.levels.interrupt);
 *           2. If a read still fails (transient bus noise), treat it as
 *              "no touch" instead of returning an error.
 */
static OPERATE_RET __esp_tp_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point, uint8_t *point_num)
{
    TDD_TP_ESP_DEV_T *dev = (TDD_TP_ESP_DEV_T *)device;
    uint16_t          x_buf[TDD_TP_ESP_MAX_POINTS] = {0};
    uint16_t          y_buf[TDD_TP_ESP_MAX_POINTS] = {0};
    uint8_t           read_num = 0;
    uint8_t           cap_num  = 0;

    if (NULL == dev || NULL == dev->esp_tp_hdl || NULL == point || NULL == point_num || 0 == max_num) {
        return OPRT_INVALID_PARM;
    }

    *point_num = 0;

    /* Skip I2C transaction entirely while idle (INT not asserted). */
    if (dev->esp_tp_hdl->config.int_gpio_num != GPIO_NUM_NC) {
        int active = (int)dev->esp_tp_hdl->config.levels.interrupt;
        if (gpio_get_level(dev->esp_tp_hdl->config.int_gpio_num) != active) {
            return OPRT_OK;
        }
    }

    if (ESP_OK != esp_lcd_touch_read_data(dev->esp_tp_hdl)) {
        /* Treat transient NACK while idle as "no touch" to avoid log spam. */
        return OPRT_OK;
    }

    cap_num = (max_num < TDD_TP_ESP_MAX_POINTS) ? max_num : TDD_TP_ESP_MAX_POINTS;

    if (false == esp_lcd_touch_get_coordinates(dev->esp_tp_hdl, x_buf, y_buf, NULL, &read_num, cap_num)) {
        return OPRT_OK;
    }

    if (read_num > cap_num) {
        read_num = cap_num;
    }

    for (uint8_t i = 0; i < read_num; i++) {
        point[i].x = x_buf[i];
        point[i].y = y_buf[i];
    }

    *point_num = read_num;

    return OPRT_OK;
}

/**
 * @brief Close hook for the TDL tp framework.
 * @param[in] device Internal adapter context.
 * @return OPRT_OK on success.
 * @note   Ownership of the esp_lcd_touch_handle_t stays with the caller; it is
 *         not freed here.
 */
static OPERATE_RET __esp_tp_close(TDD_TP_DEV_HANDLE_T device)
{
    TDD_TP_ESP_DEV_T *dev = (TDD_TP_ESP_DEV_T *)device;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}

/**
 * @brief Register an already-initialised ESP-IDF touch panel as a TuyaOpen TDD tp device.
 * @param[in] name        Device name used for later lookup.
 * @param[in] esp_tp_hdl  esp_lcd_touch_handle_t cast to void *.
 * @param[in] cfg         TP configuration.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_tp_esp_register(char *name, void *esp_tp_hdl, TDD_TP_ESP_CFG_T *cfg)
{
    OPERATE_RET       rt  = OPRT_OK;
    TDD_TP_ESP_DEV_T *dev = NULL;
    TDD_TP_INTFS_T    intfs;

    if (NULL == name || NULL == esp_tp_hdl || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    dev = (TDD_TP_ESP_DEV_T *)tal_malloc(sizeof(TDD_TP_ESP_DEV_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(TDD_TP_ESP_DEV_T));
    dev->esp_tp_hdl = (esp_lcd_touch_handle_t)esp_tp_hdl;

    memset(&intfs, 0, sizeof(TDD_TP_INTFS_T));
    intfs.open  = __esp_tp_open;
    intfs.read  = __esp_tp_read;
    intfs.close = __esp_tp_close;

    rt = tdl_tp_device_register(name, (TDD_TP_DEV_HANDLE_T)dev, &cfg->tp_cfg, &intfs);
    if (OPRT_OK != rt) {
        tal_free(dev);
    }

    return rt;
}
