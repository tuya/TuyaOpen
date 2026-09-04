/**
 * @file xteink_x4_pro.c
 * @brief Xteink X4 Pro (ESP32-S3) board_register_hardware, rails and power.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note All peripheral init goes through the TuyaOpen tkl_/tal_ wrappers.
 *       The ONLY ESP-IDF calls in this file are the deep-sleep shutdown
 *       primitives (esp_sleep_*), confined to board_x4pro_power_shutdown():
 *       TuyaOpen has no deep-sleep wrapper.
 */
#include "board_com_api.h"

#include "board_config.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_gpio.h"
#include "xteink_x4_pro_battery.h"
#include "xteink_x4_pro_buttons.h"
#include "xteink_x4_pro_epd.h"
#include "xteink_x4_pro_frontlight.h"
#include "xteink_x4_pro_sdcard.h"
#include "xteink_x4_pro_touch.h"

#include "esp_err.h"
#include "esp_sleep.h"

#include <stdbool.h>
#include <string.h>

#define TAG "x4pro_board"

/* ------------------------------------------------------------------- rails */

/**
 * @brief Initialize one output GPIO.
 * @param[in] pin GPIO number.
 * @param[in] level initial level.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __rail_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PUSH_PULL;
    cfg.direct = TUYA_GPIO_OUTPUT;
    cfg.level  = level;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Bring up the power rails in the required OEM order.
 *
 * GPIO1 is the master peripheral rail and must be driven HIGH first —
 * without it the panel rail and the SD slot stay unpowered (EPD BUSY never
 * asserts, SD reads back 0xFF). GPIO2 enables the GT911 rail (active-LOW).
 * GPIO5 enables the SD data path (active-LOW): leave it LOW until the mount
 * path pulses it, and LOW afterwards (HIGH breaks block reads with 0x107).
 *
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __rails_begin(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(__rail_init(X4PRO_RAIL_PERIPH_PIN, TUYA_GPIO_LEVEL_HIGH));
    tal_system_sleep(10);
    TUYA_CALL_ERR_RETURN(__rail_init(X4PRO_TOUCH_PIN_PWR, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(__rail_init(X4PRO_SD_PIN_PWR, TUYA_GPIO_LEVEL_LOW));

    return OPRT_OK;
}

/* --------------------------------------------------------------- panel AA */

OPERATE_RET board_x4pro_panel_viewable_get(uint32_t *out_x, uint32_t *out_y, uint32_t *out_w, uint32_t *out_h)
{
    if ((NULL == out_x) || (NULL == out_y) || (NULL == out_w) || (NULL == out_h)) {
        return OPRT_INVALID_PARM;
    }

    *out_x = (uint32_t)X4PRO_PANEL_VIEWABLE_LEFT_PX;
    *out_y = (uint32_t)X4PRO_PANEL_VIEWABLE_TOP_PX;
    *out_w = (uint32_t)X4PRO_PANEL_VIEWABLE_WIDTH;
    *out_h = (uint32_t)X4PRO_PANEL_VIEWABLE_HEIGHT;

    return OPRT_OK;
}

/* -------------------------------------------------------------------- epd */

OPERATE_RET board_x4pro_epd_init(void)
{
    return xteink_x4_pro_epd_init();
}

OPERATE_RET board_x4pro_epd_clear(void)
{
    return xteink_x4_pro_epd_clear();
}

OPERATE_RET board_x4pro_epd_display(uint8_t *image)
{
    return xteink_x4_pro_epd_display(image);
}

OPERATE_RET board_x4pro_epd_display_full_refresh(uint8_t *image)
{
    return xteink_x4_pro_epd_display_full_refresh(image);
}

OPERATE_RET board_x4pro_epd_sleep(void)
{
    return xteink_x4_pro_epd_sleep();
}

/* ------------------------------------------------------------------ touch */

OPERATE_RET board_x4pro_touch_init(void)
{
    return xteink_x4_pro_touch_init();
}

OPERATE_RET board_x4pro_touch_poll(X4PRO_TOUCH_STATE_T *state)
{
    return xteink_x4_pro_touch_poll(state);
}

/* ------------------------------------------------------------- frontlight */

OPERATE_RET board_x4pro_frontlight_init(void)
{
    return xteink_x4_pro_frontlight_init();
}

OPERATE_RET board_x4pro_frontlight_set_brightness(uint8_t percent)
{
    return xteink_x4_pro_frontlight_set_brightness(percent);
}

OPERATE_RET board_x4pro_frontlight_set_warmth(uint8_t percent)
{
    return xteink_x4_pro_frontlight_set_warmth(percent);
}

OPERATE_RET board_x4pro_frontlight_get(uint8_t *brightness, uint8_t *warmth)
{
    return xteink_x4_pro_frontlight_get(brightness, warmth);
}

/* ---------------------------------------------------------------- buttons */

OPERATE_RET board_x4pro_buttons_init(void)
{
    return xteink_x4_pro_buttons_init();
}

OPERATE_RET board_x4pro_buttons_get_state(uint8_t *state)
{
    return xteink_x4_pro_buttons_get_state(state);
}

/* ---------------------------------------------------------------- battery */

OPERATE_RET board_x4pro_battery_init(void)
{
    return xteink_x4_pro_battery_init();
}

OPERATE_RET board_x4pro_battery_read(uint32_t *voltage_mv, uint8_t *percentage)
{
    return xteink_x4_pro_battery_read(voltage_mv, percentage);
}

OPERATE_RET board_x4pro_battery_get_charge_state(X4PRO_CHARGE_STATE_E *state)
{
    return xteink_x4_pro_battery_get_charge_state(state);
}

