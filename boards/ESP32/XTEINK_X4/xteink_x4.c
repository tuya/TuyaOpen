/**
 * @file xteink_x4.c
 * @brief Xteink X4 (ESP32-C3) board_register_hardware and EPD/SD helpers.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "board_com_api.h"
#include "board_config.h"
#include "xteink_x4_epd.h"
#include "xteink_x4_sdcard.h"
#include "tkl_gpio.h"
#include "tal_api.h"
#include "tal_log.h"

#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include <stdbool.h>
#include <string.h>

OPERATE_RET board_x4_sdcard_gpio_prepare(void)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    (void)memset(&gpio_cfg, 0, sizeof(gpio_cfg));
    gpio_cfg.mode   = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(X4_SD_PIN_CS, &gpio_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_write(X4_SD_PIN_CS, TUYA_GPIO_LEVEL_HIGH));

    gpio_cfg.mode   = TUYA_GPIO_PULLUP;
    gpio_cfg.direct = TUYA_GPIO_INPUT;
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(X4_SD_PIN_MISO, &gpio_cfg));

    return rt;
}

OPERATE_RET board_x4_sdcard_mount(void)
{
    return xteink_x4_sdcard_mount();
}

OPERATE_RET board_x4_sdcard_unmount(void)
{
    return xteink_x4_sdcard_unmount();
}

bool board_x4_sdcard_ready(void)
{
    return xteink_x4_sdcard_ready();
}

OPERATE_RET board_x4_sdcard_get_usage(uint64_t *total_bytes, uint64_t *free_bytes)
{
    return xteink_x4_sdcard_get_usage(total_bytes, free_bytes);
}

OPERATE_RET board_x4_sdcard_list(const char *path, uint32_t max_files, X4_SDCARD_LIST_CB cb, void *user_data)
{
    return xteink_x4_sdcard_list(path, max_files, cb, user_data);
}

OPERATE_RET board_x4_sdcard_read_file_to_buffer(const char *path, char *buffer, size_t buffer_size, size_t max_bytes,
                                                size_t *bytes_read)
{
    return xteink_x4_sdcard_read_file_to_buffer(path, buffer, buffer_size, max_bytes, bytes_read);
}

OPERATE_RET board_x4_sdcard_write_file(const char *path, const char *content, size_t content_len)
{
    return xteink_x4_sdcard_write_file(path, content, content_len);
}

OPERATE_RET board_x4_sdcard_ensure_dir(const char *path)
{
    return xteink_x4_sdcard_ensure_dir(path);
}

bool board_x4_sdcard_exists(const char *path)
{
    return xteink_x4_sdcard_exists(path);
}

OPERATE_RET board_x4_sdcard_remove(const char *path)
{
    return xteink_x4_sdcard_remove(path);
}

OPERATE_RET board_x4_sdcard_rmdir(const char *path)
{
    return xteink_x4_sdcard_rmdir(path);
}

OPERATE_RET board_x4_sdcard_rename(const char *old_path, const char *new_path)
{
    return xteink_x4_sdcard_rename(old_path, new_path);
}

OPERATE_RET board_x4_charge_sense_get(bool *charging)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;
    TUYA_GPIO_LEVEL_E    lv = TUYA_GPIO_LEVEL_LOW;

    if (NULL == charging) {
        return OPRT_INVALID_PARM;
    }

    (void)memset(&gpio_cfg, 0, sizeof(gpio_cfg));
    gpio_cfg.mode   = TUYA_GPIO_PULLUP;
    gpio_cfg.direct = TUYA_GPIO_INPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    (void)tkl_gpio_init(X4_CHARGE_SENSE_PIN, &gpio_cfg);

    TUYA_CALL_ERR_RETURN(tkl_gpio_read(X4_CHARGE_SENSE_PIN, &lv));
    *charging = (lv == TUYA_GPIO_LEVEL_HIGH) ? TRUE : FALSE;
    return OPRT_OK;
}

OPERATE_RET board_x4_epd_init(void)
{
    return xteink_x4_epd_init();
}

OPERATE_RET board_x4_epd_clear(void)
{
    return xteink_x4_epd_clear();
}

OPERATE_RET board_x4_epd_display(uint8_t *image)
{
    return xteink_x4_epd_display(image);
}

OPERATE_RET board_x4_epd_display_full_refresh(uint8_t *image)
{
    return xteink_x4_epd_display_full_refresh(image);
}

OPERATE_RET board_x4_epd_sleep(void)
{
    return xteink_x4_epd_sleep();
}

OPERATE_RET board_x4_panel_viewable_get(uint32_t *out_x, uint32_t *out_y, uint32_t *out_w, uint32_t *out_h)
{
    if ((NULL == out_x) || (NULL == out_y) || (NULL == out_w) || (NULL == out_h)) {
        return OPRT_INVALID_PARM;
    }

    *out_x = (uint32_t)X4_PANEL_VIEWABLE_LEFT_PX;
    *out_y = (uint32_t)X4_PANEL_VIEWABLE_TOP_PX;
    *out_w = (uint32_t)X4_PANEL_VIEWABLE_WIDTH;
    *out_h = (uint32_t)X4_PANEL_VIEWABLE_HEIGHT;

    return OPRT_OK;
}

OPERATE_RET board_x4_sleep_classify_wakeup(X4_WAKEUP_CLASS_E *out_class)
{
    esp_sleep_wakeup_cause_t wc;
    esp_reset_reason_t       rr;
    bool                       usbish = false;
    OPERATE_RET                rt;

    if (NULL == out_class) {
        return OPRT_INVALID_PARM;
    }

    wc = esp_sleep_get_wakeup_cause();
    rr = esp_reset_reason();
    rt = board_x4_charge_sense_get(&usbish);
    if (OPRT_OK != rt) {
        usbish = false;
    }

    /* Port of CrossPoint Reader HalGPIO::getWakeupReason() (lib/hal/HalGPIO.cpp); USB approximated by charge sense. */
    if (((ESP_SLEEP_WAKEUP_UNDEFINED == wc) && (ESP_RST_POWERON == rr) && (false == usbish))
        || ((ESP_SLEEP_WAKEUP_GPIO == wc) && (ESP_RST_DEEPSLEEP == rr) && (true == usbish))) {
        *out_class = X4_WAKEUP_CLASS_POWER_BUTTON;
        return OPRT_OK;
    }

    if ((ESP_SLEEP_WAKEUP_UNDEFINED == wc) && (ESP_RST_UNKNOWN == rr) && (true == usbish)) {
        *out_class = X4_WAKEUP_CLASS_AFTER_FLASH;
        return OPRT_OK;
    }

    if ((ESP_SLEEP_WAKEUP_UNDEFINED == wc) && (ESP_RST_POWERON == rr) && (true == usbish)) {
        *out_class = X4_WAKEUP_CLASS_AFTER_USB_POWER;
        return OPRT_OK;
    }

    *out_class = X4_WAKEUP_CLASS_OTHER;
    return OPRT_OK;
}

