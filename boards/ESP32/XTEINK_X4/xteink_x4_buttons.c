/**
 * @file xteink_x4_buttons.c
 * @brief Xteink X4 keypad: two ADC ladders + power GPIO (ported from OpenX4 community-sdk logic).
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "board_config.h"
#include "xteink_x4_buttons.h"
#include "tkl_adc.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include "tal_api.h"
#include <limits.h>
#include <string.h>

/* Thresholds from community-sdk InputManager (midpoints between idle and pressed averages) */
#define X4_ADC_NOISE_TOP (3900)

static int32_t s_adc1_edges[] = {X4_ADC_NOISE_TOP, 3100, 2090, 750, INT32_MIN};
static int32_t s_adc2_edges[] = {X4_ADC_NOISE_TOP, 1120, INT32_MIN};

static int32_t __btn_from_edges(int32_t raw, const int32_t *edges, int num_buttons)
{
    int i;

    if (raw >= edges[0]) {
        return -1;
    }

    for (i = 0; i < num_buttons; i++) {
        if (edges[i + 1] < raw && raw <= edges[i]) {
            return i;
        }
    }

    return -1;
}

static OPERATE_RET __read_adc_mv(TUYA_GPIO_NUM_E pin, int32_t *out_mv)
{
    OPERATE_RET rt       = OPRT_OK;
    int32_t     pin_func = 0;
    uint8_t     ch_id    = 0;

    if (NULL == out_mv) {
        return OPRT_INVALID_PARM;
    }

    pin_func = tkl_io_pin_to_func((uint32_t)pin, TUYA_IO_TYPE_ADC);
    if (pin_func < 0) {
        ch_id = (uint8_t)((uint32_t)pin & 0xFFU);
    } else {
        ch_id = (uint8_t)(pin_func & 0xFFU);
    }

    rt = tkl_adc_read_single_channel(TUYA_ADC_NUM_0, ch_id, out_mv);
    return rt;
}

OPERATE_RET board_x4_buttons_init(void)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    (void)memset(&gpio_cfg, 0, sizeof(gpio_cfg));
    gpio_cfg.mode   = TUYA_GPIO_PULLUP;
    gpio_cfg.direct = TUYA_GPIO_INPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(X4_BTN_POWER_PIN, &gpio_cfg));

    return rt;
}

OPERATE_RET board_x4_buttons_get_state(uint8_t *state)
{
    OPERATE_RET       rt   = OPRT_OK;
    int32_t           v1   = 0;
    int32_t           v2   = 0;
    int32_t           b1   = 0;
    int32_t           b2   = 0;
    uint8_t           mask = 0;
    TUYA_GPIO_LEVEL_E pwr  = TUYA_GPIO_LEVEL_HIGH;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__read_adc_mv(X4_BTN_ADC1_PIN, &v1));
    TUYA_CALL_ERR_RETURN(__read_adc_mv(X4_BTN_ADC2_PIN, &v2));

    b1 = __btn_from_edges(v1, s_adc1_edges, 4); /* Back, Confirm, Left, Right */
    b2 = __btn_from_edges(v2, s_adc2_edges, 2); /* Up, Down */

    if (b1 >= 0) {
        mask |= (uint8_t)(1U << (unsigned)b1);
    }
    if (b2 >= 0) {
        mask |= (uint8_t)(1U << (unsigned)(b2 + 4));
    }

    TUYA_CALL_ERR_RETURN(tkl_gpio_read(X4_BTN_POWER_PIN, &pwr));
    if (pwr == TUYA_GPIO_LEVEL_LOW) {
        mask |= X4_BTN_POWER;
    }

    *state = mask;
    return OPRT_OK;
}