OPERATE_RET board_x4pro_battery_on_charge_state(X4PRO_CHARGE_STATE_CB cb)
{
    return xteink_x4_pro_battery_on_charge_state(cb);
}

/* ---------------------------------------------------------------- sd card */

OPERATE_RET board_x4pro_sdcard_mount(void)
{
    return xteink_x4_pro_sdcard_mount();
}

OPERATE_RET board_x4pro_sdcard_unmount(void)
{
    return xteink_x4_pro_sdcard_unmount();
}

bool board_x4pro_sdcard_ready(void)
{
    return xteink_x4_pro_sdcard_ready();
}

OPERATE_RET board_x4pro_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes)
{
    return xteink_x4_pro_sdcard_get_usage(total_bytes, free_bytes);
}

OPERATE_RET board_x4pro_sdcard_list(const char *path, uint32_t max_files, X4PRO_SDCARD_LIST_CB cb, void *user_data)
{
    return xteink_x4_pro_sdcard_list(path, max_files, cb, user_data);
}

OPERATE_RET board_x4pro_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size,
                                                   size_t max_bytes, size_t *bytes_read)
{
    return xteink_x4_pro_sdcard_read_file_to_buffer(path, buffer, buffer_size, max_bytes, bytes_read);
}

OPERATE_RET board_x4pro_sdcard_write_file(const char *path, const char *content, size_t content_len)
{
    return xteink_x4_pro_sdcard_write_file(path, content, content_len);
}

OPERATE_RET board_x4pro_sdcard_ensure_dir(const char *path)
{
    return xteink_x4_pro_sdcard_ensure_dir(path);
}

bool board_x4pro_sdcard_exists(const char *path)
{
    return xteink_x4_pro_sdcard_exists(path);
}

OPERATE_RET board_x4pro_sdcard_remove(const char *path)
{
    return xteink_x4_pro_sdcard_remove(path);
}

OPERATE_RET board_x4pro_sdcard_rmdir(const char *path)
{
    return xteink_x4_pro_sdcard_rmdir(path);
}

OPERATE_RET board_x4pro_sdcard_rename(const char *old_path, const char *new_path)
{
    return xteink_x4_pro_sdcard_rename(old_path, new_path);
}

/* ------------------------------------------------------------------ power */

/**
 * @brief Wait until the PWR key is released (idle high).
 * @return OPRT_OK when released, error from tkl_gpio_read on failure.
 * @note Deep sleep is armed for wake-on-LOW (press). Sleeping while the pin
 *       is still LOW makes the wake condition immediately true, so the chip
 *       wakes right away (looks like a restart).
 */
static OPERATE_RET __wait_power_button_released(void)
{
    OPERATE_RET       rt;
    TUYA_GPIO_LEVEL_E lv         = TUYA_GPIO_LEVEL_LOW;
    BOOL_T            did_notice = FALSE;
    uint32_t          held_ms    = 0U;

    for (;;) {
        rt = tkl_gpio_read(X4PRO_BTN_POWER_PIN, &lv);
        if (OPRT_OK != rt) {
            return rt;
        }
        if (TUYA_GPIO_LEVEL_HIGH == lv) {
            break;
        }
        if ((!did_notice) && (held_ms >= 2000U)) {
            PR_NOTICE("X4Pro: release PWR to finish shutdown");
            did_notice = TRUE;
        }
        held_ms += 20U;
        tal_system_sleep(20);
    }

    tal_system_sleep(80);
    return OPRT_OK;
}

OPERATE_RET board_x4pro_power_shutdown(void)
{
    esp_err_t   er;
    uint64_t    gpio_mask;
    OPERATE_RET wrt;

    /* EPD deep sleep; the UI should have pushed the last frame before this. */
    (void)board_x4pro_epd_sleep();

    /* --- TAL gap: TuyaOpen has no deep-sleep wrapper, so the ESP-IDF
     *     esp_sleep APIs are used directly here and nowhere else. --- */
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    wrt = __wait_power_button_released();
    if (OPRT_OK != wrt) {
        PR_ERR("[" TAG "] wait power release failed: %d", wrt);
    }

    gpio_mask = 1ULL << ((uint64_t)X4PRO_BTN_POWER_PIN & 0xFFU);
    /* ESP32-S3 has no deep-sleep "gpio" source; the power button (GPIO3) is an
     * RTC GPIO, so use RTC ext1 like FreeInk PowerManager does on Xtensa. */
    er        = esp_sleep_enable_ext1_wakeup(gpio_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    if (ESP_OK != er) {
        PR_ERR("[" TAG "] deep_sleep gpio wake cfg failed: %d", (int)er);
    }

    PR_NOTICE("[" TAG "] entering deep sleep until PWR press");
    esp_deep_sleep_start();

    return OPRT_OK;
}

/* ------------------------------------------------------------------- init */

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("[" TAG "] hardware init: rails, buttons, EPD, touch, frontlight, battery gauge");

    TUYA_CALL_ERR_RETURN(__rails_begin());
    TUYA_CALL_ERR_RETURN(board_x4pro_buttons_init());

    rt = board_x4pro_epd_init();
    if (OPRT_OK != rt) {
        PR_ERR("[" TAG "] EPD init failed: %d", rt);
        return rt;
    }

    rt = board_x4pro_touch_init();
    if (OPRT_OK != rt) {
        PR_WARN("[" TAG "] touch init failed: %d", rt);
    }

    rt = board_x4pro_frontlight_init();
    if (OPRT_OK != rt) {
        PR_WARN("[" TAG "] frontlight init failed: %d", rt);
    }

    rt = board_x4pro_battery_init();
    if (OPRT_OK != rt) {
        PR_WARN("[" TAG "] battery gauge init failed: %d", rt);
    }

    return OPRT_OK;
}