/**
 * @brief Read power key as pressed (active low).
 * @param[out] out_pressed non-NULL receives TRUE if PWR is low.
 * @return OPRT_OK on success.
 */
static OPERATE_RET __power_button_is_pressed(bool *out_pressed)
{
    OPERATE_RET       rt;
    TUYA_GPIO_LEVEL_E lv = TUYA_GPIO_LEVEL_HIGH;

    if (NULL == out_pressed) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_gpio_read(X4_BTN_POWER_PIN, &lv);
    if (OPRT_OK != rt) {
        return rt;
    }

    *out_pressed = (TUYA_GPIO_LEVEL_LOW == lv) ? true : false;
    return OPRT_OK;
}

OPERATE_RET board_x4_power_verify_gpio_wake(uint32_t required_duration_ms, BOOL_T short_press_allowed)
{
    OPERATE_RET rt;
    bool        low       = false;
    bool        saw_press = false;
    uint32_t    calib_ms;
    uint32_t    need_ms;
    uint32_t    start_ms;
    uint32_t    press_start_ms = 0U;
    uint32_t    now_ms;

    if (TRUE == short_press_allowed) {
        return OPRT_OK;
    }

    calib_ms = tal_system_get_millisecond();
    if (calib_ms < required_duration_ms) {
        need_ms = required_duration_ms - calib_ms;
    } else {
        need_ms = 1U;
    }

    start_ms = tal_system_get_millisecond();
    while ((tal_system_get_millisecond() - start_ms) < 1000U) {
        rt = __power_button_is_pressed(&low);
        if (OPRT_OK != rt) {
            return rt;
        }
        if (true == low) {
            saw_press  = true;
            press_start_ms = tal_system_get_millisecond();
            break;
        }
        tal_system_sleep(10);
    }

    if (false == saw_press) {
        (void)board_x4_power_shutdown();
    }

    for (;;) {
        rt = __power_button_is_pressed(&low);
        if (OPRT_OK != rt) {
            return rt;
        }
        now_ms = tal_system_get_millisecond();
        if (false == low) {
            if ((now_ms - press_start_ms) < need_ms) {
                (void)board_x4_power_shutdown();
            }
            return OPRT_OK;
        }
        if ((now_ms - press_start_ms) >= need_ms) {
            break;
        }
        tal_system_sleep(10);
    }

    do {
        rt = __power_button_is_pressed(&low);
        if (OPRT_OK != rt) {
            return rt;
        }
        tal_system_sleep(10);
    } while (true == low);

    return OPRT_OK;
}

/**
 * @brief Wait until the dedicated PWR key is released (idle high).
 * @return OPRT_OK when released, error from tkl_gpio_read on failure.
 * @note Deep sleep is armed for wake-on-LOW (press). If we sleep while the pin is still LOW,
 *       the wake condition is already true and the chip wakes immediately (looks like restart).
 */
