/**
 * @file tdd_button_adc.c
 * @brief ADC ladder button TDD, using the standard Tuya button TDL.
 */

#include <string.h>

#include "tal_log.h"
#include "tal_memory.h"
#include "tdl_button_manage.h"
#include "tdd_button_adc.h"
#include "tkl_adc.h"

#define TDD_ADC_BUTTON_RAW_MAX 1023U

typedef struct {
    BUTTON_ADC_CFG_T cfg;
    uint8_t last_status;
    uint8_t adc_initialized;
    uint8_t read_error_logged;
} TDD_ADC_BUTTON_DATA_T;

static OPERATE_RET __tdd_create_adc_button(TDL_BUTTON_OPRT_INFO *dev)
{
    TDD_ADC_BUTTON_DATA_T *data;
    TUYA_ADC_BASE_CFG_T adc_cfg = {0};
    OPERATE_RET ret;

    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    adc_cfg.ch_list.data = (uint32_t)1U << data->cfg.adc_ch;
    adc_cfg.ch_nums = 1U;
    adc_cfg.width = 10U;
    adc_cfg.type = TUYA_ADC_EXTERNAL_SAMPLE_VOL;
    adc_cfg.mode = TUYA_ADC_CONTINUOUS;
    adc_cfg.ref_vol = 3300U;

    ret = tkl_adc_init(data->cfg.adc_num, &adc_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("ADC button init failed: adc=%u channel=%u ret=%d", (unsigned)data->cfg.adc_num,
               (unsigned)data->cfg.adc_ch, ret);
        return ret;
    }

    data->last_status = FALSE;
    data->adc_initialized = TRUE;
    PR_NOTICE("ADC button ready: adc=%u channel=%u pressed=[%u,%u]", (unsigned)data->cfg.adc_num,
              (unsigned)data->cfg.adc_ch, (unsigned)data->cfg.pressed_min, (unsigned)data->cfg.pressed_max);
    return OPRT_OK;
}

static OPERATE_RET __tdd_delete_adc_button(TDL_BUTTON_OPRT_INFO *dev)
{
    TDD_ADC_BUTTON_DATA_T *data;

    if (dev == NULL || dev->dev_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    if (data->adc_initialized != FALSE) {
        (void)tkl_adc_deinit(data->cfg.adc_num);
    }
    tal_free(data);
    return OPRT_OK;
}

static OPERATE_RET __tdd_read_adc_button(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    TDD_ADC_BUTTON_DATA_T *data;
    int32_t raw = 0;
    OPERATE_RET ret;

    if (dev == NULL || dev->dev_handle == NULL || value == NULL) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)dev->dev_handle;
    ret = tkl_adc_read_single_channel(data->cfg.adc_num, data->cfg.adc_ch, &raw);
    if (ret != OPRT_OK || raw < 0 || raw > (int32_t)TDD_ADC_BUTTON_RAW_MAX) {
        if (data->read_error_logged == FALSE) {
            PR_WARN("ADC button sample unavailable: adc=%u channel=%u ret=%d raw=%ld; retain status=%u",
                    (unsigned)data->cfg.adc_num, (unsigned)data->cfg.adc_ch, ret, (long)raw,
                    (unsigned)data->last_status);
            data->read_error_logged = TRUE;
        }
        *value = data->last_status;
        return OPRT_OK;
    }

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
    TDD_ADC_BUTTON_DATA_T *data;
    DEVICE_BUTTON_HANDLE handle;
    TDL_BUTTON_CTRL_INFO ctrl_info = {0};
    TDL_BUTTON_DEVICE_INFO_T device_info = {0};
    OPERATE_RET ret;

    if (name == NULL || name[0] == '\0' || adc_cfg == NULL || adc_cfg->adc_num != TUYA_ADC_NUM_0 ||
        adc_cfg->adc_ch >= 16U || adc_cfg->pressed_min > adc_cfg->pressed_max ||
        adc_cfg->pressed_max > TDD_ADC_BUTTON_RAW_MAX) {
        return OPRT_INVALID_PARM;
    }

    data = (TDD_ADC_BUTTON_DATA_T *)tal_malloc(sizeof(*data));
    if (data == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(data, 0, sizeof(*data));
    data->cfg = *adc_cfg;
    handle = (DEVICE_BUTTON_HANDLE)data;

    ctrl_info.button_create = __tdd_create_adc_button;
    ctrl_info.button_delete = __tdd_delete_adc_button;
    ctrl_info.read_value = __tdd_read_adc_button;
    device_info.dev_handle = handle;
    device_info.mode = BUTTON_TIMER_SCAN_MODE;

    ret = tdl_button_register(name, &ctrl_info, &device_info);
    if (ret != OPRT_OK) {
        tal_free(data);
        PR_ERR("ADC button registration failed: name=%s ret=%d", name, ret);
        return ret;
    }

    PR_NOTICE("ADC button registered: %s", name);
    return OPRT_OK;
}
