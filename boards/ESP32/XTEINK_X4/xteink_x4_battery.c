/**
 * @file xteink_x4_battery.c
 * @brief Xteink X4 battery sense on GPIO0 (divider), percentage from community-sdk polynomial.
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note LiPo percentage polynomial adapted from OpenX4 community-sdk BatteryMonitor (MIT).
 */
#include "tuya_cloud_types.h"
#include "board_config.h"
#include "tkl_adc.h"
#include "tkl_pinmux.h"
#include "tal_api.h"
#include <string.h>

static BOOL_T              s_adc_inited = FALSE;
static TUYA_ADC_BASE_CFG_T s_adc_cfg;
static uint32_t            s_adc_ch_mask;

/**
 * @brief Build bitmask of enabled ADC channels from configuration.
 * @param[in] cfg ADC configuration.
 * @return Bit N set if channel N is enabled.
 */
static uint32_t __adc_ch_mask_from_cfg(const TUYA_ADC_BASE_CFG_T *cfg)
{
    uint32_t m = 0U;

    if (cfg->ch_list.bits.ch_0) {
        m |= (1U << 0);
    }
    if (cfg->ch_list.bits.ch_1) {
        m |= (1U << 1);
    }
    if (cfg->ch_list.bits.ch_2) {
        m |= (1U << 2);
    }
    if (cfg->ch_list.bits.ch_3) {
        m |= (1U << 3);
    }
    if (cfg->ch_list.bits.ch_4) {
        m |= (1U << 4);
    }
    if (cfg->ch_list.bits.ch_5) {
        m |= (1U << 5);
    }
    if (cfg->ch_list.bits.ch_6) {
        m |= (1U << 6);
    }
    if (cfg->ch_list.bits.ch_7) {
        m |= (1U << 7);
    }
    if (cfg->ch_list.bits.ch_8) {
        m |= (1U << 8);
    }
    if (cfg->ch_list.bits.ch_9) {
        m |= (1U << 9);
    }
    return m;
}

/**
 * @brief Count how many enabled channels appear before ch_id in scan order.
 * @param[in] mask Enabled channel bitmask.
 * @param[in] ch_id Target channel index.
 * @return Index into tkl_adc_read_voltage() buffer for this channel.
 */
static int __adc_mv_buffer_index(uint32_t mask, uint8_t ch_id)
{
    int      idx = 0;
    uint32_t ch;

    for (ch = 0U; ch < (uint32_t)ch_id; ch++) {
        if (mask & (1U << ch)) {
            idx++;
        }
    }
    return idx;
}

/**
 * @brief Count set bits in mask up to 32 channels.
 * @param[in] mask Channel bitmask.
 * @return Popcount.
 */
static uint8_t __adc_mask_popcount(uint32_t mask)
{
    uint8_t  n = 0U;
    uint32_t m = mask;

    while (m != 0U) {
        n = (uint8_t)(n + (uint8_t)(m & 1U));
        m >>= 1U;
    }
    return n;
}

static uint8_t __percentage_from_mv(uint32_t millivolts)
{
    double volts = (double)millivolts / 1000.0;
    double y     = -144.9390 * volts * volts * volts + 1655.8629 * volts * volts - 6158.8520 * volts + 7501.3202;

    if (y < 0.0) {
        y = 0.0;
    }
    if (y > 100.0) {
        y = 100.0;
    }

    return (uint8_t)(y + 0.5);
}

static OPERATE_RET __read_pin_mv(TUYA_GPIO_NUM_E pin, int32_t *mv)
{
    OPERATE_RET rt       = OPRT_OK;
    int32_t     pin_func = 0;
    uint8_t     ch_id    = 0;
    int32_t     volt_buf[16];
    int         buf_idx  = 0;
    uint8_t     need     = 0;

    if (NULL == mv) {
        return OPRT_INVALID_PARM;
    }

    pin_func = tkl_io_pin_to_func((uint32_t)pin, TUYA_IO_TYPE_ADC);
    if (pin_func < 0) {
        ch_id = (uint8_t)((uint32_t)pin & 0xFFU);
    } else {
        ch_id = (uint8_t)(pin_func & 0xFFU);
    }

    if (0U == (s_adc_ch_mask & (1U << ch_id))) {
        return OPRT_INVALID_PARM;
    }

    buf_idx = __adc_mv_buffer_index(s_adc_ch_mask, ch_id);
    need    = __adc_mask_popcount(s_adc_ch_mask);
    if (need > (uint8_t)(sizeof(volt_buf) / sizeof(volt_buf[0]))) {
        return OPRT_COM_ERROR;
    }

    rt = tkl_adc_read_voltage(TUYA_ADC_NUM_0, volt_buf, need);
    if (OPRT_OK != rt) {
        return rt;
    }

    *mv = volt_buf[buf_idx];
    return OPRT_OK;
}

OPERATE_RET board_x4_battery_adc_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_adc_inited) {
        return OPRT_OK;
    }

    (void)memset(&s_adc_cfg, 0, sizeof(s_adc_cfg));
    s_adc_cfg.ch_list.bits.ch_0 = 1;
    s_adc_cfg.ch_list.bits.ch_1 = 1;
    s_adc_cfg.ch_list.bits.ch_2 = 1;
    s_adc_cfg.ch_nums           = 3;
    s_adc_cfg.width             = 12;
    s_adc_cfg.mode              = TUYA_ADC_SINGLE;
    s_adc_cfg.type              = TUYA_ADC_INNER_SAMPLE_VOL;
    s_adc_cfg.conv_cnt          = 1;
    s_adc_cfg.ref_vol           = 3300;

    rt = tkl_adc_init(TUYA_ADC_NUM_0, &s_adc_cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    s_adc_ch_mask = __adc_ch_mask_from_cfg(&s_adc_cfg);
    s_adc_inited  = TRUE;
    return OPRT_OK;
}

OPERATE_RET board_x4_battery_read(uint32_t *voltage_mv, uint8_t *percentage)
{
    OPERATE_RET rt       = OPRT_OK;
    int32_t     pin_mv   = 0;
    uint32_t    vbat     = 0;
    uint32_t    v_cell   = 0;
    int         sample_i = 0;
    int64_t     acc_mv   = 0;
    uint8_t     cells    = X4_BATTERY_CELL_COUNT;

    if (!s_adc_inited) {
        return OPRT_COM_ERROR;
    }

    if (cells == 0U) {
        cells = 1U;
    }

    for (sample_i = 0; sample_i < 8; sample_i++) {
        TUYA_CALL_ERR_RETURN(__read_pin_mv(X4_BATTERY_ADC_PIN, &pin_mv));
        if (pin_mv < 0) {
            return OPRT_COM_ERROR;
        }
        acc_mv += (int64_t)pin_mv;
    }
    pin_mv = (int32_t)(acc_mv / 8);

    vbat = (uint32_t)((float)pin_mv * X4_BATTERY_DIVIDER_MULT);
    v_cell = vbat / (uint32_t)cells;

    if (NULL != voltage_mv) {
        *voltage_mv = vbat;
    }
    if (NULL != percentage) {
        *percentage = __percentage_from_mv(v_cell);
    }

    return rt;
}
