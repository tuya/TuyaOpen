/**
 * @file tdd_button_adc.c
 * @brief Generic ADC / ADKEY ladder button TDD, using the standard Tuya button TDL.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tal_log.h"
#include "tal_memory.h"
#include "tdl_button_manage.h"
#include "tdd_button_adc.h"
#include "tkl_adc.h"

#define TDD_ADC_BUTTON_DEFAULT_WIDTH   10U
#define TDD_ADC_BUTTON_DEFAULT_REF_MV  3300U
#define TDD_ADC_BUTTON_DEFAULT_SAMPLES 3U
#define TDD_ADC_BUTTON_MAX_SAMPLES     7U
#define TDD_ADC_BUTTON_MIN_WIDTH       8U
#define TDD_ADC_BUTTON_MAX_WIDTH       16U

typedef struct {
    BUTTON_ADC_CFG_T cfg;
    uint32_t         raw_max;
    uint8_t          sample_cnt;
    uint8_t          last_status;
    uint8_t          adc_initialized;
    uint8_t          read_error_logged;
} TDD_ADC_BUTTON_DATA_T;

typedef struct {
    uint32_t ch_mask;
    uint16_t ref_cnt;
    uint8_t  width;
    uint16_t ref_vol;
} TDD_ADC_PORT_STATE_T;

static TDD_ADC_PORT_STATE_T s_adc_ports[TUYA_ADC_NUM_MAX];

static uint8_t __tdd_resolve_width(uint8_t width)
{
    return (width == 0U) ? TDD_ADC_BUTTON_DEFAULT_WIDTH : width;
}

static uint16_t __tdd_resolve_ref_vol(uint16_t ref_vol)
{
    return (ref_vol == 0U) ? TDD_ADC_BUTTON_DEFAULT_REF_MV : ref_vol;
}

static uint8_t __tdd_resolve_sample_cnt(uint8_t sample_cnt)
{
    if (sample_cnt == 0U) {
        return TDD_ADC_BUTTON_DEFAULT_SAMPLES;
    }
    return (sample_cnt > TDD_ADC_BUTTON_MAX_SAMPLES) ? TDD_ADC_BUTTON_MAX_SAMPLES : sample_cnt;
}

static uint32_t __tdd_raw_max_for_width(uint8_t width)
{
    return ((uint32_t)1U << width) - 1U;
}

static int32_t __tdd_median_sample(int32_t *samples, uint8_t count)
{
    uint8_t i;

    for (i = 1U; i < count; i++) {
        int32_t key = samples[i];
        int32_t j   = (int32_t)i - 1;

        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    return samples[count / 2U];
}

static OPERATE_RET __tdd_create_adc_button(TDL_BUTTON_OPRT_INFO *dev)
{
    TDD_ADC_BUTTON_DATA_T *data;
    TDD_ADC_PORT_STATE_T  *port;
    TUYA_ADC_BASE_CFG_T    adc_cfg = {0};
    uint8_t                width;
    uint16_t               ref_vol;
    OPERATE_RET            ret;

    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    if (data->cfg.adc_num >= TUYA_ADC_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    port    = &s_adc_ports[data->cfg.adc_num];
    width   = __tdd_resolve_width(data->cfg.adc_width);
    ref_vol = __tdd_resolve_ref_vol(data->cfg.ref_vol);

    adc_cfg.ch_list.data = port->ch_mask | ((uint32_t)1U << data->cfg.adc_ch);
    adc_cfg.ch_nums      = 1U;
    adc_cfg.width        = (port->ref_cnt > 0U) ? port->width : width;
    adc_cfg.type         = TUYA_ADC_EXTERNAL_SAMPLE_VOL;
    adc_cfg.mode         = TUYA_ADC_CONTINUOUS;
    adc_cfg.ref_vol      = (port->ref_cnt > 0U) ? port->ref_vol : ref_vol;

    ret = tkl_adc_init(data->cfg.adc_num, &adc_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("ADC button init failed: adc=%u channel=%u ret=%d", (unsigned)data->cfg.adc_num,
               (unsigned)data->cfg.adc_ch, ret);
        return ret;
    }

    if (port->ref_cnt == 0U) {
        port->width   = width;
        port->ref_vol = ref_vol;
    }
    port->ch_mask |= (uint32_t)1U << data->cfg.adc_ch;
    port->ref_cnt++;

    data->last_status     = FALSE;
    data->adc_initialized = TRUE;
    PR_NOTICE("ADC button ready: adc=%u channel=%u pressed=[%u,%u] width=%u samples=%u", (unsigned)data->cfg.adc_num,
              (unsigned)data->cfg.adc_ch, (unsigned)data->cfg.pressed_min, (unsigned)data->cfg.pressed_max,
              (unsigned)width, (unsigned)data->sample_cnt);
    return OPRT_OK;
}

static OPERATE_RET __tdd_delete_adc_button(TDL_BUTTON_OPRT_INFO *dev)
{
    TDD_ADC_BUTTON_DATA_T *data;

    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    if (data->adc_initialized != FALSE && data->cfg.adc_num < TUYA_ADC_NUM_MAX) {
        TDD_ADC_PORT_STATE_T *port = &s_adc_ports[data->cfg.adc_num];
        if (port->ref_cnt > 0U) {
            port->ref_cnt--;
            if (port->ref_cnt == 0U) {
                port->ch_mask = 0U;
            }
        }
        (void)tkl_adc_deinit(data->cfg.adc_num);
    }
    tal_free(data);
    return OPRT_OK;
}

static OPERATE_RET __tdd_read_adc_button(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    TDD_ADC_BUTTON_DATA_T *data;
    int32_t                samples[TDD_ADC_BUTTON_MAX_SAMPLES] = {0};
    uint8_t                valid                               = 0U;
    uint8_t                i;
    int32_t                raw = 0;

    if (dev == NULL || dev->dev_handle == NULL || value == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    for (i = 0U; i < data->sample_cnt; i++) {
        int32_t     sample = 0;
        OPERATE_RET ret    = tkl_adc_read_single_channel(data->cfg.adc_num, data->cfg.adc_ch, &sample);

        if (ret == OPRT_OK && sample >= 0 && (uint32_t)sample <= data->raw_max) {
            samples[valid++] = sample;
        } else {
            raw = sample;
        }
    }

    if (valid == 0U) {
        if (data->read_error_logged == FALSE) {
            PR_WARN("ADC button sample unavailable: adc=%u channel=%u raw=%ld; retain status=%u",
                    (unsigned)data->cfg.adc_num, (unsigned)data->cfg.adc_ch, (long)raw, (unsigned)data->last_status);
            data->read_error_logged = TRUE;
        }
        *value = data->last_status;
        return OPRT_OK;
    }

    raw                     = (valid == 1U) ? samples[0] : __tdd_median_sample(samples, valid);
    data->read_error_logged = FALSE;
    {
        uint8_t pressed = (raw >= (int32_t)data->cfg.pressed_min && raw <= (int32_t)data->cfg.pressed_max);
        if (pressed != data->last_status) {
            PR_NOTICE("ADC button edge: adc=%u channel=%u raw=%ld pressed=%u", (unsigned)data->cfg.adc_num,
                      (unsigned)data->cfg.adc_ch, (long)raw, (unsigned)pressed);
        }
        data->last_status = pressed;
    }
    *value = data->last_status;
    return OPRT_OK;
}

OPERATE_RET tdd_adc_button_register(char *name, BUTTON_ADC_CFG_T *adc_cfg)
{
    TDD_ADC_BUTTON_DATA_T   *data;
    DEVICE_BUTTON_HANDLE     handle;
    TDL_BUTTON_CTRL_INFO     ctrl_info   = {0};
    TDL_BUTTON_DEVICE_INFO_T device_info = {0};
    uint8_t                  width;
    uint32_t                 raw_max;
    OPERATE_RET              ret;

    if (name == NULL || name[0] == '\0' || adc_cfg == NULL || adc_cfg->adc_num >= TUYA_ADC_NUM_MAX ||
        adc_cfg->adc_ch >= 32U) {
        return OPRT_INVALID_PARM;
    }

    width = __tdd_resolve_width(adc_cfg->adc_width);
    if (width < TDD_ADC_BUTTON_MIN_WIDTH || width > TDD_ADC_BUTTON_MAX_WIDTH) {
        return OPRT_INVALID_PARM;
    }

    raw_max = __tdd_raw_max_for_width(width);
    if (adc_cfg->pressed_min > adc_cfg->pressed_max || (uint32_t)adc_cfg->pressed_max > raw_max) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)tal_malloc(sizeof(*data));
    if (data == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(data, 0, sizeof(*data));
    data->cfg        = *adc_cfg;
    data->raw_max    = raw_max;
    data->sample_cnt = __tdd_resolve_sample_cnt(adc_cfg->sample_cnt);
    handle           = (DEVICE_BUTTON_HANDLE)data;

    ctrl_info.button_create = __tdd_create_adc_button;
    ctrl_info.button_delete = __tdd_delete_adc_button;
    ctrl_info.read_value    = __tdd_read_adc_button;
    device_info.dev_handle  = handle;
    device_info.mode        = BUTTON_TIMER_SCAN_MODE;

    ret = tdl_button_register(name, &ctrl_info, &device_info);
    if (ret != OPRT_OK) {
        tal_free(data);
        PR_ERR("ADC button registration failed: name=%s ret=%d", name, ret);
        return ret;
    }

    PR_NOTICE("ADC button registered: %s", name);
    return OPRT_OK;
}

OPERATE_RET tdd_adc_button_register_keys(TUYA_ADC_NUM_E adc_num, uint8_t adc_ch, uint8_t adc_width, uint16_t ref_vol,
                                         uint8_t sample_cnt, const BUTTON_ADC_KEY_T *keys, uint8_t key_count)
{
    uint8_t i;

    if (keys == NULL || key_count == 0U) {
        return OPRT_INVALID_PARM;
    }

    for (i = 0U; i < key_count; i++) {
        BUTTON_ADC_CFG_T cfg = {
            .adc_num     = adc_num,
            .adc_ch      = adc_ch,
            .pressed_min = keys[i].pressed_min,
            .pressed_max = keys[i].pressed_max,
            .adc_width   = adc_width,
            .ref_vol     = ref_vol,
            .sample_cnt  = sample_cnt,
        };
        OPERATE_RET ret = tdd_adc_button_register((char *)keys[i].name, &cfg);
        if (ret != OPRT_OK) {
            return ret;
        }
    }

    return OPRT_OK;
}
