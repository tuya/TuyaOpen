/**
 * @file xteink_x4_pro_buttons.c
 * @brief Xteink X4 Pro digital button driver.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "xteink_x4_pro_buttons.h"

#include "board_config.h"
#include "tal_log.h"
#include "tkl_gpio.h"
#include <string.h>

static BOOL_T s_buttons_inited = FALSE;

/**
 * @brief Initialize one active-low button as pull-up input.
 * @param[in] pin GPIO number.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __button_init(TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_BASE_CFG_T cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.mode   = TUYA_GPIO_PULLUP;
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    return tkl_gpio_init(pin, &cfg);
}

/**
 * @brief Sample one active-low button.
 * @param[in] pin GPIO number.
 * @return true while the key is held (level LOW).
 */
static bool __button_down(TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;

    if (OPRT_OK != tkl_gpio_read(pin, &level)) {
        return false;
    }

    return (level == TUYA_GPIO_LEVEL_LOW);
}

OPERATE_RET xteink_x4_pro_buttons_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_buttons_inited) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__button_init(X4PRO_BTN_LEFT_PIN));
    TUYA_CALL_ERR_RETURN(__button_init(X4PRO_BTN_RIGHT_PIN));
    TUYA_CALL_ERR_RETURN(__button_init(X4PRO_BTN_POWER_PIN));

    s_buttons_inited = TRUE;
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_buttons_get_state(uint8_t *state)
{
    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }
    if (!s_buttons_inited) {
        return OPRT_COM_ERROR;
    }

    *state = 0;
    if (__button_down(X4PRO_BTN_LEFT_PIN)) {
        *state |= X4PRO_BTN_LEFT;
    }
    if (__button_down(X4PRO_BTN_RIGHT_PIN)) {
        *state |= X4PRO_BTN_RIGHT;
    }
    if (__button_down(X4PRO_BTN_POWER_PIN)) {
        *state |= X4PRO_BTN_POWER;
    }

    return OPRT_OK;
}
