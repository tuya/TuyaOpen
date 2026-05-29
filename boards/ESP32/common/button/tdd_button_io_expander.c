/**
 * @file tdd_button_io_expander.c
 * @brief TDD button driver for buttons connected via IO expander.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tdl_button_manage.h"
#include "tdd_button_io_expander.h"

#include "tal_memory.h"
#include "tal_log.h"

typedef struct {
    TDD_BUTTON_IO_EXP_CFG_T cfg;
} TDD_BTN_IO_EXP_HANDLE_T;

static OPERATE_RET __tdd_create_io_exp_button(TDL_BUTTON_OPRT_INFO *dev)
{
    if (NULL == dev || NULL == dev->dev_handle) {
        return OPRT_INVALID_PARM;
    }

    TDD_BTN_IO_EXP_HANDLE_T *hdl = (TDD_BTN_IO_EXP_HANDLE_T *)dev->dev_handle;

    if (NULL == hdl->cfg.get_level || NULL == hdl->cfg.set_dir) {
        PR_ERR("io_exp button: get_level/set_dir must not be NULL");
        return OPRT_INVALID_PARM;
    }

    if (hdl->cfg.init) {
        if (hdl->cfg.init() != 0) {
            PR_ERR("io_exp button: expander init failed");
            return OPRT_COM_ERROR;
        }
    }

    /* configure pin as input */
    if (hdl->cfg.set_dir(hdl->cfg.pin_mask, 1) != 0) {
        PR_ERR("io_exp button: set_dir failed");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_delete_io_exp_button(TDL_BUTTON_OPRT_INFO *dev)
{
    if (NULL == dev || NULL == dev->dev_handle) {
        return OPRT_INVALID_PARM;
    }

    tal_free(dev->dev_handle);
    return OPRT_OK;
}

static OPERATE_RET __tdd_read_io_exp_value(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value)
{
    if (NULL == dev || NULL == dev->dev_handle || NULL == value) {
        return OPRT_INVALID_PARM;
    }

    TDD_BTN_IO_EXP_HANDLE_T *hdl = (TDD_BTN_IO_EXP_HANDLE_T *)dev->dev_handle;

    uint32_t level = 0;
    if (hdl->cfg.get_level(hdl->cfg.pin_mask, &level) != 0) {
        return OPRT_COM_ERROR;
    }

    /* non-zero means at least one masked pin is high */
    int pin_high = (level & hdl->cfg.pin_mask) != 0;

    *value = (hdl->cfg.active_level == TUYA_GPIO_LEVEL_HIGH) ? pin_high : !pin_high;
    return OPRT_OK;
}

OPERATE_RET tdd_io_expander_button_register(char *name, TDD_BUTTON_IO_EXP_CFG_T *cfg)
{
    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    TDD_BTN_IO_EXP_HANDLE_T *hdl = (TDD_BTN_IO_EXP_HANDLE_T *)tal_malloc(sizeof(TDD_BTN_IO_EXP_HANDLE_T));
    if (NULL == hdl) {
        PR_ERR("io_exp button: malloc failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(hdl, 0, sizeof(TDD_BTN_IO_EXP_HANDLE_T));
    memcpy(&hdl->cfg, cfg, sizeof(TDD_BUTTON_IO_EXP_CFG_T));

    TDL_BUTTON_CTRL_INFO ctrl_info = {
        .button_create = __tdd_create_io_exp_button,
        .button_delete = __tdd_delete_io_exp_button,
        .read_value    = __tdd_read_io_exp_value,
    };

    TDL_BUTTON_DEVICE_INFO_T dev_info = {
        .dev_handle = hdl,
        .mode       = BUTTON_TIMER_SCAN_MODE,
    };

    OPERATE_RET rt = tdl_button_register(name, &ctrl_info, &dev_info);
    if (OPRT_OK != rt) {
        tal_free(hdl);
        PR_ERR("io_exp button: tdl_button_register failed %d", rt);
        return rt;
    }

    PR_DEBUG("io_exp button '%s' registered (pin_mask=0x%08X)", name, cfg->pin_mask);
    return OPRT_OK;
}