static OPERATE_RET __wait_power_button_released(void)
{
    OPERATE_RET       rt;
    TUYA_GPIO_LEVEL_E lv         = TUYA_GPIO_LEVEL_LOW;
    BOOL_T            did_notice = FALSE;
    uint32_t          held_ms    = 0U;

    for (;;) {
        rt = tkl_gpio_read(X4_BTN_POWER_PIN, &lv);
        if (OPRT_OK != rt) {
            return rt;
        }
        if (TUYA_GPIO_LEVEL_HIGH == lv) {
            break;
        }
        if ((!did_notice) && (held_ms >= 2000U)) {
            PR_NOTICE("X4: release PWR to finish shutdown");
            did_notice = TRUE;
        }
        held_ms += 20U;
        tal_system_sleep(20);
    }

    tal_system_sleep(80);
    return OPRT_OK;
}

OPERATE_RET board_x4_power_shutdown(void)
{
    esp_err_t   er;
    uint64_t    gpio_mask;
    unsigned    gpio_num;
    OPERATE_RET wrt;

    gpio_num  = (unsigned)X4_BTN_POWER_PIN & 0xFFU;
    gpio_mask = 1ULL << gpio_num;

    /* EPD driver deep sleep (panel registers); UI should have pushed the last frame before this. */
    (void)board_x4_epd_sleep();

    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    /* ESP32-C3 omits RTC_* esp_sleep_pd_domain_t entries when SOC_PM_SUPPORT_RTC_*_PD is off; rely on IDF defaults. */

    wrt = __wait_power_button_released();
    if (OPRT_OK != wrt) {
        PR_ERR("wait power release failed: %d", wrt);
    }

    er = esp_deep_sleep_enable_gpio_wakeup(gpio_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (ESP_OK != er) {
        PR_ERR("deep_sleep gpio wake cfg failed: %d", (int)er);
    }

    PR_NOTICE("X4: entering deep sleep (ESP32-C3 minimal power; system rail is hardware latch)");

    esp_deep_sleep_start();
    return OPRT_OK;
}

#define X4_WAKE_GATE_HOLD_MS 3000U
#define X4_WAKE_GATE_IDLE_MS 500U

/**
 * @brief After GPIO deep sleep wake, require a 3s PWR hold to continue boot; otherwise re-enter deep sleep.
 * @return none
 * @note If wakeup cause is not ESP_SLEEP_WAKEUP_GPIO, returns immediately (normal cold boot).
 */
void board_x4_deep_sleep_wake_gate(void)
{
    esp_sleep_wakeup_cause_t cause;
    OPERATE_RET              rt;
    TUYA_GPIO_LEVEL_E        lv;
    uint32_t                 low_streak_ms = 0U;
    uint32_t                 idle_hi_ms    = 0U;
    BOOL_T                   saw_press_low = FALSE;

    cause = esp_sleep_get_wakeup_cause();
    if (ESP_SLEEP_WAKEUP_GPIO != cause) {
        return;
    }

    PR_NOTICE("X4: GPIO wake from deep sleep; hold PWR >=3s to run, else back to sleep");

    for (;;) {
        rt = tkl_gpio_read(X4_BTN_POWER_PIN, &lv);
        if (OPRT_OK != rt) {
            PR_ERR("wake gate gpio read %d", rt);
            return;
        }

        if (TUYA_GPIO_LEVEL_LOW == lv) {
            saw_press_low = TRUE;
            idle_hi_ms    = 0U;
            low_streak_ms += 20U;
            if (low_streak_ms >= X4_WAKE_GATE_HOLD_MS) {
                (void)__wait_power_button_released();
                return;
            }
        } else {
            if ((TRUE == saw_press_low) && (low_streak_ms > 0U) && (low_streak_ms < X4_WAKE_GATE_HOLD_MS)) {
                PR_NOTICE("X4: PWR released before 3s -> deep sleep again");
                (void)board_x4_power_shutdown();
            }
            low_streak_ms = 0U;
            if (TRUE != saw_press_low) {
                idle_hi_ms += 20U;
                if (idle_hi_ms >= X4_WAKE_GATE_IDLE_MS) {
                    PR_NOTICE("X4: no PWR hold after GPIO wake -> deep sleep again");
                    (void)board_x4_power_shutdown();
                }
            }
        }

        tal_system_sleep(20);
    }
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("XTEINK_X4 board_register_hardware");

    TUYA_CALL_ERR_RETURN(board_x4_battery_adc_init());
    TUYA_CALL_ERR_RETURN(board_x4_buttons_init());
    TUYA_CALL_ERR_RETURN(board_x4_sdcard_gpio_prepare());

    rt = board_x4_epd_init();
    if (OPRT_OK != rt) {
        PR_ERR("X4 EPD init failed:%d", rt);
        return rt;
    }

    return OPRT_OK;
}
